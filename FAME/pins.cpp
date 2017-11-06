#ifndef PINS_H
	#include "pins.h"
	#define PINS_H
#endif

#ifndef ALLOCATOR_H
	#include "allocator.h"
	#define ALLOCATOR_H
#endif


HRESULT CPinOutput::InitializeAllocatorRequirements()	{

	// check input pin connection
	if(m_pFilter->m_pInput->IsConnected() == FALSE)	{
        return E_UNEXPECTED;
    }

    HRESULT hr = NOERROR;

	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) m_pFilter->m_pInput->CurrentMediaType().Format();

	m_pFilter->outputAllocProps.cbBuffer = pVih->bmiHeader.biHeight*(pVih->bmiHeader.biWidth + m_pFilter->stride_8u_C1);	
	m_pFilter->outputAllocProps.cBuffers = 10;
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

	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) m_pFilter->m_pInput->CurrentMediaType().Format();

	CFame2D* pFilt = (CFame2D*) m_pFilter;
	LONG lAlignedWidth = pVih->bmiHeader.biWidth + pFilt->stride_8u_C1;
	m_pFilter->inputAllocProps.cbAlign = 32;
	m_pFilter->inputAllocProps.cbBuffer = lAlignedWidth * pVih->bmiHeader.biHeight;
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

	m_pIPPAllocator = new CIPPAllocatorOut_8u_C1(	m_pFilter,
													&m_pFilter->m_csReceive,
													this,
													NAME("IPPAllocatorOut"),
													NULL,
													&hr,
													m_pFilter->frameSize,
													m_pFilter->step_8u_C1,
													m_pFilter->step_32f_C1);
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
	CFame2D* pFilt = (CFame2D*) m_pFilter;

	m_pIPPAllocator->m_lAlignment = pFilt->outputAllocProps.cbAlign;	
	m_pIPPAllocator->m_lSize = pFilt->outputAllocProps.cbBuffer;
	m_pIPPAllocator->m_lCount = pFilt->outputAllocProps.cBuffers;
	m_pIPPAllocator->m_lPrefix = pFilt->outputAllocProps.cbPrefix;


	// Return the IMemAllocator interface.
	return m_pIPPAllocator->QueryInterface( IID_IMemAllocator, (void**)ppAlloc );
}



HRESULT CPinOutput::DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc)	{

    HRESULT hr = NOERROR;
    *ppAlloc = NULL;

    ALLOCATOR_PROPERTIES prop;
    ZeroMemory(&prop, sizeof(prop));

    // whatever he returns, we assume prop is either all zeros
    // or he has filled it out.
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

    /* Likewise we may not have an interface to release */

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

	// this is important to set up, otherwise VFW_E_SIZENOTSET occurs from CBaseAllocator::Alloc
	CFame2D* pFilt = (CFame2D*) m_pFilter;

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
	CAutoLock cAutolock(&m_pFilter->m_csFilter);

	// we haven't initialized it yet, so we do it here
	InitializeAllocatorRequirements();

	DbgLog((LOG_TRACE,0,TEXT("GetAllocatorRequirements")));

	CFame2D* pFilt = (CFame2D*) m_pFilter;
	CheckPointer(pProps, E_POINTER);
	ValidateReadWritePtr(pProps, sizeof(ALLOCATOR_PROPERTIES));

	pProps->cbAlign = pFilt->inputAllocProps.cbAlign;
	pProps->cbBuffer = pFilt->inputAllocProps.cbBuffer;
	pProps->cbPrefix = pFilt->inputAllocProps.cbPrefix;
	pProps->cbPrefix = pFilt->inputAllocProps.cBuffers;

	return S_OK;
}

// Overridden so we can create the output queue object to send data to our associated peer pin
HRESULT CPinOutput::Active()	{

	// no lock needed
    HRESULT hr = NOERROR;

    // Create the output queue if needed
    if(m_pOutputQueue == NULL)
    {
        m_pOutputQueue = new CMyOutputQueue(m_Connected, &hr, TRUE, FALSE);
		if(m_pOutputQueue == NULL)	{
            return E_OUTOFMEMORY;
		}		

        // Make sure that the constructor did not return any error
        if(FAILED(hr))
        {
			DbgLog((LOG_TRACE,0,TEXT("CPinOutput::Active(), queue failed")));
            delete m_pOutputQueue;
            m_pOutputQueue = NULL;
            return hr;
        }
    }

	return CBaseOutputPin::Active();
}

HRESULT CPinOutput::Inactive()	{

	//locked in the caller
	// Delete the output queues associated with the pin.
	if(m_pOutputQueue)	{
		delete m_pOutputQueue;
		m_pOutputQueue = NULL;
	}
	
	return CBaseOutputPin::Inactive();
}


HRESULT CPinOutput::Deliver(IMediaSample *pMediaSample)
{
    CheckPointer(pMediaSample,E_POINTER);

	if (m_Connected == NULL) {
        return VFW_E_NOT_CONNECTED;
    }

	// let the sample die
	if(m_pOutputQueue == NULL)
        return NOERROR;
		
   return m_pOutputQueue->Receive(pMediaSample);
}

HRESULT CPinOutput::DeliverEndOfStream()
{
	CAutoLock lock(&m_pFilter->m_csReceive);

	if (m_Connected == NULL) {
        return VFW_E_NOT_CONNECTED;
    }

    // Make sure that we have an output queue
	if(m_pOutputQueue == NULL)	{
		return m_Connected->EndOfStream();
	}

    m_pOutputQueue->EOS();
    return NOERROR;
}


HRESULT CPinOutput::DeliverBeginFlush()
{
	CAutoLock lock(&m_pFilter->m_csFilter);

	DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOOutputPin::DBF()")));

    // Make sure that we have an output queue
	if(m_pOutputQueue == NULL)	{
		// if not, try to deliver it directly
		return m_Connected->BeginFlush();
	}

	// should not be called if we're already flushing
	ASSERT(!m_pOutputQueue->IsFlushing());

	m_pOutputQueue->BeginFlush();

    return NOERROR;
}

HRESULT CPinOutput::DeliverEndFlush()
{
	CAutoLock lock(&m_pFilter->m_csFilter);
	
    // if we don't have a queue for some reason
	if(m_pOutputQueue == NULL)	{
		return m_Connected->EndFlush();
	}

	// shuould not be called unless we're not flushing
	ASSERT(m_pOutputQueue->IsFlushing());
    m_pOutputQueue->EndFlush();

    return NOERROR;
}

HRESULT CPinOutput::DeliverNewSegment(REFERENCE_TIME tStart, 
                                         REFERENCE_TIME tStop,  
                                         double dRate)
{

	CAutoLock lock(&m_pFilter->m_csReceive);

    // Make sure that we have an output queue
	if(m_pOutputQueue == NULL)	{
		return m_Connected->NewSegment(tStart, tStop, dRate);
	}

    m_pOutputQueue->NewSegment(tStart, tStop, dRate);

	return NOERROR;

}