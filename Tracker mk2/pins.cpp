#ifndef PINS_H
	#include "pins.h"
#endif

#ifndef KALMANTRACKING_H
	#include "..\Tracker\kalmanTracking.h"
#endif



CPinOutput::CPinOutput(CBaseMultiIOFilter* pFilter,
		CCritSec* pLock,
		HRESULT* phr,
		LPCWSTR pName) :
	CBaseMultiIOOutputPin(pFilter, pLock, phr, pName, 0),
	m_pFilter((CTrackerMkII*) pFilter),
	m_pListAllocator(NULL),
	m_pPosition(NULL)
{
}

CPinOutput::~CPinOutput()	{
	if( m_pListAllocator )
	{
		// already decommitted here
		m_pListAllocator = NULL;
	};
	if (m_pPosition) m_pPosition->Release();
}


HRESULT CPinOutput::CheckMediaType( const CMediaType* pMediaType )
{
	CAutoLock lock(&m_pFilter->m_csMultiIO);

	CheckPointer(pMediaType,E_POINTER);
/*test	if ((pMediaType->subtype != MEDIASUBTYPE_CMovingObjects))
	{
		return VFW_E_TYPE_NOT_ACCEPTED;
	}
*/
	if(pMediaType->subtype != MEDIASUBTYPE_RGB24)	{
		return VFW_E_TYPE_NOT_ACCEPTED;
	}
	// reinterpret cast is safe because we already checked formattype and cbFormat
	VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(pMediaType->pbFormat);

	m_pFilter->InitializeProcessParams(pVih);

	return NOERROR;
}

// When the input pin doesn't provide a valid allocator we have to come with a proposition
HRESULT CPinOutput::InitAllocator(IMemAllocator **ppAlloc)	{

	DbgLog((LOG_TRACE,0,TEXT("InitAllocator")));

	// hr has to be initialized to NOERROR because it is set in CBaseAllocator::CBaseAllocator
	// only if an error occurs
	HRESULT hr = NOERROR;

	if(FAILED(hr = InitializeAllocatorRequirements())) return hr;

	//testm_pListAllocator = new CListAllocatorOut(
	m_pListAllocator = new CIPPAllocatorOut_8u_C3(
		m_pFilter,
		&m_pFilter->m_csReceive,
		this,
		NAME("ListAllocatorOut"),
		NULL,
		&hr
	);
	if (!m_pListAllocator)
	{
		return E_OUTOFMEMORY;
	}
	if (FAILED(hr))
    {
        delete m_pListAllocator;
        return hr;
    }

	// this is important to set up, otherwise VFW_E_SIZENOTSET occurs from CBaseAllocator::Alloc
	CTrackerMkII* pFilt = (CTrackerMkII*) m_pFilter;
	m_pListAllocator->m_lAlignment = pFilt->outputAllocProps.cbAlign;	
	m_pListAllocator->m_lSize = pFilt->outputAllocProps.cbBuffer;
	m_pListAllocator->m_lCount = pFilt->outputAllocProps.cBuffers;
	m_pListAllocator->m_lPrefix = pFilt->outputAllocProps.cbPrefix;

	// Return the IMemAllocator interface.
	return m_pListAllocator->QueryInterface( IID_IMemAllocator, (void**)ppAlloc );
}


HRESULT CPinOutput::InitializeAllocatorRequirements()	{

	CAutoLock lock(&m_pFilter->m_csMultiIO);

	if( m_pFilter->inputPinLuma->IsConnected() == FALSE &&
		m_pFilter->inputPinColor->IsConnected() == FALSE )	{
        return E_UNEXPECTED;
    }

    HRESULT hr = NOERROR;
//	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) m_pFilter->m_pLuma->CurrentMediaType().Format();

//test	m_pFilter->outputAllocProps.cbBuffer = 30;
	m_pFilter->outputAllocProps.cbBuffer = m_pFilter->frameSize.height*m_pFilter->step_8u_C3;
	m_pFilter->outputAllocProps.cBuffers = 1;
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
	HRESULT hr1, hr2;

	// iterate through all connected input pins and notify the sink or pass the notification on
	if( m_pFilter->inputPinLuma->IsConnected() )	{
		if( m_pFilter->inputPinLuma->m_pQSink )	{
			return m_pFilter->inputPinLuma->m_pQSink->Notify(m_pMultiIO, q);
		}
		hr1 = m_pFilter->inputPinLuma->PassNotify(q);
	}

	if( m_pFilter->inputPinColor->IsConnected() )	{
		if( m_pFilter->inputPinColor->m_pQSink )	{
			return m_pFilter->inputPinColor->m_pQSink->Notify(m_pMultiIO, q);
		}
		hr2 = m_pFilter->inputPinColor->PassNotify(q);
	}
	if(FAILED(hr1))
		return hr1;
	if(FAILED(hr1))
		return hr2;

	return S_OK;
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


// overriden to expose IMediaPosition and IMediaSeeking control interfaces
STDMETHODIMP CPinOutput::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
    CheckPointer(ppv,E_POINTER);
    ValidateReadWritePtr(ppv,sizeof(PVOID));
    *ppv = NULL;

    if (riid == IID_IMediaPosition || riid == IID_IMediaSeeking) {
		// seeking notifications will go via inputPinLuma
        ASSERT(m_pFilter->inputPinLuma != NULL);

        if (m_pPosition == NULL) {
            HRESULT hr = CreatePosPassThru(
                             GetOwner(),
                             FALSE,
							 (IPin *)m_pFilter->inputPinLuma,
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



HRESULT CPinInputColor::CheckMediaType(const CMediaType *pmt)
{
	CAutoLock lock(&m_pFilter->m_csMultiIO);

	CheckPointer(pmt, E_POINTER);

if ((pmt->majortype != MEDIATYPE_Video) ||
		(pmt->subtype != MEDIASUBTYPE_RGB32) ||
		(pmt->formattype != FORMAT_VideoInfo) ||		// is this really necessary?
		(pmt->cbFormat < sizeof(VIDEOINFOHEADER)))		// do all CMediaTypes have a VIH format?
	{
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	return NOERROR;
}


HRESULT CPinInputColor::Receive(IMediaSample *pSample)	{

	HRESULT hr;
	CAutoLock lock(&m_pFilter->m_csReceive);

	hr = CBaseInputPin::Receive(pSample);
	if (hr != S_OK)	return hr;

	CheckPointer(pSample,E_POINTER);
	ValidateReadPtr(pSample,sizeof(IMediaSample));
	ASSERT(pSample);

	return m_pFilter->CollectSample(pSample, this);
}


HRESULT CPinInputLuma::CheckMediaType(const CMediaType *pmt)
{
	CAutoLock lock(&m_pFilter->m_csMultiIO);

	CheckPointer(pmt,E_POINTER);

	if ((pmt->majortype != MEDIATYPE_Video) ||
		(pmt->subtype != MEDIASUBTYPE_8BitMonochrome_internal) ||
		(pmt->formattype != FORMAT_VideoInfo) ||		// is this really necessary?
		(pmt->cbFormat < sizeof(VIDEOINFOHEADER)))		// do all CMediaTypes have a VIH format?
	{
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	// reinterpret cast is safe because we already checked formattype and cbFormat
	VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(pmt->pbFormat);

	m_pFilter->InitializeProcessParams(pVih);

	return NOERROR;
}

HRESULT CPinInputLuma::Receive(IMediaSample *pSample)	{

	HRESULT hr;
	CAutoLock lock(&m_pFilter->m_csReceive);
	
	hr = CBaseInputPin::Receive(pSample);
	if (hr != S_OK)	return hr;
		
	ASSERT(pSample);
	CheckPointer(pSample,E_POINTER);
	ValidateReadPtr(pSample,sizeof(IMediaSample));

	return m_pFilter->CollectSample(pSample, this);	
}

HRESULT CPinInputLuma::EndOfStream()	{

	DbgLog((LOG_TRACE,0,TEXT("CPinInputMin::EndOfStream()")));

	// if we're here, the luma pin has received all the samples and came to the end of stream. however,
	// the color pin may still have samples to be received, and a situation may occur where we have a sample
	// from the luma pin queued and receive a sample from the color pin and deliver after EOS.
	// (deliveries should not happen after EOS - this confuses renderer). to avoid this, we empty the luma queue
	//
	// This way we lose a pair of input samples that comes right before EOS:(
	// An alternative might be to allow the other pin also to deliver notifications and handle them
	// together wih the ones from the luma pin.
	//
	//m_pFilter->EmptyLumaQueue();
	//m_pFilter->EmptyColorQueue();

	if(m_pFilter->m_bOKToDeliverEndOfStream)
	{
		CAutoLock lock(&m_pFilter->m_csReceive);

		HRESULT hr;
		if(m_pFilter->outputPin->IsConnected())	{
			hr = m_pFilter->outputPin->DeliverEndOfStream();
			if(FAILED(hr))	{
				return hr;
			}
		}

		m_pFilter->m_bOKToDeliverEndOfStream = FALSE;
		return NOERROR;
	}

	// we want both input pins to confirm before we deliver EOS
	m_pFilter->m_bOKToDeliverEndOfStream = TRUE;

	return NOERROR;
}

HRESULT CPinInputColor::BeginFlush()	{

	DbgLog((LOG_TRACE,0,TEXT("CPinInputColor::BeginFlush()")));

	CAutoLock lock(&m_pFilter->m_csMultiIO);
	HRESULT hr = CBaseInputPin::BeginFlush();

/*	if(m_pFilter->outputPin->IsConnected() && m_pFilter->outputPin->m_pOutputQueue)
	{
		if(!m_pFilter->outputPin->m_pOutputQueue->IsFlushing())
		{
			hr = m_pFilter->outputPin->DeliverBeginFlush();
			if(FAILED(hr))	{
				return hr;
			}
		}
	}
*/
	CAutoLock lock2(&m_pFilter->m_csReceive);
	m_pFilter->EmptyColorQueue();

	DbgLog((LOG_TRACE,0,TEXT("CPinInputColor::BeginFlush() done")));

	return NOERROR;
}

HRESULT CPinInputColor::EndFlush()	{

	DbgLog((LOG_TRACE,0,TEXT("CPinInputColor::EndFlush()")));

	CAutoLock lock(&m_pFilter->m_csMultiIO);
/*
	DbgLog((LOG_TRACE,0,TEXT("inputPinColor endflush")));

	HRESULT hr;

	if(m_pFilter->outputPin->IsConnected())	{
		hr = m_pFilter->outputPin->DeliverEndFlush();
		if(FAILED(hr))	{
			return hr;
		}
	}
*/
	return CBaseInputPin::EndFlush();
}

HRESULT CPinInputColor::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate )	{

	DbgLog((LOG_TRACE,0,TEXT("CPinInputColor::NewSegment")));

	/*CAutoLock lock(&m_pFilter->m_csReceive);	

	HRESULT hr;

	if(m_pFilter->outputPin->IsConnected())
	{
		hr = m_pFilter->outputPin->DeliverNewSegment(tStart, tStop, dRate);
		if(FAILED(hr))	return hr;
	}
*/
	return NOERROR;
}


HRESULT CPinInputColor::EndOfStream()	{

	DbgLog((LOG_TRACE,0,TEXT("CPinInputColor::EndOfStream()")));	

	if(m_pFilter->m_bOKToDeliverEndOfStream)
	{
		CAutoLock lock(&m_pFilter->m_csReceive);

		HRESULT hr;
		if(m_pFilter->outputPin->IsConnected())	{
			hr = m_pFilter->outputPin->DeliverEndOfStream();
			if(FAILED(hr))	{
				return hr;
			}
		}
		m_pFilter->m_bOKToDeliverEndOfStream = FALSE;
		return NOERROR;
	}
	// we want both input pins to confirm before we deliver EOS
	m_pFilter->m_bOKToDeliverEndOfStream = TRUE;

	return NOERROR;
}

HRESULT CPinInputLuma::BeginFlush()	{

	DbgLog((LOG_TRACE,0,TEXT("CPinInputLuma::BeginFlush()")));

	CAutoLock lock(&m_pFilter->m_csMultiIO);

	HRESULT hr = CBaseInputPin::BeginFlush();

	DbgLog((LOG_TRACE,0,TEXT("CPinInputLuma bginlush")));

	if(m_pFilter->outputPin->IsConnected() && m_pFilter->outputPin->m_pOutputQueue)
	{
		if(!m_pFilter->outputPin->m_pOutputQueue->IsFlushing())	{
			hr = m_pFilter->outputPin->DeliverBeginFlush();
			if(FAILED(hr))	{
				return hr;
			}
		}
	}

	CAutoLock lock2(&m_pFilter->m_csReceive);
	m_pFilter->EmptyLumaQueue();

	DbgLog((LOG_TRACE,0,TEXT("CPinInputLuma::BeginFlush() done")));

	return NOERROR;
}

HRESULT CPinInputLuma::EndFlush()	{

	DbgLog((LOG_TRACE,0,TEXT("CPinInputLuma::EndFlush()")));

	CAutoLock lock(&m_pFilter->m_csMultiIO);

	DbgLog((LOG_TRACE,0,TEXT("bkgndPin endflush")));
	HRESULT hr;

	if(m_pFilter->outputPin->IsConnected())	{
		hr = m_pFilter->outputPin->DeliverEndFlush();
		if(FAILED(hr))	{
			return hr;
		}
	}

	return CBaseInputPin::EndFlush();
}

HRESULT CPinInputLuma::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate )	{

	DbgLog((LOG_TRACE,0,TEXT("CPinInputLuma::NewSegment")));
	
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


// This is called when we start running or go paused. We create the
// output queue object to send data to our associated peer pin
//
HRESULT CPinOutput::Active()	{

	DbgLog((LOG_TRACE,0,TEXT("CPinOutput::Inactive")));

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
	if (m_pAllocator == NULL) {
		DbgLog((LOG_TRACE,0,TEXT("no allocator")));
        return VFW_E_NO_ALLOCATOR;
    }
	DbgLog((LOG_TRACE,0,TEXT("committing")));
    return m_pAllocator->Commit();
}

// Called when we stop streaming, we delete the output queue
HRESULT CPinOutput::Inactive()	{

	DbgLog((LOG_TRACE,0,TEXT("CPinOutput::Inactive()")));

	// decommitt our allocator
	m_bRunTimeError = FALSE;
    if (m_pAllocator == NULL) {
        return VFW_E_NO_ALLOCATOR;
    }
    HRESULT hr = m_pAllocator->Decommit();

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

	DbgLog((LOG_TRACE,0,TEXT("queue deleted")));

	return hr;
}



// actually, no need to override the next two methods
HRESULT CPinInputLuma::Inactive(void){

	DbgLog((LOG_TRACE,0,TEXT("CPinInputLuma::Inactive")));

	m_bRunTimeError = FALSE;
	
    if (m_pAllocator == NULL) {
        return VFW_E_NO_ALLOCATOR;
    }
	m_bFlushing = FALSE;

	{
		CAutoLock lock(&m_pFilter->m_csReceive);

		// queue used in syncing input samples
		m_pFilter->EmptyLumaQueue();
	}
    return m_pAllocator->Decommit();
}

HRESULT CPinInputColor::Inactive(void){

	DbgLog((LOG_TRACE,0,TEXT("CPinInputColor::Inactive")));


    m_bRunTimeError = FALSE;
	
    if (m_pAllocator == NULL) {
        return VFW_E_NO_ALLOCATOR;
    }
	m_bFlushing = FALSE;

	{
		CAutoLock lock(&m_pFilter->m_csReceive);

		// queue used in syncing input samples
		m_pFilter->EmptyColorQueue();
	}

    return m_pAllocator->Decommit();
}

// called when upstream filter's output pin starts negotiating allocators;
// our input pin requirements will be given to him.
// also, input ALLOCATOR_PROPERTIES will get initialized and stored locally in main filter class
STDMETHODIMP CPinInputColor::GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps)
{
	CAutoLock cAutolock(&m_pFilter->m_csMultiIO);

	DbgLog((LOG_TRACE,0,TEXT("GetAllocatorRequirements")));

	// we haven't initialized the requirements yet, so we do it here
	InitializeAllocatorRequirements();

	CTrackerMkII* pFilt = (CTrackerMkII*) m_pFilter;
	CheckPointer(pProps, E_POINTER);
	ValidateReadWritePtr(pProps, sizeof(ALLOCATOR_PROPERTIES));

	pProps->cbAlign = pFilt->inputAllocProps.cbAlign;
	pProps->cbBuffer = pFilt->inputAllocProps.cbBuffer;
	pProps->cbPrefix = pFilt->inputAllocProps.cbPrefix;
	pProps->cbPrefix = pFilt->inputAllocProps.cBuffers;

	return S_OK;
}


HRESULT CPinInputColor::InitializeAllocatorRequirements()	{

	// check input pin connection
	if(IsConnected() == FALSE)	{
        return E_UNEXPECTED;
    }

    HRESULT hr = NOERROR;

	CTrackerMkII* pFilt = (CTrackerMkII*) m_pFilter;
	pFilt->inputAllocProps.cbAlign = 32;
	pFilt->inputAllocProps.cbBuffer = 5;
	pFilt->inputAllocProps.cbPrefix = 0;
	pFilt->inputAllocProps.cBuffers = 1;

	return NOERROR;
}

STDMETHODIMP CPinInputColor::NotifyAllocator(IMemAllocator* pAllocator, BOOL bReadOnly)	{

	// update inputAllocProps
	CTrackerMkII* pFilt = (CTrackerMkII*) m_pFilter;
	pAllocator->GetProperties(&(pFilt->inputAllocProps));

	DbgLog((LOG_TRACE,0,TEXT("allocator that we got looks like: %d %d"),
		pFilt->inputAllocProps.cbBuffer, pFilt->inputAllocProps.cBuffers));

	return CBaseInputPin::NotifyAllocator(pAllocator, bReadOnly);
}