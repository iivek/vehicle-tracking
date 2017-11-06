#include "Pins.h"

// When the input pin doesn't provide a valid allocator we have to come with propositions of
// our own
HRESULT CPinOutput :: InitAllocator(IMemAllocator **ppAlloc)	{

	// hr has to be initialized to NOERROR because CBaseAllocator::CBaseAllocator sets it
	// only if an error occurs
	HRESULT hr = NOERROR;

	m_pIPPAllocator = new CIPPAllocator_8u_C1(m_pExtractY, NAME("IPPAllocatorOut"), NULL, &hr);

	DbgLog((LOG_TRACE,0,TEXT("Output Allocator :: Init allocator")));
	if (!m_pIPPAllocator)
	{
		return E_OUTOFMEMORY;
	}
	if (FAILED(hr)){
		delete m_pIPPAllocator;
		m_pIPPAllocator = NULL;
		return hr;
	}
	
	// this is important to set up (because we force our allocator),
	// otherwise VFW_E_SIZENOTSET occurs from CBaseAllocator::Alloc
	CExtractY* pFilt = (CExtractY*) m_pExtractY;

	m_pIPPAllocator->m_lAlignment = pFilt->outputAllocProps.cbAlign;	
	m_pIPPAllocator->m_lSize = pFilt->outputAllocProps.cbBuffer;
	m_pIPPAllocator->m_lCount = pFilt->outputAllocProps.cBuffers;
	m_pIPPAllocator->m_lPrefix = pFilt->outputAllocProps.cbPrefix;

	// Return the IMemAllocator interface.
	DbgLog((LOG_TRACE,0,TEXT("	CPinInput::InitAllocator QueryInterface'd")));
	return m_pIPPAllocator->QueryInterface( IID_IMemAllocator, (void**)ppAlloc );
}


HRESULT CPinOutput :: DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc)	{

	DbgLog((LOG_TRACE,0,TEXT("DecideAllocator")));

	HRESULT hr;
	if(FAILED(hr = InitializeAllocatorRequirements())) return hr;

	hr = NOERROR;

	DbgLog((LOG_TRACE,0,TEXT("DecideAllocator pointers")));
	CheckPointer(pPin,E_POINTER);
    CheckPointer(ppAlloc,E_POINTER);
	DbgLog((LOG_TRACE,0,TEXT("DecideAllocator pointers OK")));

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

	DbgLog((LOG_TRACE,0,TEXT("CPinInput::GetAllocator")));

	CheckPointer(ppAllocator, E_POINTER);
	ValidateReadWritePtr(ppAllocator,sizeof(IMemAllocator *));

	DbgLog((LOG_TRACE,0,TEXT("GetAllocator pointers OK")));
    CAutoLock cObjectLock(m_pLock);
	
	if (m_pAllocator) {
	// we already have an allocator, so return that one
		DbgLog((LOG_TRACE,0,TEXT("GetAllocator: allocator already exists %p"),m_pAllocator));
		*ppAllocator = m_pAllocator;
		(*ppAllocator)->AddRef();
		return S_OK;
	}

	// we haven't got allocator yet, so we propose our own
	// hr has to be initialized to NOERROR because it is set in CBaseAllocator::CBaseAllocator
	// only if an error occurs
	HRESULT hr = NOERROR;

	m_pIPPAllocator= new CIPPAllocator_8u_AC4(m_pTransformFilter, NAME("IPPAllocatorIn"), NULL, &hr);
	if (!m_pIPPAllocator){
		DbgLog((LOG_TRACE,0,TEXT("CPinInput::GetAllocator, out of memory")));
		return E_OUTOFMEMORY;
	}
	if (FAILED(hr)){
		delete m_pIPPAllocator;
		m_pIPPAllocator = NULL;
		return hr;
	}
	// this is important to set up, otherwise VFW_E_SIZENOTSET occurs from CBaseAllocator::Alloc
	CExtractY* pFilt = (CExtractY*) m_pExtractY;

	m_pIPPAllocator->m_lAlignment = pFilt->inputAllocProps.cbAlign;	
	m_pIPPAllocator->m_lSize = pFilt->inputAllocProps.cbBuffer;
	m_pIPPAllocator->m_lCount = pFilt->inputAllocProps.cBuffers;
	m_pIPPAllocator->m_lPrefix = pFilt->inputAllocProps.cbPrefix;

	// Return and remember IMemAllocator from our allocator

	DbgLog((LOG_TRACE,0,TEXT("	CPinInput::InitAllocator QueryInterface'd")));
	return  m_pIPPAllocator->QueryInterface(IID_IMemAllocator, (void**)ppAllocator);
}


HRESULT CPinOutput::InitializeAllocatorRequirements()	{
	
	CExtractY* pFilt = (CExtractY*) m_pFilter;

	// check input pin connection
	if(pFilt->m_pInput->IsConnected() == FALSE)	{
        return E_UNEXPECTED;
    }

    HRESULT hr = NOERROR;

	pFilt->outputAllocProps.cbBuffer = pFilt->frameSize.height*pFilt->step_8u_C1;
	pFilt->outputAllocProps.cBuffers = 5;
	pFilt->outputAllocProps.cbAlign = 32;
	pFilt->outputAllocProps.cbPrefix = 0;

	DbgLog((LOG_TRACE,0,TEXT("IAR")));
	DbgLog((LOG_TRACE,0,TEXT("%d"),pFilt->outputAllocProps.cbBuffer));

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

	CExtractY* pFilt = (CExtractY*) m_pExtractY;
	CheckPointer(pProps, E_POINTER);
	ValidateReadWritePtr(pProps, sizeof(ALLOCATOR_PROPERTIES));

	pProps->cbAlign = pFilt->inputAllocProps.cbAlign = 32;
	pFilt->inputAllocProps.cbBuffer = pFilt->frameSize.height*pFilt->step_8u_AC4;
	pProps->cbPrefix = pFilt->inputAllocProps.cbPrefix = 0;
	pProps->cbPrefix = pFilt->inputAllocProps.cBuffers = 1;

	DbgLog((LOG_TRACE,0,TEXT("GetAllocatorRequirements")));

	return S_OK;
}