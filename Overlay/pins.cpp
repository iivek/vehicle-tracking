#ifndef PINS_H
	#include "pins.h"
#endif

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
	m_pFilter((COverlay*) pFilter),
	m_pIPPAllocator(NULL),
	m_pPosition(NULL)
{
}

CPinOutput::~CPinOutput()	{
	if( m_pIPPAllocator )
	{
		// already decommitted here
		m_pIPPAllocator = NULL;
	};
	if (m_pPosition) m_pPosition->Release();
}


HRESULT CPinOutput::CheckMediaType( const CMediaType* pMediaType )
{
	CAutoLock lock(&m_pFilter->m_csMultiIO);

	CheckPointer(pMediaType,E_POINTER);

	if ((pMediaType->majortype != MEDIATYPE_Video) ||
		(pMediaType->subtype != MEDIASUBTYPE_RGB24) ||
		(pMediaType->formattype != FORMAT_VideoInfo) ||		// is this really necessary?
		(pMediaType->cbFormat < sizeof(VIDEOINFOHEADER)))	// do all CMediaTypes have a VIH format?
	{
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	// reinterpret cast is safe because we already checked formattype and cbFormat
	VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(pMediaType->pbFormat);

	m_pFilter->InitializeProcessParams(pVih);

	return NOERROR;
}

// When the input pin doesn't provide a valid allocator we have to come with a proposition
HRESULT CPinOutput::InitAllocator(IMemAllocator **ppAlloc)	{

	CAutoLock lock(&m_pFilter->m_csReceive);

	// hr has to be initialized to NOERROR because it is set in CBaseAllocator::CBaseAllocator
	// only if an error occurs
	HRESULT hr = NOERROR;

	if(FAILED(hr = InitializeAllocatorRequirements()))
	{
		DbgLog((LOG_TRACE,0,TEXT("bu!")));
		return hr;
	}

	m_pIPPAllocator = new CIPPAllocatorOut_8u_C3(
		m_pFilter,
		&m_pFilter->m_csReceive,		//sync with receive
		this,
		NAME("IPPAllocator"),
		NULL,
		&hr
		);

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

	// Return the IMemAllocator interface.
	return m_pIPPAllocator->QueryInterface( IID_IMemAllocator, (void**)ppAlloc );
}


HRESULT CPinOutput::InitializeAllocatorRequirements()	{

	CAutoLock lock(&m_pFilter->m_csMultiIO);

	if( m_pFilter->inputMainPin->IsConnected() == FALSE &&
		m_pFilter->inputOverlayPin->IsConnected() == FALSE )	{
        return E_UNEXPECTED;
    }

    HRESULT hr = NOERROR;

	// at this timepoint it's convenient to calculate strides and what our allocators might need

	m_pFilter->outputAllocProps.cbBuffer = m_pFilter->frameSize.height*m_pFilter->step_8u_C3;
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
	HRESULT hr1, hr2;

	// iterate through all connected input pins and notify the sink or pass the notification on
	if( m_pFilter->inputMainPin->IsConnected() )	{
		if( m_pFilter->inputMainPin->m_pQSink )	{
			return m_pFilter->inputMainPin->m_pQSink->Notify(m_pMultiIO, q);
		}
		hr1 = m_pFilter->inputMainPin->PassNotify(q);
	}

	if( m_pFilter->inputOverlayPin->IsConnected() )	{
		if( m_pFilter->inputOverlayPin->m_pQSink )	{
			return m_pFilter->inputOverlayPin->m_pQSink->Notify(m_pMultiIO, q);
		}
		hr2 = m_pFilter->inputOverlayPin->PassNotify(q);
	}
	if(FAILED(hr1))
		return hr1;
	if(FAILED(hr1))
		return hr2;


	return S_OK;
}


HRESULT CPinOutput::DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc)	{
    
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


// overriden to expose IMediaPosition and IMediaSeeking control interfaces
STDMETHODIMP CPinOutput::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
    CheckPointer(ppv,E_POINTER);
    ValidateReadWritePtr(ppv,sizeof(PVOID));
    *ppv = NULL;

    if (riid == IID_IMediaPosition || riid == IID_IMediaSeeking) {
		// seeking notifications will go via inputMainPin
        ASSERT(m_pFilter->inputMainPin != NULL);

        if (m_pPosition == NULL) {
            HRESULT hr = CreatePosPassThru(
                             GetOwner(),
                             FALSE,
							 (IPin *)m_pFilter->inputMainPin,
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



HRESULT CPinInputOverlay::CheckMediaType(const CMediaType *pmt)
{
	CAutoLock lock(&m_pFilter->m_csMultiIO);

	CheckPointer(pmt, E_POINTER);

	if (//(pmt->majortype != MEDIATYPE_Video) ||
		(pmt->subtype != MEDIASUBTYPE_CMovingObjects))// ||
		//(pmt->formattype != FORMAT_VideoInfo) ||		// is this really necessary?
		//(pmt->cbFormat < sizeof(VIDEOINFOHEADER)))		// do all CMediaTypes have a VIH format?
	{
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	return NOERROR;
}


HRESULT CPinInputOverlay::Receive(IMediaSample *pSample)	{

	HRESULT hr;
	CAutoLock lock(&m_pFilter->m_csReceive);

	hr = CBaseInputPin::Receive(pSample);
	if (hr != S_OK)	return hr;

	CheckPointer(pSample,E_POINTER);
	ValidateReadPtr(pSample,sizeof(IMediaSample));
	ASSERT(pSample);

	return m_pFilter->CollectSample(pSample, this);
}


HRESULT CPinInputMain::CheckMediaType(const CMediaType *pmt)
{
	CAutoLock lock(&m_pFilter->m_csMultiIO);

	CheckPointer(pmt,E_POINTER);

	if ((pmt->majortype != MEDIATYPE_Video) ||
		(pmt->subtype != MEDIASUBTYPE_RGB32) ||
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

HRESULT CPinInputMain::Receive(IMediaSample *pSample)	{

	HRESULT hr;
	CAutoLock lock(&m_pFilter->m_csReceive);
	
	hr = CBaseInputPin::Receive(pSample);
	if (hr != S_OK)	return hr;
		
	ASSERT(pSample);
	CheckPointer(pSample,E_POINTER);
	ValidateReadPtr(pSample,sizeof(IMediaSample));

	return m_pFilter->CollectSample(pSample, this);	

/*
	HRESULT hr;
	CAutoLock lock(&m_pFilter->m_csReceive);
	
	hr = CBaseInputPin::Receive(pSample);
	if (hr != S_OK)	return hr;
	
	ASSERT(pSample);
	CheckPointer(pSample,E_POINTER);
	ValidateReadPtr(pSample,sizeof(IMediaSample));
		
	hr = NOERROR;

	IppStatus status;
	BYTE* pMain_8u_AC4;
	BYTE* pOut_8u_C3;

	// create a new media sample based on the sample from main input pin
	IMediaSample *pOutSample;

	DbgLog((LOG_TRACE,0,TEXT("getting output sample")));
	hr = m_pFilter->InitializeOutputSample(pSample, &pOutSample);
	if(FAILED(hr)) return hr;
	DbgLog((LOG_TRACE,0,TEXT("we have output sample")));
	
	// get the addresses of the actual data (buffers) and set IPL headers
	pSample->GetPointer( &pMain_8u_AC4 );
	pOutSample->GetPointer( &pOut_8u_C3 );

		// copy main buffer to output
	status = ippiCopy_8u_AC4C3R(pMain_8u_AC4, m_pFilter->step_8u_AC4, pOut_8u_C3, m_pFilter->step_8u_C3, m_pFilter->frameSize);
	m_pFilter->outputPin->Deliver(pOutSample);
	
	return hr;
	*/
}

HRESULT CPinInputMain::EndOfStream()	{

	DbgLog((LOG_TRACE,0,TEXT("CPinInputMin::EndOfStream()")));

	
	// if we're here, the main pin has received all the samples and came to the end of stream. however,
	// the overlay pin may still have samples to be received, and a situation may occur where we have a sample
	// from the main pin queued and receive a sample from the overlay pin and deliver after EOS.
	// (deliveries should not happen after EOS - this confuses renderer). to avoid this, we empty the main queue
	//
	// This way we lose a pair of input samples that comes right before EOS:(
	// An alternative might be to allow the overlay also to deliver notifications and handle them
	// together wih the ones from the main pin.
	//
	//m_pFilter->EmptyMainQueue();
	//m_pFilter->EmptyOverlayQueue();


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

HRESULT CPinInputOverlay::BeginFlush()	{

	DbgLog((LOG_TRACE,0,TEXT("CPinInputOverlay::BeginFlush()")));

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
	m_pFilter->EmptyOverlayQueue();

	DbgLog((LOG_TRACE,0,TEXT("CPinInputOverlay::BeginFlush() done")));

	return NOERROR;
}

HRESULT CPinInputOverlay::EndFlush()	{

	DbgLog((LOG_TRACE,0,TEXT("CPinInputOverlay::EndFlush()")));

	CAutoLock lock(&m_pFilter->m_csMultiIO);
/*
	DbgLog((LOG_TRACE,0,TEXT("inputOverlayPin endflush")));

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

HRESULT CPinInputOverlay::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate )	{

	DbgLog((LOG_TRACE,0,TEXT("CPinInputOverlay::NewSegment")));

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


HRESULT CPinInputOverlay::EndOfStream()	{

	DbgLog((LOG_TRACE,0,TEXT("CPinInputOverlay::EndOfStream()")));	

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

HRESULT CPinInputMain::BeginFlush()	{

	DbgLog((LOG_TRACE,0,TEXT("CPinInputMain::BeginFlush()")));

	CAutoLock lock(&m_pFilter->m_csMultiIO);

	HRESULT hr = CBaseInputPin::BeginFlush();

	DbgLog((LOG_TRACE,0,TEXT("CPinInputMain bginlush")));

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
	m_pFilter->EmptyMainQueue();

	DbgLog((LOG_TRACE,0,TEXT("CPinInputMain::BeginFlush() done")));

	return NOERROR;
}

HRESULT CPinInputMain::EndFlush()	{

	DbgLog((LOG_TRACE,0,TEXT("CPinInputMain::EndFlush()")));

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

HRESULT CPinInputMain::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate )	{

	DbgLog((LOG_TRACE,0,TEXT("CPinInputMain::NewSegment")));
	
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
HRESULT CPinInputMain::Inactive(void){

	DbgLog((LOG_TRACE,0,TEXT("CPinInputMain::Inactive")));

	m_bRunTimeError = FALSE;
	
    if (m_pAllocator == NULL) {
        return VFW_E_NO_ALLOCATOR;
    }
	m_bFlushing = FALSE;

	{
		CAutoLock lock(&m_pFilter->m_csReceive);

		// queue used in syncing input samples
		m_pFilter->EmptyMainQueue();
	}
    return m_pAllocator->Decommit();
}

HRESULT CPinInputOverlay::Inactive(void){

	DbgLog((LOG_TRACE,0,TEXT("CPinInputOverlay::Inactive")));


    m_bRunTimeError = FALSE;
	
    if (m_pAllocator == NULL) {
        return VFW_E_NO_ALLOCATOR;
    }
	m_bFlushing = FALSE;

	{
		CAutoLock lock(&m_pFilter->m_csReceive);

		// queue used in syncing input samples
		m_pFilter->EmptyOverlayQueue();
	}

    return m_pAllocator->Decommit();
}

// called when upstream filter's output pin starts negotiating allocators;
// our input pin requirements will be given to him.
// also, input ALLOCATOR_PROPERTIES will get initialized and stored locally in main filter class
STDMETHODIMP CPinInputOverlay::GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps)
{
	CAutoLock cAutolock(&m_pFilter->m_csMultiIO);

	DbgLog((LOG_TRACE,0,TEXT("GetAllocatorRequirements")));

	// we haven't initialized the requirements yet, so we do it here
	InitializeAllocatorRequirements();

	COverlay* pFilt = (COverlay*) m_pFilter;
	CheckPointer(pProps, E_POINTER);
	ValidateReadWritePtr(pProps, sizeof(ALLOCATOR_PROPERTIES));

	pProps->cbAlign = pFilt->inputAllocProps.cbAlign;
	pProps->cbBuffer = pFilt->inputAllocProps.cbBuffer;
	pProps->cbPrefix = pFilt->inputAllocProps.cbPrefix;
	pProps->cbPrefix = pFilt->inputAllocProps.cBuffers;

	return S_OK;
}


HRESULT CPinInputOverlay::InitializeAllocatorRequirements()	{

	// check input pin connection
	if(IsConnected() == FALSE)	{
        return E_UNEXPECTED;
    }

    HRESULT hr = NOERROR;

	COverlay* pFilt = (COverlay*) m_pFilter;
	pFilt->inputAllocProps.cbAlign = 32;
	pFilt->inputAllocProps.cbBuffer = 1;
	pFilt->inputAllocProps.cbPrefix = 0;
	pFilt->inputAllocProps.cBuffers = 1;

	return NOERROR;
}

STDMETHODIMP CPinInputOverlay::NotifyAllocator(IMemAllocator* pAllocator, BOOL bReadOnly)	{

	// update inputAllocProps
	COverlay* pFilt = (COverlay*) m_pFilter;
	pAllocator->GetProperties(&(pFilt->inputAllocProps));

	DbgLog((LOG_TRACE,0,TEXT("allocaror that we got looks like: %d %d"),
		pFilt->inputAllocProps.cbBuffer, pFilt->inputAllocProps.cBuffers));

	return CBaseInputPin::NotifyAllocator(pAllocator, bReadOnly);
}