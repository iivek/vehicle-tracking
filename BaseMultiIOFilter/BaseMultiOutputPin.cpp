#pragma warning(disable:4312)

#ifndef BASEMULTIOUTPUTPIN_H
	#include "..\BaseMultiIOFilter\BaseMultiOutputPin.h"
#endif

#ifndef BASEMULTIIOFILTER_H
	#include "..\BaseMultiIOFilter\BaseMultiIOFilter.h"
#endif


CBaseMultiIOOutputPin::CBaseMultiIOOutputPin(
	CBaseMultiIOFilter* pFilter, CCritSec* pLock, HRESULT* phr, LPCWSTR pName, int nIndex)
	: CBaseOutputPin(NAME("Output Pin"), pFilter, pLock, phr, pName),
	m_pMultiIO(pFilter),
	m_pinIndex(nIndex),
	m_pOutputQueue(NULL),	// has to be initialized or we would never create a queue
	m_cOurRef(0)
	{
		DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOOutputPin::CBaseMultiIOOutputPin")));
	}

CBaseMultiIOOutputPin::~CBaseMultiIOOutputPin()
{
	DbgLog((LOG_TRACE,0,TEXT("We're in the output pin destructor")));
}


	 //________________________________________
	//	Connection stuff
   //_________


HRESULT CBaseMultiIOOutputPin::CheckMediaType( const CMediaType* pMediaType )
{
	CAutoLock lock(&m_pMultiIO->m_csMultiIO);

	return m_pMultiIO->CheckOutputType(pMediaType);
}

HRESULT CBaseMultiIOOutputPin::GetMediaType( int iPosition, CMediaType* pMediaType )
{
	CAutoLock lock(&m_pMultiIO->m_csMultiIO);
	// pass the call to the filter class, VFW_S_NO_MORE_ITEMS will eventually be returned from it
	return m_pMultiIO->GetMediaType(iPosition, pMediaType, this);
}


HRESULT CBaseMultiIOOutputPin::CompleteConnect(IPin *pReceivePin)
{
	CAutoLock lock(&m_pMultiIO->m_csMultiIO);

	HRESULT	hr = CBaseOutputPin::CompleteConnect(pReceivePin);
	if (SUCCEEDED(hr))	{
		// inside the OnConnect we will add additional pins if neccessary

		m_pMultiIO->OnConnect(this);
	}

	return hr;
}

HRESULT CBaseMultiIOOutputPin::BreakConnect()
{
	CAutoLock lock(&m_pMultiIO->m_csMultiIO);

	HRESULT hr = CBaseOutputPin::BreakConnect();
	if (SUCCEEDED(hr))	{
		// pass it through to the filter class
		PIN_DIRECTION direction;
		QueryDirection(&direction);

		m_pMultiIO->OnDisconnect(this);
	}

	return hr;
}


HRESULT CBaseMultiIOOutputPin::DecideBufferSize( IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pRequestProperties )
{
	//Pass the buffer allocation on to the mixer filter
	return m_pMultiIO->DecideBufferSize(pAlloc, pRequestProperties, this);
}

	 //________________________________________
	//	Output queue, as in inftee filter
   //_________


// This is called when we start running or go paused. We create the
// output queue object to send data to our associated peer pin
//
HRESULT CBaseMultiIOOutputPin::Active()	{

    HRESULT hr = NOERROR;

    // Make sure that the pin is connected
    ASSERT(m_Connected != NULL);

    // Create the output queue if we have to
    if(m_pOutputQueue == NULL)
    {
        m_pOutputQueue = new CMyOutputQueue(m_Connected, &hr, TRUE, FALSE);
		if(m_pOutputQueue == NULL)	{
			DbgLog((LOG_TRACE,0,TEXT("queue outofmem")));
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

	DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOOutputPin::Active(), queue created")));

    // Pass the call on to the base class
    return hr = CBaseOutputPin::Active();
}

// Called when we stop streaming, we delete the output queue
HRESULT CBaseMultiIOOutputPin::Inactive()	{

	DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOOutputPin::Inactive()")));

    // Delete the output queues associated with the pin.
    if(m_pOutputQueue)
    {
        delete m_pOutputQueue;
        m_pOutputQueue = NULL;
    }

    CBaseOutputPin::Inactive();

    return NOERROR;
}

HRESULT CBaseMultiIOOutputPin::Deliver(IMediaSample *pMediaSample)
{
	DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOOutputPin::Deliver(), %d"), m_pinIndex));

    CheckPointer(pMediaSample,E_POINTER);

	// let the sample die
	if(m_pOutputQueue == NULL)	{
		pMediaSample->Release();
        return NOERROR;
	}
		
    HRESULT hr = m_pOutputQueue->Receive(pMediaSample);
	return hr;
}

HRESULT CBaseMultiIOOutputPin::DeliverEndOfStream()
{
	CAutoLock lock(&m_pMultiIO->m_csReceive);

	DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOOutputPin::DeliverEndOfStream()")));

    // Make sure that we have an output queue
	if(m_pOutputQueue == NULL)	{
		return m_Connected->EndOfStream();
	}

    m_pOutputQueue->EOS();
    return NOERROR;
}


HRESULT CBaseMultiIOOutputPin::DeliverBeginFlush()
{
	CAutoLock lock(&m_pMultiIO->m_csMultiIO);

	DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOOutputPin::DBF()")));

    // Make sure that we have an output queue
	if(m_pOutputQueue == NULL)	{
		DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOOutputPin::DBF(), we don't have a queue, delivering directly")));
		return m_Connected->BeginFlush();
	}

	// should not be called if we're already flushing
			DbgLog((LOG_TRACE,0,TEXT("			IsFlushing called!")));
	ASSERT(!m_pOutputQueue->IsFlushing());

	DbgLog((LOG_TRACE,0,TEXT("flushing the queue... working")));
	m_pOutputQueue->BeginFlush();
	DbgLog((LOG_TRACE,0,TEXT("flushing the queue... dunn")));

    return NOERROR;
}

HRESULT CBaseMultiIOOutputPin::DeliverEndFlush()	{

	CAutoLock lock(&m_pMultiIO->m_csMultiIO);
	DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOOutputPin::DEF(), pin %d"), m_pinIndex));

    // if we don't have a queue
	if(m_pOutputQueue == NULL)	{
		DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOOutputPin::DEF(), calling endflisfadsh")));
		return m_Connected->EndFlush();
	}
	DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOOutputPin::DEF(), calling endflish")));

	// shuould not be called unless we're not flushing
	DbgLog((LOG_TRACE,0,TEXT("			IsFlushing called! 2")));
	ASSERT(m_pOutputQueue->IsFlushing());
    m_pOutputQueue->EndFlush();

    return NOERROR;
}

HRESULT CBaseMultiIOOutputPin::DeliverNewSegment(REFERENCE_TIME tStart, 
                                         REFERENCE_TIME tStop,  
                                         double dRate)	{

	CAutoLock lock(&m_pMultiIO->m_csReceive);

    // Make sure that we have an output queue
	if(m_pOutputQueue == NULL)	{
		return m_Connected->NewSegment(tStart, tStop, dRate);
	}

    m_pOutputQueue->NewSegment(tStart, tStop, dRate);

	return NOERROR;

}

// override this if you have some idea what to do with it
// for now our first output pin will send notifications to all input pins

HRESULT CBaseMultiIOOutputPin::Notify(IBaseFilter * pSender, Quality q){

	CAutoLock lock(&m_pMultiIO->m_csMultiIO);

	UNREFERENCED_PARAMETER(pSender);
    ValidateReadPtr(pSender,sizeof(IBaseFilter));
	HRESULT hr;

	if(this == m_pMultiIO->m_outputPins.begin()->second)	{
		// iterate through all connected input pins and notify the sink or pass the notification on
	
		INPUT_PINS::iterator iter = m_pMultiIO->m_inputPins.begin();
		while(iter != m_pMultiIO->m_inputPins.end()){
			if( iter->second->IsConnected())	{
				//find sink
				if(iter->second->m_pQSink != NULL)	{
					return iter->second->m_pQSink->Notify(m_pMultiIO, q);
				}
				hr = iter->second->PassNotify(q);
				if(FAILED(hr))
						return hr;
			}	
			++iter;
		}
	
	}
	return S_OK;
}


//
// Next two methods are taken and adapted from InfPinTee project
//
// NonDelegatingAddRef
//
// We need override this method so that we can do proper reference counting
// on our Output pins. The base class CBasePin does not do any reference
// counting on the pin in RETAIL.
//
// Please refer to the comments for the NonDelegatingRelease method for more
// info on why we need to do this.
//
STDMETHODIMP_(ULONG) CBaseMultiIOOutputPin::NonDelegatingAddRef()
{
    CAutoLock lock(&m_pMultiIO->m_csMultiIO);

#ifdef DEBUG
    // Update the debug only variable maintained by the base class
    m_cRef++;
    ASSERT(m_cRef > 0);
#endif

    // Now update our reference count
    m_cOurRef++;
    ASSERT(m_cOurRef > 0);

    return m_cOurRef;

} // NonDelegatingAddRef



//
// NonDelegatingRelease
//
// If the reference count is one, this means that the pin is held by our filter only,
// i.e. it is free for us to do whatever we want with it
//
// Also, since CBasePin::NonDelegatingAddRef passes the call to the owning
// filter, we will have to call Release on the owning filter as well.
//
// Note that we maintain our own reference count m_cOurRef as the m_cRef
// variable maintained by CBasePin is debug only.
//
STDMETHODIMP_(ULONG) CBaseMultiIOOutputPin::NonDelegatingRelease()	{
    CAutoLock lock(&m_pMultiIO->m_csMultiIO);

#ifdef DEBUG
    // Update the debug only variable in CBasePin
    m_cRef--;
    ASSERT(m_cRef >= 0);
#endif

    // Now update our reference count
    m_cOurRef--;
    ASSERT(m_cOurRef >= 0);

	// if the reference count on the object has gone to one, this means that noone
	// other than our filter has it addref'fed

    // Also, when the ref count drops to 0, it really means that our
    // filter that is holding one ref count has released it so we
    // should delete the pin as well.

	if(m_cOurRef <= 1)	{
		if(m_cOurRef == 1)	{		
	
			// we kill this pin if there is another unconnected pin beyond the minimum of pins
			// we set in the filter class constructor.
			// note that we want one free pin at all times. Ovride if u need mo
			if( m_pMultiIO->m_outputPins.size() > m_pMultiIO->m_nrInitialOut &&
				m_pMultiIO->m_outputPins.size() - m_pMultiIO->AllOutputConnected() > 1 )	{
	
#ifdef DEBUG
				m_cRef = 0;
#endif

				m_pMultiIO->DeleteOutputPin(this);
				m_cOurRef = 0;
				return m_cOurRef;
				}
			}

		if(m_cOurRef < 1)	{		
	
			// we remove the pin from the map and call the destructor
			m_pMultiIO->DeleteOutputPin(this);
		}
	}
	
	return(ULONG) m_cOurRef;
} // NonDelegatingRelease