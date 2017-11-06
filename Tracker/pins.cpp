#ifndef PINS_H
	#include "pins.h"
	#define PINS_H
#endif

#ifndef ALLOCATORS_H
	#include "allocators.h"
	#define ALLOCATORS_H
#endif

/*
HRESULT CPinOutputCustom::CheckMediaType(const CMediaType* pmtOut)	{

	// must have selected input first
    ASSERT(m_pTransformFilter->m_pInput != NULL);
    if ((m_pTransformFilter->m_pInput->IsConnected() == FALSE)) {
	        return E_INVALIDARG;
    }

    return m_pTransformFilter->CheckTransform(
				    &m_pTransformFilter->m_pInput->CurrentMediaType(),
				    pmtOut);

}
*/

HRESULT CPinOutput::InitializeAllocatorRequirements()	{

	// check input pin connection
	if(m_pFilter->m_pInput->IsConnected() == FALSE)	{
        return E_UNEXPECTED;
    }

    HRESULT hr = NOERROR;

	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) m_pFilter->m_pInput->CurrentMediaType().Format();

	m_pFilter->outputAllocProps.cbBuffer = 50;
	m_pFilter->outputAllocProps.cBuffers = 1;
	m_pFilter->outputAllocProps.cbAlign = 32;
	m_pFilter->outputAllocProps.cbPrefix = 0;
	// if upstream filter doesn't ask us for our allocator requirements, inputAllocProps
	// won't get set up (inftee doesn't do that, for example)	

	return NOERROR;
}

HRESULT CPinInput::InitializeAllocatorRequirements()	{

	// check input pin connection
	if(IsConnected() == FALSE)	{
        return E_UNEXPECTED;
    }

    HRESULT hr = NOERROR;

	CTrackerFilter* pFilt = (CTrackerFilter*) m_pFilter;
	m_pFilter->inputAllocProps.cbAlign = 32;
	m_pFilter->inputAllocProps.cbBuffer = m_pFilter->frameSize.height * m_pFilter->step_8u_C1;
	m_pFilter->inputAllocProps.cbPrefix = 0;
	m_pFilter->inputAllocProps.cBuffers = 1;

	return NOERROR;
}

// When the input pin doesn't provide a valid allocator we have to come with propositions of
// our own
HRESULT CPinOutput :: InitAllocator(IMemAllocator **ppAlloc)	{

	DbgLog((LOG_TRACE,0,TEXT("InitAllocator")));

	// hr has to be initialized to NOERROR because it is set in CBaseAllocator::CBaseAllocator
	// only if an error occurs
	HRESULT hr = NOERROR;

	if(FAILED(hr = InitializeAllocatorRequirements())) return hr;

	m_pAllocator = new CListAllocatorOut(	m_pFilter,
											&m_pFilter->m_csReceive,
											this,
											NAME("IPPAllocatorOut"),
											NULL,
											&hr
										);
	if (!m_pAllocator)
	{
		return E_OUTOFMEMORY;
	}
	if (FAILED(hr))
    {
        delete m_pAllocator;
        return hr;
    }

	// this is important to set up, otherwise VFW_E_SIZENOTSET occurs from CBaseAllocator::Alloc
	CTrackerFilter* pFilt = (CTrackerFilter*) m_pFilter;

	m_pAllocator->m_lAlignment = pFilt->outputAllocProps.cbAlign;	
	m_pAllocator->m_lSize = pFilt->outputAllocProps.cbBuffer;
	m_pAllocator->m_lCount = pFilt->outputAllocProps.cBuffers;
	m_pAllocator->m_lPrefix = pFilt->outputAllocProps.cbPrefix;

	DbgLog((LOG_TRACE,0,TEXT("Allocator 4 output pin provided")));
	DbgLog((LOG_TRACE,0,TEXT("CPinOutput::InitAllocator: %d %d %d %d"),
				m_pAllocator->m_lSize,m_pAllocator->m_lCount,
				m_pAllocator->m_lAlignment,m_pAllocator->m_lPrefix ));	

	// Return the IMemAllocator interface.
	return m_pAllocator->QueryInterface( IID_IMemAllocator, (void**)ppAlloc );
}



HRESULT CPinOutput::DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc)	{

    HRESULT hr = NOERROR;
    *ppAlloc = NULL;

    ALLOCATOR_PROPERTIES prop;
    ZeroMemory(&prop, sizeof(prop));

    pPin->GetAllocatorRequirements(&prop);

    // We don't want the allocator provided by the input pin
    // Try to satisfy the input pin's requirements with our output pin's allocator
    hr = InitAllocator(ppAlloc);
    if (SUCCEEDED(hr)) {

        // note - the properties passed here are in the same
        // structure as above and may have been modified by
        // the previous call to DecideBufferSize
        hr = DecideBufferSize(*ppAlloc, &prop);
        if (SUCCEEDED(hr)) {
            hr = pPin->NotifyAllocator(*ppAlloc, FALSE);
            if (SUCCEEDED(hr)) {
                return NOERROR;
            }
        }
    }

    // Likewise we may not have an interface to release 

    if (*ppAlloc) {
        (*ppAlloc)->Release();
        *ppAlloc = NULL;
    }
    return hr;
}




STDMETHODIMP CPinInput::GetAllocator(IMemAllocator **ppAllocator)	{


	DbgLog((LOG_TRACE,0,TEXT("GetAllocator")));

	CheckPointer(ppAllocator, E_POINTER);
	ValidateReadWritePtr(ppAllocator,sizeof(IMemAllocator *));

		//locked
	CAutoLock lock(m_pLock);
	
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

	m_pIPPAllocator = new CIPPAllocatorIn_8u_C1(	m_pFilter,
													&m_pFilter->m_csReceive,
													this,
													NAME("IPPAllocatorIn"),
													NULL,
													&hr,
													m_pFilter->frameSize,
													m_pFilter->step_8u_C1);
	if (!m_pIPPAllocator){
		return E_OUTOFMEMORY;
	}
	if (FAILED(hr)){

			DbgLog((LOG_TRACE,0,TEXT("Failed o ne :( zasto pobogu")));

		delete m_pIPPAllocator;
		return hr;
	}

	// Return  IMemAllocator from our allocator
	DbgLog((LOG_TRACE,0,TEXT("CPinInput::GetAllocator: We proposed our allocator 4 input pin")));

	return m_pIPPAllocator->QueryInterface(IID_IMemAllocator, (void**)ppAllocator);

}

// called when upstream filter's output pin starts negotiating allocators;
// our input pin requirements will be given to him.
// also, input ALLOCATOR_PROPERTIES will get initialized and stored locally in main filter class
STDMETHODIMP CPinInput::GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps)
{
	CAutoLock cAutolock(&m_pFilter->m_csFilter);

	// we haven't initialized it yet, so we do it here
	InitializeAllocatorRequirements();

	DbgLog((LOG_TRACE,0,TEXT("GetAllocatorRequirements")));

	CTrackerFilter* pFilt = (CTrackerFilter*) m_pFilter;
	CheckPointer(pProps, E_POINTER);
	ValidateReadWritePtr(pProps, sizeof(ALLOCATOR_PROPERTIES));

	pProps->cbAlign = pFilt->inputAllocProps.cbAlign;
	pProps->cbBuffer = pFilt->inputAllocProps.cbBuffer;
	pProps->cbPrefix = pFilt->inputAllocProps.cbPrefix;
	pProps->cbPrefix = pFilt->inputAllocProps.cBuffers;

	return S_OK;
}