#pragma warning(disable:4312)

#ifndef BASEMULTIINPUTPIN_H
	#include "..\BaseMultiIOFilter\BaseMultiInputPin.h"
#endif

#ifndef BASEMULTIIOFILTER_H
	#include "..\BaseMultiIOFilter\BaseMultiIOFilter.h"
#endif

	 //________________________________________
	//	Input Pin definition 
   //______

CBaseMultiIOInputPin::CBaseMultiIOInputPin(
	CBaseMultiIOFilter* pFilter, CCritSec* pLock, HRESULT* phr, LPCWSTR pName, int nIndex)
	: CBaseInputPin(NAME("Input Pin"), pFilter, pLock, phr, pName),
	m_pMultiIO(pFilter),
	m_pinIndex(nIndex),
	m_cOurRef(0)
	{
		DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOInputPin::CBaseMultiIOInputPin, pin index %d"),
			m_pinIndex));		
	}

CBaseMultiIOInputPin::~CBaseMultiIOInputPin()
{
	DbgLog((LOG_TRACE,0,TEXT("We're in the input pin destructor")));
}

STDMETHODIMP CBaseMultiIOInputPin::BeginFlush()
{
	CAutoLock lock(&m_pMultiIO->m_csMultiIO);

	DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOInputPin::BeginFlush(), pin %d"), m_pinIndex));

	HRESULT hr = CBaseInputPin::BeginFlush();
	if (FAILED(hr)) {
		return hr;
	}

	return m_pMultiIO->BeginFlush(this);

}

STDMETHODIMP CBaseMultiIOInputPin::EndFlush()
{
	CAutoLock lock(&m_pMultiIO->m_csMultiIO);

	HRESULT hr = CBaseInputPin::EndFlush();
	if (FAILED(hr))	{
		return hr;
	}

	return m_pMultiIO->EndFlush(this);
}

STDMETHODIMP CBaseMultiIOInputPin::EndOfStream()
{
	CAutoLock lock(&m_pMultiIO->m_csMultiIO);

	HRESULT hr = CBaseInputPin::EndOfStream();
	if (FAILED(hr))
	{
		return hr;
	}
	return m_pMultiIO->EndOfStream(this);

	return hr;
}

STDMETHODIMP CBaseMultiIOInputPin::NewSegment(
	REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate )	{

	CAutoLock lock(&m_pMultiIO->m_csMultiIO);

	HRESULT hr = CBaseInputPin::NewSegment(tStart, tStop, dRate);
	if (FAILED(hr))
	{
		return hr;
	}
	return m_pMultiIO->NewSegment(tStart, tStop, dRate, this );
}

STDMETHODIMP CBaseMultiIOInputPin::Receive(IMediaSample *pMediaSample)
{
	CAutoLock lock(&m_pMultiIO->m_csReceive);

	HRESULT hr = NOERROR;
	ASSERT(pMediaSample);

    // check if all is well with the base class
    hr = CBaseInputPin::Receive(pMediaSample);

	// here we could pass a call to the base class's Receive method and do sth.

/*    if (S_OK == hr) {
        hr = m_pMultiIO->Receive(pMediaSample, m_pinIndex);
	}
	*/
    return hr;
}

/*
// Check streaming status
HRESULT CBaseMultiIOInputPin::CheckStreaming()
{
    if (!m_pMultiIO->CheckOutputConnection()) {
        return VFW_E_NOT_CONNECTED;
    } else {
        //  Shouldn't be able to get any data if we're not connected!
        ASSERT(IsConnected());

        //  we're flushing
        if (m_bFlushing) {
            return S_FALSE;
        }
        //  Don't process stuff in Stopped state
        if (IsStopped()) {
            return VFW_E_WRONG_STATE;
        }
        if (m_bRunTimeError) {
    	    return VFW_E_RUNTIME_ERROR;
        }
        return S_OK;
    }
}
*/
HRESULT CBaseMultiIOInputPin::CheckMediaType(const CMediaType *pmt)
{
	CAutoLock lock(&m_pMultiIO->m_csMultiIO);

	// pass it over to filter class
	return m_pMultiIO->CheckInputType(pmt);
}

HRESULT CBaseMultiIOInputPin::SetMediaType(const CMediaType *pmt)
{
	//buCAutoLock lock(&m_pMultiIO->m_csMultiIO);

	HRESULT hr = CBaseInputPin::SetMediaType(pmt);
	if (FAILED(hr))	{
		return hr;
	}

	return m_pMultiIO->SetInputType(this, pmt);
}

// we could force reconnections if another media type gets connected and stuff like that
HRESULT CBaseMultiIOInputPin::CompleteConnect(IPin *pReceivePin)
{

	CAutoLock lock(&m_pMultiIO->m_csMultiIO);

	DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOInputPin::CompleteConnect")));

	HRESULT	hr = CBaseInputPin::CompleteConnect(pReceivePin);
	if (SUCCEEDED(hr))	{

		DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOInputPin::CompleteConnect succeeded")));
		// inside the OnConnect we will add additional pins if neccessary

		m_pMultiIO->OnConnect(this);
	}

	DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOInputPin::CompleteConnect succeeded... for real")));

	return hr;
}

HRESULT CBaseMultiIOInputPin::BreakConnect()
{
	CAutoLock lock(&m_pMultiIO->m_csMultiIO);

	HRESULT hr = CBaseInputPin::BreakConnect();
	if (SUCCEEDED(hr))	{
		// pass it through to the filter class

		m_pMultiIO->OnDisconnect(this);
	}
	
	return hr;
}

// as in baseclasses
HRESULT CBaseMultiIOInputPin::PassNotify(Quality q)	{
	// We pass the message on, which means that we find the quality sink
    // for our input pin and send it there

	CAutoLock lock(&m_pMultiIO->m_csMultiIO);

    DbgLog((LOG_TRACE,0,TEXT("Passing Quality notification through transform")));
	// try our sink
    if (m_pQSink!=NULL) {
        return m_pQSink->Notify(m_pFilter, q);
    } else {
        // no sink set, so pass it upstream
        HRESULT hr;
        IQualityControl * pIQC;

        hr = VFW_E_NOT_FOUND;                   // default
        if (m_Connected) {
            m_Connected->QueryInterface(IID_IQualityControl, (void**)&pIQC);

            if (pIQC!=NULL) {
                hr = pIQC->Notify(m_pFilter, q);
                pIQC->Release();
            }
        }
        return hr;
    }	
}


//
// Next two methods are taken and adapted from InfPinTee project
//
// NonDelegatingAddRef
//
// We need override this method so that we can do proper reference counting
// on our input pins. The base class CBasePin does not do any reference
// counting on the pin in RETAIL.
//
// Please refer to the comments for the NonDelegatingRelease method for more
// info on why we need to do this.
//
STDMETHODIMP_(ULONG) CBaseMultiIOInputPin::NonDelegatingAddRef()
{
    CAutoLock lock(&m_pMultiIO->m_csMultiIO);

#ifdef DEBUG
    // Update the debug only variable maintained by the base class
    //m_cRef++;
	InterlockedIncrement(&m_cRef);
    ASSERT(m_cRef > 0);
#endif

    // Now update our reference count
    //m_cOurRef++;
	InterlockedIncrement(&m_cOurRef);
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
STDMETHODIMP_(ULONG) CBaseMultiIOInputPin::NonDelegatingRelease()	{
    CAutoLock lock(&m_pMultiIO->m_csMultiIO);

#ifdef DEBUG
    // Update the debug only variable in CBasePin
    m_cRef--;
	//InterlockedDecrement(&m_cRef);
    ASSERT(m_cRef >= 0);
#endif

    // Now update our reference count
    m_cOurRef--;
	//InterlockedDecrement(&m_cOurRef);
    ASSERT(m_cOurRef >= 0);

    // if the reference count on the object has gone to one, this means that noone
	// other than our filter has it addref'fed

    // Also, when the ref count drops to 0, it really means that our
    // filter that is holding one ref count has released it so we
    // should delete the pin as well.

	
	if(m_cOurRef <= 1)	{
		if(m_cOurRef == 1)	{		
			// this means othat only our filter holds the pin, i.e. it is not connected
	
			// we kill this pin if there is another unconnected pin beyond the minimum of pins
			// we set in the filter class constructor.
			// note that we want one free pin at all times. Ovride if u need mo
			if( m_pMultiIO->m_inputPins.size() > m_pMultiIO->m_nrInitialIn &&
				m_pMultiIO->m_inputPins.size() - m_pMultiIO->AllInputConnected() > 1 )	{
	
#ifdef DEBUG
				m_cRef = 0;
#endif

				m_pMultiIO->DeleteInputPin(this);
				m_cOurRef = 0;
				return m_cOurRef;
				}
			}

		if(m_cOurRef < 1)	{		
	
			// we remove the pin from the map and call the destructor
			m_pMultiIO->DeleteInputPin(this);
		}
	}
	
	return(ULONG) m_cOurRef;

} // NonDelegatingRelease
