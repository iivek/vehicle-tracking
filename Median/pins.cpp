#ifndef PINS_H
	#include "pins.h"
	#define PINS_H
#endif

#ifndef ALLOCATOR_H
	#include "allocator.h"
	#define ALLOCATOR_H
#endif

// When the input pin doesn't provide a valid allocator we have to come with propositions of
// our own
HRESULT CPinOutput :: InitAllocator(IMemAllocator **ppAlloc)	{

	DbgLog((LOG_TRACE,0,TEXT("InitAllocator")));

	// hr has to be initialized to NOERROR because it is set in CBaseAllocator::CBaseAllocator
	// only if an error occurs
	HRESULT hr = NOERROR;

	m_pIPPAllocator = new CIPPAllocator_8u_C1(m_pFilter, NAME("IPPAllocator"), NULL, &hr);
	if (!m_pIPPAllocator)
	{
		return E_OUTOFMEMORY;
	}
	if (FAILED(hr))
    {
		DbgLog((LOG_TRACE,0,TEXT("InitAllocator FAILED!!!!!!!!!!!!!!!!!!!")));

        delete m_pIPPAllocator;
        return hr;
    }

	// this is important to set up, otherwise VFW_E_SIZENOTSET occurs from CBaseAllocator::Alloc
	CMedian2D* pFilt = (CMedian2D*) m_pFilter;

	m_pIPPAllocator->m_lAlignment = pFilt->outputAllocProps.cbAlign;	
	m_pIPPAllocator->m_lSize = pFilt->outputAllocProps.cbBuffer;
	m_pIPPAllocator->m_lCount = pFilt->outputAllocProps.cBuffers;
	m_pIPPAllocator->m_lPrefix = pFilt->outputAllocProps.cbPrefix;

	DbgLog((LOG_TRACE,0,TEXT("Allocator 4 output pin provided")));

	// Return the IMemAllocator interface.
	return m_pIPPAllocator->QueryInterface( IID_IMemAllocator, (void**)ppAlloc );
}


STDMETHODIMP CPinInput::GetAllocator(IMemAllocator **ppAllocator)	{

	DbgLog((LOG_TRACE,0,TEXT("GetAllocator")));

	CheckPointer(ppAllocator, E_POINTER);
	ValidateReadWritePtr(ppAllocator,sizeof(IMemAllocator *));
    CAutoLock cObjectLock(&lock);
	
	if (m_pAllocator) {
	//we already have allocator, so return that one
		 DbgLog((LOG_TRACE,0,TEXT("GetAllocator: allocator already exists %p"),m_pAllocator));
		*ppAllocator = m_pAllocator;
		(*ppAllocator)->AddRef();

		// we avoid messing with addref and call it directly
		//return m_pAllocator->QueryInterface(IID_IMemAllocator, (void**)ppAllocator);
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

			DbgLog((LOG_TRACE,0,TEXT("Failed o ne :( zasto pobogu")));

		delete m_pIPPAllocator;
		return hr;
	}

	// this is important to set up, otherwise VFW_E_SIZENOTSET occurs from CBaseAllocator::Alloc
	CMedian2D* pFilt = (CMedian2D*) m_pFilter;

	m_pIPPAllocator->m_lAlignment = pFilt->inputAllocProps.cbAlign;	
	m_pIPPAllocator->m_lSize = pFilt->inputAllocProps.cbBuffer;
	m_pIPPAllocator->m_lCount = pFilt->inputAllocProps.cBuffers;
	m_pIPPAllocator->m_lPrefix = pFilt->inputAllocProps.cbPrefix;

	// Return  IMemAllocator from our allocator
	DbgLog((LOG_TRACE,0,TEXT("CPinInput::GetAllocator: We proposed our allocator 4 input pin")));
	return m_pIPPAllocator->QueryInterface(IID_IMemAllocator, (void**)ppAllocator);

}

// called when upstream filter's output pin starts negotiating allocators;
// our input pin requirements will be given to him.
// also, input ALLOCATOR_PROPERTIES will get initialized and stored locally in main filter class
STDMETHODIMP CPinInput::GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps)
{
	CAutoLock cAutolock(&lock);

	DbgLog((LOG_TRACE,0,TEXT("GetAllocatorRequirements")));

	CMedian2D* pFilt = (CMedian2D*) m_pFilter;
	CheckPointer(pProps, E_POINTER);
	ValidateReadWritePtr(pProps, sizeof(ALLOCATOR_PROPERTIES));

	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) pFilt->m_pInput->CurrentMediaType().Format();
	LONG lAlignedWidth = pVih->bmiHeader.biWidth*pFilt->bytesperpixel_8u_C1 + pFilt->stride_8u_C1;

	/*pProps->cbAlign = pFilt->alignment;	
	pProps->cbBuffer = lAlignedWidth * pVih->bmiHeader.biHeight;
	pProps->cBuffers = 1;
	pProps->cbPrefix = 0;
	*/
	pProps->cbAlign = pFilt->inputAllocProps.cbAlign = 32;
	pProps->cbBuffer = pFilt->inputAllocProps.cbBuffer = lAlignedWidth * pVih->bmiHeader.biHeight;
	pProps->cbPrefix = pFilt->inputAllocProps.cbPrefix = 0;
	pProps->cbPrefix = pFilt->inputAllocProps.cBuffers = 1;

	return S_OK;
}