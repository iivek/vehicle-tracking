#ifndef PINS_H
	#include "pins.h"
#endif


#ifndef PINS_H
	#include "pins.h"
#endif

CPinOutput :: CPinOutput(CBaseMultiIOFilter* pFilter,
		CCritSec* pLock,
		HRESULT* phr,
		LPCWSTR pName) :
	CBaseMultiIOOutputPin(pFilter, pLock, phr, pName, 0),
	m_pFilter((CThreshDiff*) pFilter),
	m_pIPPAllocator(NULL){	}

CPinOutput :: ~CPinOutput()	{
	if( m_pIPPAllocator ) m_pIPPAllocator->Release();
}


// When the input pin doesn't provide a valid allocator we have to come with propositions of
// our own
HRESULT CPinOutput :: InitAllocator(IMemAllocator **ppAlloc)	{

	CAutoLock lock(&m_pFilter->m_csReceive);

	// hr has to be initialized to NOERROR because it is set in CBaseAllocator::CBaseAllocator
	// only if an error occurs
	HRESULT hr = NOERROR;

	if(FAILED(hr = InitializeAllocatorRequirements())) return hr;

	m_pIPPAllocator = new CIPPAllocatorOut_8u_C1(
		m_pFilter,
		&m_pFilter->m_csReceive,		//sync with receive
		this,
		NAME("IPPAllocator"),
		NULL,
		&hr,
		m_pFilter->frameSize,
		m_pFilter->step_8u_C1);

	// set the allocator's parameters
	m_pIPPAllocator->m_lAlignment = m_pFilter->outputAllocProps.cbAlign;	
	m_pIPPAllocator->m_lSize = m_pFilter->outputAllocProps.cbBuffer;
	m_pIPPAllocator->m_lCount = m_pFilter->outputAllocProps.cBuffers;
	m_pIPPAllocator->m_lPrefix = m_pFilter->outputAllocProps.cbPrefix;

	DbgLog((LOG_TRACE,0,TEXT("CPinOutput::InitAllocator: %d %d %d %d"),
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

	// addref the allocator
	m_pIPPAllocator->AddRef();

	// Return the IMemAllocator interface.
	return m_pIPPAllocator->QueryInterface( IID_IMemAllocator, (void**)ppAlloc );
}


HRESULT CPinOutput::InitializeAllocatorRequirements()	{

	CAutoLock lock(&m_pFilter->m_csMultiIO);

	if( m_pFilter->backgroundPin->IsConnected() == FALSE &&
		m_pFilter->scenePin->IsConnected() == FALSE )	{
        return E_UNEXPECTED;
    }

    HRESULT hr = NOERROR;

	VIDEOINFOHEADER* pVih = NULL;
	if( m_pFilter->backgroundPin->IsConnected() == TRUE )	{
		pVih = (VIDEOINFOHEADER*) m_pFilter->backgroundPin->CurrentMediaType().Format();
	} else	{
		pVih = (VIDEOINFOHEADER*) m_pFilter->scenePin->CurrentMediaType().Format();
	}

	// at this timepoint it's convenient to calculate strides and what our allocators might need
	m_pFilter->InitializeProcessParams(pVih);

	m_pFilter->outputAllocProps.cbBuffer = pVih->bmiHeader.biHeight* (pVih->bmiHeader.biWidth + m_pFilter->stride_8u_C1);	

	m_pFilter->outputAllocProps.cBuffers = 2;
	m_pFilter->outputAllocProps.cbAlign = 32;
	m_pFilter->outputAllocProps.cbPrefix = 0;
	// if upstream filter doesn't ask us for our allocator requirements, inputAllocProps
	// won't get set up (inftee doesn't do that, for example)	

	return NOERROR;
}

// our output pin will send notifications to all input pins
HRESULT CPinOutput::Notify(IBaseFilter * pSender, Quality q){

	CAutoLock lock(&m_pFilter->m_csMultiIO);

	UNREFERENCED_PARAMETER(pSender);
    ValidateReadPtr(pSender,sizeof(IBaseFilter));
	HRESULT hr;

	// iterate through all connected input pins and notify the sink or pass the notification on
	if( m_pFilter->backgroundPin->IsConnected() )	{
		if( m_pFilter->backgroundPin->m_pQSink )	{
			return m_pFilter->backgroundPin->m_pQSink->Notify(m_pMultiIO, q);
		}
		hr = m_pFilter->backgroundPin->PassNotify(q);
		if(FAILED(hr))
			return hr;
	}

	if( m_pFilter->scenePin->IsConnected() )	{
		if( m_pFilter->scenePin->m_pQSink )	{
			return m_pFilter->scenePin->m_pQSink->Notify(m_pMultiIO, q);
		}
		hr = m_pFilter->scenePin->PassNotify(q);
		if(FAILED(hr))
			return hr;
	}

	return S_OK;
}


HRESULT CPinOutput::DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc)	{
    
	HRESULT hr = NOERROR;

	CheckPointer(pPin,E_POINTER);
    CheckPointer(ppAlloc,E_POINTER);

    ALLOCATOR_PROPERTIES prop;
    ZeroMemory(&prop, sizeof(prop));

    // we don't care about it actually
    pPin->GetAllocatorRequirements(&prop);

    // We don't want the allocator provided by the input pin
 
	// we ask ourself what kind of allocator we'd like :)
	hr = DecideBufferSize(*ppAlloc, &prop);
	if(FAILED(hr)) return hr;

    hr = InitAllocator(ppAlloc);
	if (SUCCEEDED(hr)) {
		hr = pPin->NotifyAllocator(*ppAlloc, FALSE);
		
	}

    return hr;
}

HRESULT CPinInputScene :: Receive(IMediaSample *pSample)	{

	//CAutoLock lock(m_pFilter->m_pLock);
	//CAutoLock lock2(&m_pFilter->m_csReceive);

	HRESULT hr;
	{
		CAutoLock lock(&m_pFilter->m_csReceive);

		hr = CBaseInputPin::Receive(pSample);
		if (hr != S_OK)	return hr;

		// we're inside receive lock, a good place to reset the event
		ResetEvent(m_pFilter->hSafeToDecommit);
		ResetEvent(m_pFilter->hHasDelivered[1]);

		DbgLog((LOG_TRACE,0,TEXT("scenePin received")));
	
		CheckPointer(pSample,E_POINTER);
	    ValidateReadPtr(pSample,sizeof(IMediaSample));
		ASSERT(pSample);
	
		// an alternative would be to wait in the flush call for the SafeToDecommit
		if(IsFlushing())	{
			DbgLog((LOG_TRACE,0,TEXT("Check it out!")));
			return NOERROR;
		}
		
		/* Check for IMediaSample2 */
		IMediaSample2 *pSample2;
		if (SUCCEEDED(pSample->QueryInterface(IID_IMediaSample2, (void **)&pSample2))) {
	        hr = pSample2->GetProperties(sizeof(m_SampleProps), (PBYTE)&m_SampleProps);
			pSample2->Release();
			if (FAILED(hr)) {
	            return hr;
			}
		} else {
	        /*  Get the properties the hard way */
			m_SampleProps.cbData = sizeof(m_SampleProps);
			m_SampleProps.dwTypeSpecificFlags = 0;
			m_SampleProps.dwStreamId = AM_STREAM_MEDIA;
			m_SampleProps.dwSampleFlags = 0;
			if (S_OK == pSample->IsDiscontinuity()) {
	            m_SampleProps.dwSampleFlags |= AM_SAMPLE_DATADISCONTINUITY;
			}
			if (S_OK == pSample->IsPreroll()) {
	            m_SampleProps.dwSampleFlags |= AM_SAMPLE_PREROLL;
			}
			if (S_OK == pSample->IsSyncPoint()) {
	            m_SampleProps.dwSampleFlags |= AM_SAMPLE_SPLICEPOINT;
			}
			if (SUCCEEDED(pSample->GetTime(&m_SampleProps.tStart,
										&m_SampleProps.tStop))) {
				m_SampleProps.dwSampleFlags |= AM_SAMPLE_TIMEVALID |
											AM_SAMPLE_STOPVALID;
			}
			if (S_OK == pSample->GetMediaType(&m_SampleProps.pMediaType)) {
	            m_SampleProps.dwSampleFlags |= AM_SAMPLE_TYPECHANGED;
			}
			pSample->GetPointer(&m_SampleProps.pbBuffer);
			m_SampleProps.lActual = pSample->GetActualDataLength();
			m_SampleProps.cbBuffer = pSample->GetSize();
		}
	
		hr = m_pFilter->pSampleCollector->AddSample(pSample, this);	
	}	// protected block end

	// set event outside the lock so other filter can do its stuff and arrive at the
	// point at which it also sets its event
	if(SUCCEEDED(hr))	{
		SetEvent(m_pFilter->hHasDelivered[1]);
		return hr;
	}	
	WaitForSingleObject(m_pFilter->hHasDelivered[0], INFINITE);

	return hr;
}




HRESULT CPinInputBackground :: Receive(IMediaSample *pSample)	{

	// so we can't get an eos ot sth while here
	//CAutoLock lock(m_pFilter->m_pLock);
	//CAutoLock lock2(&m_pFilter->m_csReceive);

	HRESULT hr;
	{	CAutoLock lock(&m_pFilter->m_csReceive);
	
		hr = CBaseInputPin::Receive(pSample);
		if (hr != S_OK)	return hr;
	
		// we're inside receive lock, a good place to reset the event
		ResetEvent(m_pFilter->hSafeToDecommit);
		ResetEvent(m_pFilter->hHasDelivered[0]);
	
		DbgLog((LOG_TRACE,0,TEXT("bkgndPin received")));
	
		// on our pause call downstream filters are inactive (ao are we), but upstream
		// aren't yet. we don't want to deliver anything when paused. same with flushing,
		// we check flushing
		if(IsFlushing())	{
			DbgLog((LOG_TRACE,0,TEXT("Check it out!")));

			return NOERROR;
		}
	
		CheckPointer(pSample,E_POINTER);
		ValidateReadPtr(pSample,sizeof(IMediaSample));
		ASSERT(pSample);
	
		/* Check for IMediaSample2 */
	    IMediaSample2 *pSample2;
	    if (SUCCEEDED(pSample->QueryInterface(IID_IMediaSample2, (void **)&pSample2))) {
			hr = pSample2->GetProperties(sizeof(m_SampleProps), (PBYTE)&m_SampleProps);
			pSample2->Release();
			if (FAILED(hr)) {
	            return hr;
			}
		} else {
	        /*  Get the properties the hard way */
			m_SampleProps.cbData = sizeof(m_SampleProps);
			m_SampleProps.dwTypeSpecificFlags = 0;
			m_SampleProps.dwStreamId = AM_STREAM_MEDIA;
			m_SampleProps.dwSampleFlags = 0;
			if (S_OK == pSample->IsDiscontinuity()) {
	            m_SampleProps.dwSampleFlags |= AM_SAMPLE_DATADISCONTINUITY;
			}
			if (S_OK == pSample->IsPreroll()) {
	            m_SampleProps.dwSampleFlags |= AM_SAMPLE_PREROLL;
			}
			if (S_OK == pSample->IsSyncPoint()) {
	            m_SampleProps.dwSampleFlags |= AM_SAMPLE_SPLICEPOINT;
			}
			if (SUCCEEDED(pSample->GetTime(&m_SampleProps.tStart,
										&m_SampleProps.tStop))) {
				m_SampleProps.dwSampleFlags |= AM_SAMPLE_TIMEVALID |
											AM_SAMPLE_STOPVALID;
			}
			if (S_OK == pSample->GetMediaType(&m_SampleProps.pMediaType)) {
	            m_SampleProps.dwSampleFlags |= AM_SAMPLE_TYPECHANGED;
			}
			pSample->GetPointer(&m_SampleProps.pbBuffer);
			m_SampleProps.lActual = pSample->GetActualDataLength();
			m_SampleProps.cbBuffer = pSample->GetSize();
		}
		
		hr = m_pFilter->pSampleCollector->AddSample(pSample, this);	
	}	//protected block over

	if(SUCCEEDED(hr))	{
		SetEvent(m_pFilter->hHasDelivered[0]);
		return hr;
	}
	else DbgLog((LOG_TRACE,0,TEXT("hahah!")));
	WaitForSingleObject(m_pFilter->hHasDelivered[1], INFINITE);

	return hr;
}

HRESULT CPinInputScene::EndOfStream()	{

	DbgLog((LOG_TRACE,0,TEXT("scenePin endofstream")));

	WaitForSingleObject(m_pFilter->hSafeToDecommit, INFINITE);
	CAutoLock lock(&m_pFilter->m_csReceive);

	HRESULT hr;
	if(m_pFilter->outputPin->IsConnected())	{
		hr = m_pFilter->outputPin->DeliverEndOfStream();
		if(FAILED(hr))	{
			return hr;
		}
	}

	return NOERROR;
}

HRESULT CPinInputScene::BeginFlush()	{

	CAutoLock lock(&m_pFilter->m_csMultiIO);

	HRESULT hr = CBaseInputPin::BeginFlush();

	if(m_pFilter->outputPin->IsConnected() && !m_pFilter->outputPin->m_pOutputQueue->IsFlushing())	{
		hr = m_pFilter->outputPin->DeliverBeginFlush();
		if(FAILED(hr))	{
			return hr;
		}
	}

	CAutoLock lock2(&m_pFilter->m_csReceive);

	// if we receive a sample from only one input pin (since the last delivery to the
	// output pin) and the upstream filter flushes, event has to be set from here;
	// otherwise we would wait for the sample from the other input pin infinitely
	// SetEvent(m_pFilter->hSafeToDecommit);

	m_pFilter->pSampleCollector->Reset();
	DbgLog((LOG_TRACE,0,TEXT("scenePin bginlush")));

	return NOERROR;
}

HRESULT CPinInputScene::EndFlush()	{

	CAutoLock lock(&m_pFilter->m_csMultiIO);

	DbgLog((LOG_TRACE,0,TEXT("scenePin endflush")));

	HRESULT hr;

	if(m_pFilter->outputPin->IsConnected())	{
		hr = m_pFilter->outputPin->DeliverEndFlush();
		if(FAILED(hr))	{
			return hr;
		}
	}

	return CBaseInputPin::EndFlush();
}

HRESULT CPinInputScene::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate )	{

	CAutoLock lock(&m_pFilter->m_csReceive);	
	HRESULT hr;

	if(m_pFilter->outputPin->IsConnected())	{
		hr = m_pFilter->outputPin->DeliverNewSegment(tStart, tStop, dRate);
		if(FAILED(hr))	{
			return hr;
		}
	}

	return NOERROR;
}


HRESULT CPinInputBackground::EndOfStream()	{

	DbgLog((LOG_TRACE,0,TEXT("bkgndPin EOS")));

	CAutoLock lock(&m_pFilter->m_csReceive);

	return NOERROR;
}

HRESULT CPinInputBackground::BeginFlush()	{

	CAutoLock lock(&m_pFilter->m_csMultiIO);

	HRESULT hr = CBaseInputPin::BeginFlush();

	DbgLog((LOG_TRACE,0,TEXT("bkgndPin bginlush waiting12312")));

	// we have to deliverbeginflush from here also, although all notifications are done by scene pin
	if(m_pFilter->outputPin->IsConnected() && !m_pFilter->outputPin->m_pOutputQueue->IsFlushing())	{
		hr = m_pFilter->outputPin->DeliverBeginFlush();
		if(FAILED(hr))	{
			return hr;
		}
	}

	CAutoLock lock2(&m_pFilter->m_csReceive);
	m_pFilter->pSampleCollector->Reset();

	return NOERROR;
}

HRESULT CPinInputBackground::EndFlush()	{

	CAutoLock lock(&m_pFilter->m_csMultiIO);

	DbgLog((LOG_TRACE,0,TEXT("bkgndPin endflush")));
	//HRESULT hr;
/*
	if(m_pFilter->outputPin->IsConnected())	{
		hr = m_pFilter->outputPin->DeliverEndFlush();
		if(FAILED(hr))	{
			return hr;
		}
	}
*/
	return CBaseInputPin::EndFlush();
}

HRESULT CPinInputBackground::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate )	{
	
	CAutoLock lock(&m_pFilter->m_csReceive);
	//HRESULT hr;
/*
	if(m_pFilter->outputPin->IsConnected())	{
		hr = m_pFilter->outputPin->DeliverNewSegment(tStart, tStop, dRate);
		if(FAILED(hr))	{
			return hr;
		}
	}
*/
	return NOERROR;
}




// This is called when we start running or go paused. We create the
// output queue object to send data to our associated peer pin
//
HRESULT CPinOutput::Active()	{

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

	// don't forget to Commit()!
	if (m_pIPPAllocator == NULL) {
		DbgLog((LOG_TRACE,0,TEXT("no allocator")));
        return VFW_E_NO_ALLOCATOR;
    }
	DbgLog((LOG_TRACE,0,TEXT("committing")));
    return m_pIPPAllocator->Commit();
}

// Called when we stop streaming, we delete the output queue
HRESULT CPinOutput::Inactive()	{

	DbgLog((LOG_TRACE,0,TEXT("CPinOutput::Inactive()...waiting")));
	WaitForSingleObject(m_pFilter->hSafeToDecommit, INFINITE);

	// decommitt our allocator - it's why we override it in the first place
    if (m_pIPPAllocator == NULL) {
        return VFW_E_NO_ALLOCATOR;
    }
    HRESULT hr = m_pIPPAllocator->Decommit();

	DbgLog((LOG_TRACE,0,TEXT("CPinOutput::Decommitted()")));

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


HRESULT CPinOutput::BreakConnect()
{
	CAutoLock lock(&m_pFilter->m_csMultiIO);

	HRESULT hr = CBaseOutputPin::BreakConnect();
	if (SUCCEEDED(hr))	{
		// pass it through to the filter class
		PIN_DIRECTION direction;
		QueryDirection(&direction);

		m_pMultiIO->OnDisconnect(this);
	}

	// Release any allocator that we are holding
    if (m_pIPPAllocator)
    {
        m_pIPPAllocator->Release();
        m_pIPPAllocator = NULL;
    }

	return hr;
}


// actually, no need to override the next two methods
HRESULT CPinInputBackground::Inactive(void){

	DbgLog((LOG_TRACE,0,TEXT("INPUTFAKINGPIN::Inactive()...waiting")));

	//WaitForSingleObject(m_pFilter->hSafeToDecommit, INFINITE);

	m_bRunTimeError = FALSE;
	
    if (m_pAllocator == NULL) {
        return VFW_E_NO_ALLOCATOR;
    }
	m_bFlushing = FALSE;

    return m_pAllocator->Decommit();
}

HRESULT CPinInputScene::Inactive(void){

	DbgLog((LOG_TRACE,0,TEXT("INPUTFAKINGPI2N::Inactive()...waiting")));

	//WaitForSingleObject(m_pFilter->hSafeToDecommit, INFINITE);

    m_bRunTimeError = FALSE;
	
    if (m_pAllocator == NULL) {
        return VFW_E_NO_ALLOCATOR;
    }
	m_bFlushing = FALSE;

    return m_pAllocator->Decommit();
}