#ifndef PINS_H
	#include "pins.h"
#endif

#ifndef ALLOCATOR_H
	#include "allocator.h"
#endif


// When the input pin doesn't provide a valid allocator we have to come with propositions of
// our own
HRESULT CPinOutput :: InitAllocator(IMemAllocator **ppAlloc)	{

	DbgLog((LOG_TRACE,0,TEXT("InitAllocator")));

	// hr has to be initialized to NOERROR because it is set in CBaseAllocator::CBaseAllocator
	// only if an error occurs
	HRESULT hr = NOERROR;

	m_pIPPAllocator = new CIPPAllocator_8u_C3(m_pFilter, NAME("IPPAllocator"), NULL, &hr);
	if (!m_pIPPAllocator)
	{
		return E_OUTOFMEMORY;
	}
	if (FAILED(hr))
    {
        delete m_pIPPAllocator;
		m_pIPPAllocator = NULL;
        return hr;
    }

	// this is important to set up, otherwise VFW_E_SIZENOTSET occurs from CBaseAllocator::Alloc
	CDupY* pFilt = (CDupY*) m_pFilter;

	m_pIPPAllocator->m_lAlignment = pFilt->outputAllocProps.cbAlign;	
	m_pIPPAllocator->m_lSize = pFilt->outputAllocProps.cbBuffer;
	m_pIPPAllocator->m_lCount = pFilt->outputAllocProps.cBuffers;
	m_pIPPAllocator->m_lPrefix = pFilt->outputAllocProps.cbPrefix;

	DbgLog((LOG_TRACE,0,TEXT("Allocator 4 output pin provided, %d"),m_pIPPAllocator->m_lSize));

	// Return the IMemAllocator interface.
	return m_pIPPAllocator->QueryInterface( IID_IMemAllocator, (void**)ppAlloc );
}


HRESULT CPinOutput :: DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc)	{
DbgLog((LOG_TRACE,0,TEXT("DecideAllocator")));

	HRESULT hr;
	if(FAILED(hr = InitializeAllocatorRequirements())) return hr;

	hr = NOERROR;

	CheckPointer(pPin,E_POINTER);
    CheckPointer(ppAlloc,E_POINTER);

    ALLOCATOR_PROPERTIES prop;
	ZeroMemory(&prop, sizeof(prop));

    // we don't care about it actually
    pPin->GetAllocatorRequirements(&prop);

    // We don't want the allocator provided by the input pin
 
	// we ask ourself what kind of allocator we'd like :)
	hr = InitAllocator(ppAlloc);
	hr = DecideBufferSize(*ppAlloc, &prop);
	if(FAILED(hr))
	{
		DbgLog((LOG_TRACE,0,TEXT("decidebuffersize failed")));
		return hr;
	}

	hr = pPin->NotifyAllocator(*ppAlloc, FALSE);
	return hr;
}


STDMETHODIMP CPinInput::GetAllocator(IMemAllocator **ppAllocator)	{

	DbgLog((LOG_TRACE,0,TEXT("GetAllocator")));

	CheckPointer(ppAllocator, E_POINTER);
	ValidateReadWritePtr(ppAllocator,sizeof(IMemAllocator *));
    CAutoLock cObjectLock(&lock);
	
	if (m_pAllocator) {
	//we already have allocator, so return that one
		 DbgLog((LOG_TRACE,0,TEXT("GetAllocator: allocator already exists %p"),m_pIPPAllocator));
		*ppAllocator = m_pAllocator;
		(*ppAllocator)->AddRef();
		return S_OK;
	}

	// we haven't got allocator yet, so we propose our own

	// hr has to be initialized to NOERROR because it is set in CBaseAllocator::CBaseAllocator
	// only if an error occurs
	HRESULT hr = NOERROR;

	m_pIPPAllocator = new CIPPAllocator_8u_C1(m_pTransformFilter, NAME("IPPAllocatorIn"),NULL,&hr);
	if (!m_pIPPAllocator){
		return E_OUTOFMEMORY;
	}
	if (FAILED(hr)){
		delete m_pIPPAllocator;
		m_pIPPAllocator = NULL;
		return hr;
	}
	// this is important to set up, otherwise VFW_E_SIZENOTSET occurs from CBaseAllocator::Alloc
	CDupY* pFilt = (CDupY*) m_pFilter;

	m_pIPPAllocator->m_lAlignment = pFilt->inputAllocProps.cbAlign;	
	m_pIPPAllocator->m_lSize = pFilt->inputAllocProps.cbBuffer;
	m_pIPPAllocator->m_lCount = pFilt->inputAllocProps.cBuffers;
	m_pIPPAllocator->m_lPrefix = pFilt->inputAllocProps.cbPrefix;

	// Return  IMemAllocator from our allocator
	DbgLog((LOG_TRACE,0,TEXT("CPinInput::GetAllocator: We proposed our allocator 4 input pin")));
	return m_pIPPAllocator->QueryInterface(IID_IMemAllocator, (void**)ppAllocator);

}

HRESULT CPinOutput::InitializeAllocatorRequirements()	{

	HRESULT hr = NOERROR;

	CDupY* pFilt = (CDupY*) m_pFilter;

	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) CurrentMediaType().Format();
	pFilt->outputAllocProps.cbBuffer = pVih->bmiHeader.biHeight*pFilt->step_8u_C3;
	pFilt->outputAllocProps.cBuffers = 1;
	pFilt->outputAllocProps.cbAlign = 32;
	pFilt->outputAllocProps.cbPrefix = 0;
	
	// if upstream filter doesn't ask us for our allocator requirements, inputAllocProps
	// won't get set up (inftee doesn't do that, for example)	

	return NOERROR;
}

// called when upstream filter's output pin starts negotiating allocators;
// our input pin requirements will be given to him.
// also, input ALLOCATOR_PROPERTIES will get initialized and stored locally in main filter class
STDMETHODIMP CPinInput::GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps)
{
	CAutoLock cAutolock(&lock);

	DbgLog((LOG_TRACE,0,TEXT("GetAllocatorRequirements")));

	CDupY* pFilt = (CDupY*) m_pFilter;
	CheckPointer(pProps, E_POINTER);
	ValidateReadWritePtr(pProps, sizeof(ALLOCATOR_PROPERTIES));

	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) pFilt->m_pInput->CurrentMediaType().Format();
	LONG lAlignedWidth = pVih->bmiHeader.biWidth*pFilt->bytesPerPixel_8u_C1 + pFilt->stride_8u_C1;

	pProps->cbAlign = pFilt->inputAllocProps.cbAlign = 32;
	pProps->cbBuffer = pFilt->inputAllocProps.cbBuffer = lAlignedWidth * pVih->bmiHeader.biHeight;
	pProps->cbPrefix = pFilt->inputAllocProps.cbPrefix = 0;
	pProps->cbPrefix = pFilt->inputAllocProps.cBuffers = 3;

	return S_OK;
}