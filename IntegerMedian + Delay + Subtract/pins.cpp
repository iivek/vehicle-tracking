#ifndef PINS_H
	#include "pins.h"
	#define PINS_H
#endif

#ifndef ALLOCATOR_H
	#include "allocator.h"
	#define ALLOCATOR_H
#endif

CPinOutputBackground :: CPinOutputBackground(CBaseMultiIOFilter* pFilter,
		CCritSec* pLock,
		HRESULT* phr,
		LPCWSTR pName) :
	CBaseMultiIOOutputPin(pFilter, pLock, phr, pName, 0),
	m_pFilter((CMovDetectorFilter*) pFilter),
	m_pIPPAllocator(NULL){	}

CPinOutputBackground :: ~CPinOutputBackground()	{
	if(m_pIPPAllocator)
	{
		//we have been fully released, no need to delete
		m_pIPPAllocator = NULL;
	}
}


// When the input pin doesn't provide a valid allocator we have to come with propositions of our own
HRESULT CPinOutputBackground :: InitAllocator(IMemAllocator **ppAlloc)	{

	DbgLog((LOG_TRACE,0,TEXT("InitAllocator")));

	CAutoLock lock(&m_pFilter->m_csReceive);

	// hr has to be initialized to NOERROR because it is set in CBaseAllocator::CBaseAllocator
	// only if an error occurs
	HRESULT hr = NOERROR;

	DbgLog((LOG_TRACE,0,TEXT("CPinOutputBackground::InitAllocator: %d %d"),
				m_pFilter->bytesPerPixel_8u_C1,
				m_pFilter->stride_8u_C1 ));

	m_pIPPAllocator = new CIPPAllocatorBackground_8u_C1(
		m_pFilter,
		&m_pFilter->m_csReceive,		//sync with receive
		this,
		NAME("IPPAllocator"),
		NULL,
		&hr);

	// set the allocator's parameters
	m_pIPPAllocator->m_lAlignment = m_pFilter->outputAllocProps.cbAlign;	
	m_pIPPAllocator->m_lSize = m_pFilter->outputAllocProps.cbBuffer;
	m_pIPPAllocator->m_lCount = m_pFilter->outputAllocProps.cBuffers;
	m_pIPPAllocator->m_lPrefix = m_pFilter->outputAllocProps.cbPrefix;

	DbgLog((LOG_TRACE,0,TEXT("CPinOutputBackground::InitAllocator: %d %d %d %d"),
				m_pIPPAllocator->m_lSize,m_pIPPAllocator->m_lCount,
				m_pIPPAllocator->m_lAlignment,m_pIPPAllocator->m_lPrefix ));

	if (!m_pIPPAllocator)
	{
		return E_OUTOFMEMORY;
	}
	if (FAILED(hr))
    {
        delete m_pIPPAllocator;
        return hr;
    }

	// Return the IMemAllocator interface.
	return m_pIPPAllocator->QueryInterface( IID_IMemAllocator, (void**)ppAlloc );
}


HRESULT CPinOutputBackground::InitializeAllocatorRequirements()
{
	DbgLog((LOG_TRACE,0,TEXT("InitializeAllocatorRequirements")));

	CAutoLock lock(&m_pFilter->m_csMultiIO);

	if( m_pFilter->pInputPin->IsConnected() == FALSE )
	{
        return E_UNEXPECTED;
    }

    HRESULT hr = NOERROR;

	VIDEOINFOHEADER* pVih = NULL;
	pVih = (VIDEOINFOHEADER*) m_pFilter->pInputPin->CurrentMediaType().Format();

	// at this timepoint it's convenient to calculate strides and what our allocators might need
	m_pFilter->InitializeProcessParams(pVih);

	m_pFilter->outputAllocProps.cbBuffer = m_pFilter->frameSize.height * m_pFilter->step_8u_C1;	
	m_pFilter->outputAllocProps.cBuffers = 1;
	m_pFilter->outputAllocProps.cbAlign = 32;
	m_pFilter->outputAllocProps.cbPrefix = 0;

	// if upstream filter doesn't ask us for our allocator requirements, inputAllocProps
	// won't get set up (inftee doesn't do that, for example)	

	return NOERROR;
}

// our output pin will send notifications the input pin
// TODO: at the moment it passes notifications to only one pin
HRESULT CPinOutputBackground::Notify(IBaseFilter * pSender, Quality q){

	return S_OK;
}


HRESULT CPinOutputBackground::DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc)	{
    
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


CPinOutputMovement :: CPinOutputMovement(CBaseMultiIOFilter* pFilter,
		CCritSec* pLock,
		HRESULT* phr,
		LPCWSTR pName) :
	CBaseMultiIOOutputPin(pFilter, pLock, phr, pName, 0),
	m_pFilter((CMovDetectorFilter*) pFilter),
	m_pIPPAllocator(NULL),
	m_pPosition(NULL)
{
}

CPinOutputMovement :: ~CPinOutputMovement()	{

	if(m_pIPPAllocator)	m_pIPPAllocator = NULL;	//we have been fully released, no need to delete
	if (m_pPosition) m_pPosition->Release();
}


// When the input pin doesn't provide a valid allocator we have to come with propositions of our own
HRESULT CPinOutputMovement :: InitAllocator(IMemAllocator **ppAlloc)	{

	DbgLog((LOG_TRACE,0,TEXT("InitAllocator")));

	CAutoLock lock(&m_pFilter->m_csReceive);


	// hr has to be initialized to NOERROR because it is set in CBaseAllocator::CBaseAllocator
	// only if an error occurs
	HRESULT hr = NOERROR;

	DbgLog((LOG_TRACE,0,TEXT("CPinOutputBackground::InitAllocator: %d %d"),
				m_pFilter->bytesPerPixel_8u_C1,
				m_pFilter->stride_8u_C1 ));

	m_pIPPAllocator = new CIPPAllocatorMovement_8u_C1(
		m_pFilter,
		&m_pFilter->m_csReceive,		//sync with receive
		this,
		NAME("IPPAllocator"),
		NULL,
		&hr);

	// set the allocator's parameters
	m_pIPPAllocator->m_lAlignment = m_pFilter->outputAllocProps.cbAlign;	
	m_pIPPAllocator->m_lSize = m_pFilter->outputAllocProps.cbBuffer;
	m_pIPPAllocator->m_lCount = m_pFilter->outputAllocProps.cBuffers;
	m_pIPPAllocator->m_lPrefix = m_pFilter->outputAllocProps.cbPrefix;

	DbgLog((LOG_TRACE,0,TEXT("CPinOutputBackground::InitAllocator: %d %d %d %d"),
				m_pIPPAllocator->m_lSize,m_pIPPAllocator->m_lCount,
				m_pIPPAllocator->m_lAlignment,m_pIPPAllocator->m_lPrefix ));

	if (!m_pIPPAllocator)
	{
		return E_OUTOFMEMORY;
	}
	if (FAILED(hr))
    {
        delete m_pIPPAllocator;
        return hr;
    }

	// Return the IMemAllocator interface.
	return m_pIPPAllocator->QueryInterface( IID_IMemAllocator, (void**)ppAlloc );
}


HRESULT CPinOutputMovement::InitializeAllocatorRequirements()	{

	DbgLog((LOG_TRACE,0,TEXT("InitializeAllocatorRequirements")));

	CAutoLock lock(&m_pFilter->m_csMultiIO);

	if(	m_pFilter->pInputPin->IsConnected() == FALSE )	{
        return E_UNEXPECTED;
    }

    HRESULT hr = NOERROR;

	VIDEOINFOHEADER* pVih = NULL;
	pVih = (VIDEOINFOHEADER*) m_pFilter->pInputPin->CurrentMediaType().Format();

	// at this timepoint it's convenient to calculate strides and what our allocators might need
	m_pFilter->InitializeProcessParams(pVih);

	m_pFilter->outputAllocProps.cbBuffer = m_pFilter->frameSize.height * m_pFilter->step_8u_C1;
	m_pFilter->outputAllocProps.cBuffers = 1;
	m_pFilter->outputAllocProps.cbAlign = 32;
	m_pFilter->outputAllocProps.cbPrefix = 0;

	DbgLog((LOG_TRACE,0,TEXT("MOVEMENT PIN: height = %d, step = %d"),m_pFilter->frameSize.height,m_pFilter->step_8u_C1 ));

	// if upstream filter doesn't ask us for our allocator requirements, inputAllocProps
	// won't get set up (inftee doesn't do that, for example)	

	return NOERROR;
}

// our output pin will send notifications to all input pins
// TODO: at the moment it passes notifications to only one pin
HRESULT CPinOutputMovement::Notify(IBaseFilter * pSender, Quality q){

	CAutoLock lock(&m_pFilter->m_csMultiIO);

	UNREFERENCED_PARAMETER(pSender);
    ValidateReadPtr(pSender,sizeof(IBaseFilter));
	HRESULT hr;

	// iterate through all connected input pins and notify the sink or pass the notification on
	if( m_pFilter->pInputPin->IsConnected() )	{
		if( m_pFilter->pInputPin->m_pQSink )	{
			return m_pFilter->pInputPin->m_pQSink->Notify(m_pMultiIO, q);
		}
		hr = m_pFilter->pInputPin->PassNotify(q);
		if(FAILED(hr))
			return hr;
	}

	DbgLog((LOG_TRACE,0,TEXT("Notify passed")));
	return S_OK;
}


HRESULT CPinOutputMovement::DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc)	{
    
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



HRESULT CPinInput :: Receive(IMediaSample *pInputSample)	{

	HRESULT hr;

	CAutoLock lock(&m_pFilter->m_csReceive);

	hr = CBaseInputPin::Receive(pInputSample);
	if (hr != S_OK)	return hr;
	
	CheckPointer(pInputSample,E_POINTER);
    ValidateReadPtr(pInputSample,sizeof(IMediaSample));
	ASSERT(pInputSample);
	
	// an alternative would be to wait in the flush call for the SafeToDecommit... i guess...
	if(IsFlushing())	{
		return NOERROR;
	}

	// samples
	IMediaSample *pOutBackgroundSample;
	IMediaSample *pOutMovementSample;

	Ipp8u* pMiddleOne_8u_C1;
	Ipp8u* pMedian_8u_C1;
	//buffer pointers	
	Ipp8u* pMovement_8u_C1;
	Ipp8u* pBackground_8u_C1;
	Ipp8u* pInput_8u_C1;
	pInputSample->GetPointer(&pInput_8u_C1);

	bool notPushed = true;

	// create a new media sample	
	if(m_pFilter->pBackgroundPin->IsConnected())
	{
		hr = m_pFilter->InitializeOutputSampleBackground(pInputSample, &pOutBackgroundSample);
		if(FAILED(hr)) return hr;

		//buffer pointer
		pOutBackgroundSample->GetPointer(&pBackground_8u_C1);

		// the processing
		pMedian_8u_C1 = m_pFilter->pMedian->Push(pInput_8u_C1, &pMiddleOne_8u_C1);
		notPushed = false;
		ippiCopy_8u_C1R(pMedian_8u_C1, m_pFilter->step_8u_C1, pBackground_8u_C1,
						m_pFilter->step_8u_C1, m_pFilter->frameSize);
	
		// it will get addreffed inside if needed
		hr = m_pFilter->pBackgroundPin->Deliver(pOutBackgroundSample);
	}

	if(m_pFilter->pMovementPin->IsConnected())
	{
		hr = m_pFilter->InitializeOutputSampleMovement(pInputSample, &pOutMovementSample);
		if(FAILED(hr)) return hr;

		// buffer pointer
		pOutMovementSample->GetPointer(&pMovement_8u_C1);
		
		// the processing
		if(notPushed)
			pMedian_8u_C1 = m_pFilter->pMedian->Push(pInput_8u_C1, &pMiddleOne_8u_C1);
		// subtract the background from the delayed input frame - the one at the half of median window
		// rounded towards the ceiling
		ippiAbsDiff_8u_C1R( pMiddleOne_8u_C1, m_pFilter->step_8u_C1, pMedian_8u_C1,	m_pFilter->step_8u_C1,
							pMovement_8u_C1, m_pFilter->step_8u_C1, m_pFilter->frameSize );
		ippiThreshold_LTVal_8u_C1IR(	pMovement_8u_C1,
										m_pFilter->step_8u_C1,
										m_pFilter->frameSize,
										m_pFilter->m_params.threshold,
										0);


		// it will get addreffed inside if needed
		hr = m_pFilter->pMovementPin->Deliver(pOutMovementSample);
	}

	return hr;
}


HRESULT CPinInput::EndOfStream()	{

	CAutoLock lock(&m_pFilter->m_csReceive);

	HRESULT hr;
	if(m_pFilter->pBackgroundPin->IsConnected())	{
		hr = m_pFilter->pBackgroundPin->DeliverEndOfStream();
		if(FAILED(hr))
			return hr;
	}
	if(m_pFilter->pMovementPin->IsConnected())
	{
		hr = m_pFilter->pMovementPin->DeliverEndOfStream();
		if(FAILED(hr))
			return hr;
	}

	return NOERROR;
}

HRESULT CPinInput::BeginFlush()	{

	CAutoLock lock(&m_pFilter->m_csMultiIO);

	HRESULT hr = CBaseInputPin::BeginFlush();

					DbgLog((LOG_TRACE,0,TEXT("			IsFlushing called! 5")));

	if(m_pFilter->pBackgroundPin->IsConnected() && m_pFilter->pBackgroundPin->m_pOutputQueue)
	{
		if(	!m_pFilter->pBackgroundPin->m_pOutputQueue->IsFlushing())
		{
		hr = m_pFilter->pBackgroundPin->DeliverBeginFlush();
		if(FAILED(hr))
			return hr;
		}
	}

					DbgLog((LOG_TRACE,0,TEXT("			IsFlushing called! 6")));
	if(m_pFilter->pMovementPin->IsConnected() && m_pFilter->pMovementPin->m_pOutputQueue)
	{
		if(	!m_pFilter->pMovementPin->m_pOutputQueue->IsFlushing())
		{
		hr = m_pFilter->pMovementPin->DeliverBeginFlush();
		if(FAILED(hr))
			return hr;
		}
	}

//	CAutoLock lock2(&m_pFilter->m_csReceive);
//	do sth. kewl

	return NOERROR;
}

HRESULT CPinInput::EndFlush()	{

	CAutoLock lock(&m_pFilter->m_csMultiIO);

	DbgLog((LOG_TRACE,0,TEXT("scenePin endflush")));

	HRESULT hr;


					DbgLog((LOG_TRACE,0,TEXT("			IsFlushing called! 7")));
	if(m_pFilter->pBackgroundPin->IsConnected() && m_pFilter->pBackgroundPin->m_pOutputQueue)
	{
		if(m_pFilter->pBackgroundPin->m_pOutputQueue->IsFlushing())
		{
			hr = m_pFilter->pBackgroundPin->DeliverEndFlush();
			if(FAILED(hr))
				return hr;
		}
	}

				DbgLog((LOG_TRACE,0,TEXT("			IsFlushing called! 8")));
	if(m_pFilter->pMovementPin->IsConnected() && m_pFilter->pMovementPin->m_pOutputQueue)
	{
		if(m_pFilter->pMovementPin->m_pOutputQueue->IsFlushing())
		{
			hr = m_pFilter->pMovementPin->DeliverEndFlush();
			if(FAILED(hr))
				return hr;
		}
	}

	return CBaseInputPin::EndFlush();
}

HRESULT CPinInput::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate )	{

	CAutoLock lock(&m_pFilter->m_csReceive);	
	HRESULT hr;

	if(m_pFilter->pMovementPin->IsConnected())	{
		hr = m_pFilter->pMovementPin->DeliverNewSegment(tStart, tStop, dRate);
		if(FAILED(hr))
			return hr;
	}

	if(m_pFilter->pBackgroundPin->IsConnected())	{
		hr = m_pFilter->pBackgroundPin->DeliverNewSegment(tStart, tStop, dRate);
		if(FAILED(hr))
			return hr;
	}

	return NOERROR;
}


HRESULT CPinInput::Inactive(void){

    m_bRunTimeError = FALSE;
	
    if (m_pAllocator == NULL) {
        return VFW_E_NO_ALLOCATOR;
    }
	m_bFlushing = FALSE;

	CAutoLock lock(&m_pFilter->m_csReceive);
	// do something smart

    return m_pAllocator->Decommit();
}


// This is called when we start running or go paused. We create the
// output queue object to send data to our associated peer pin
//
HRESULT CPinOutputBackground::Active()	{

    HRESULT hr = NOERROR;

    // Make sure that the pin is connected
    ASSERT(m_Connected != NULL);

    // Create the output queue if we have to
    if(m_pOutputQueue == NULL)
    {
        m_pOutputQueue = new CMyOutputQueue(m_Connected, &hr, TRUE, FALSE);
		if(m_pOutputQueue == NULL)	{
            return E_OUTOFMEMORY;
		}		

        // Make sure that the constructor did not return any error
        if(FAILED(hr))
        {
			DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOOutputPin::Active(), queue FAILED")));
            delete m_pOutputQueue;
            m_pOutputQueue = NULL;
            return hr;
        }
    }

	DbgLog((LOG_TRACE,0,TEXT("COutputPin::Active(), queue created")));

	return CBaseOutputPin::Active();
}

// Called when we stop streaming, we delete the output queue
HRESULT CPinOutputBackground::Inactive()	{

	HRESULT hr = CBaseOutputPin::Inactive();

	// section synchronized with the streaming thread
	{
		 CAutoLock lock(&m_pFilter->m_csReceive);
		// Delete the output queues associated with the pin.

		if(m_pOutputQueue)	{
			delete m_pOutputQueue;
			m_pOutputQueue = NULL;
		}
			
	}

	return hr;
}


// This is called when we start running or go paused. We create the
// output queue object to send data to our associated peer pin
//
HRESULT CPinOutputMovement::Active()	{

    HRESULT hr = NOERROR;

    // Make sure that the pin is connected
    ASSERT(m_Connected != NULL);

    // Create the output queue if we have to
    if(m_pOutputQueue == NULL)
    {
        m_pOutputQueue = new CMyOutputQueue(m_Connected, &hr, TRUE, FALSE);
		if(m_pOutputQueue == NULL)	{
            return E_OUTOFMEMORY;
		}		

        // Make sure that the constructor did not return any error
        if(FAILED(hr))
        {
			DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOOutputPin::Active(), queue FAILED")));
            delete m_pOutputQueue;
            m_pOutputQueue = NULL;
            return hr;
        }
    }

	DbgLog((LOG_TRACE,0,TEXT("COutputPin::Active(), queue created")));

	return CBaseOutputPin::Active();
}

// Called when we stop streaming, we delete the output queue
HRESULT CPinOutputMovement::Inactive()	{

	HRESULT hr = CBaseOutputPin::Inactive();

	// section synchronized with the streaming thread
	{
		 CAutoLock lock(&m_pFilter->m_csReceive);
		// Delete the output queues associated with the pin.

		if(m_pOutputQueue)	{
			delete m_pOutputQueue;
			m_pOutputQueue = NULL;
		}
			
	}

	return hr;
}

// overriden to expose IMediaPosition and IMediaSeeking control interfaces
STDMETHODIMP CPinOutputMovement::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
    CheckPointer(ppv,E_POINTER);
    ValidateReadWritePtr(ppv,sizeof(PVOID));
    *ppv = NULL;

    if (riid == IID_IMediaPosition || riid == IID_IMediaSeeking) {       
        ASSERT(m_pFilter->pInputPin != NULL);

        if (m_pPosition == NULL) {
            HRESULT hr = CreatePosPassThru(
                             GetOwner(),
                             FALSE,
							 (IPin *)m_pFilter->pInputPin,
                             &m_pPosition);
            if (FAILED(hr)) {
                return hr;
            }
        }
        return m_pPosition->QueryInterface(riid, ppv);
    } else {
        return CBaseOutputPin::NonDelegatingQueryInterface(riid, ppv);
    }
}
