#ifndef BASEMULTI_H
	#include "basemulti.h"
	#define BASEMULTI_H
#endif

	 //________________________________________
	//	Output Pin definition 
   //______

CBaseMultiIOOutputPin::CBaseMultiIOOutputPin(
    TCHAR *pObjectName, CBaseMultiIOFilter *pFilter, HRESULT * phr, LPCWSTR pName):
	CBaseOutputPin(pObjectName, pFilter, &pFilter->m_csMultiIO, phr, pName), pMultiIO(pFilter)
	{
		DbgLog((LOG_TRACE,2,TEXT("CBaseMultiIOOutputPin::CBaseMultiIOOutputPin")));		
	}

CBaseMultiIOOutputPin::~CBaseMultiIOOutputPin()
{
	if (position) {
		position->Release();
		position = NULL;
	}
}

// overriden to expose IMediaPosition and IMediaSeeking control interfaces
STDMETHODIMP CBaseMultiIOOutputPin::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
	if (riid == IID_IMediaPosition || riid == IID_IMediaSeeking) {

		// create a PosPassThru object to work with the first input pin
		if (position == NULL) {
			HRESULT hr = CreatePosPassThru(GetOwner(), FALSE,
				(IPin*)m_pMultiIO->inputPins.front(), &position);
			if (FAILED(hr))	{
				return hr;
			}
		}

		return position->QueryInterface(riid, ppv);

	} else {
		return CBaseOuputPin::NonDelegatingQueryInterface(riid, ppv);
	}
}


HRESULT CBaseMultiIOOutputPin::CheckMediaType(const CMediaType *pmt)
{
	return m_pMultiIO->CheckOutputType(pmt);
}

HRESULT CBaseMultiIOOutputPin::SetMediaType(const CMediaType *pmt)
{
	HRESULT	hr = CBaseOutputPin::SetMediaType(pmt);
	if (FAILED(hr))	{
		return hr;
	}
	return m_pMultiIO->SetOutputType(pmt);
}

HRESULT CBaseMultiIOOutputPin::CompleteConnect(IPin *pReceivePin)
{
	HRESULT hr = CBaseOutputPin::CompleteConnect(pReceivePin);
	if (FAILED(hr))	{
		return hr;
	}
	return m_pMultiIO->CompleteOutputConnect(pReceivePin);
}

HRESULT CBaseMultiIOOutputPin::BreakConnect()
{
	HRESULT hr = CBaseOutputPin::BreakConnect();
	if (FAILED(hr))	{
		return hr;
	}
	return m_pMultiIO->BreakOutputConnect();
}

HRESULT CBaseMuxOutputPin::GetMediaType(int i, CMediaType *pmt)
{
	return m_pMultiIO->GetMediaType(i, pmt);
}

HRESULT CBaseMuxOutputPin::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProps)
{
	return m_pMultiIO->DecideBufferSize(pAlloc, pProps);
}



	 //________________________________________
	//	Input Pin definition 
   //______

CBaseMultiIOInputPin::CBaseMultiIOInputPin(
    TCHAR *pObjectName, CBaseMultiIOFilter *pFilter, HRESULT * phr, LPCWSTR pName):
	CBaseInputPin(pObjectName, pFilter, &pFilter->m_csMultiIO, phr, pName), pMultiIO(pFilter)
	{
		DbgLog((LOG_TRACE,2,TEXT("CBaseMultiIOInputPin::CBaseMultiIOInputPin")));		
	}

CBaseMultiIOInputPin::~CBaseMultiIOInputPin()
{
}

STDMETHODIMP CBaseMultiIOInputPin::BeginFlush()
{
	CAutoLock lock(&m_pMultiIO->m_csMultiIO);

	if(FAILED(m_pMultiIO->CheckOutputConnection()))	{
		return VFW_E_NOT_CONNECTED;
	}
    
	HRESULT	hr = CBaseInputPin::BeginFlush();
	if (FAILED(hr)) return hr;

	return NOERROR;
}

STDMETHODIMP CBaseMultiIOInputPin::EndFlush()
{
	CAutoLock lock(&m_pMultiIO->m_csMultiIO);

	if(FAILED(m_pMultiIO->CheckOutputConnection()))	{
		return VFW_E_NOT_CONNECTED;
	}

	HRESULT hr = CBaseInputPin::EndFlush();
	if (FAILED(hr)) return hr;

	// nothing more so far....
	return NOERROR;
}

STDMETHODIMP CBaseMultiIOInputPin::EndOfStream()
{
	HRESULT hr = NOERROR;
    // we have no queued data (otherwise we would have to handle the queue first) {
	if(hr = SUCCEEDED(m_pMultiIO->CheckOutputConnection()))	{
        hr = m_pOutput->DeliverEndOfStream();
    }

	return hr;
}

// we don't hold the sample or anything, just pass it to the base class's Receive
STDMETHODIMP CBaseMultiIOInputPin::Receive(IMediaSample *pMediaSample)
{
	HRESULT hr = NOERROR;
	CAutoLock lock(&m_pMultiIO->m_csMultiIO);
	ASSERT(pSample);

    // check if all is well with the base class
    hr = CBaseInputPin::Receive(pSample);
    if (S_OK == hr) {
        hr = m_pMultiIO->Receive(pSample);
    }
    return hr;
}

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

HRESULT CBaseMuxInputPin::CheckMediaType(const CMediaType *pmt)
{
	// pass it over to filter class
	return m_pMultiIO->CheckInputType(pmt);
}

HRESULT CBaseMuxInputPin::SetMediaType(const CMediaType *pmt)
{
	HRESULT hr = CBaseInputPin::SetMediaType(pmt);
	if (FAILED(hr))	{
		return hr;
	}
	else	{
		return m_pMultiIO->SetInputType(this, pmt);
	}
}

HRESULT CBaseMuxInputPin::CompleteConnect(IPin *pReceivePin)
{
	HRESULT	hr = CBaseInputPin::CompleteConnect(pReceivePin);
	if (FAILED(hr))	{
		return hr;
	}

	// check if all input pins are connected. if so add an extra one.
	return m_pMultiIO->CompleteInputConnect(this, pReceivePin);
}

HRESULT CBaseMuxInputPin::BreakConnect()
{
	// pass the responsibility to our filter class
	return m_pMultiIO->BreakInputConnect(this);

	HRESULT hr = CBaseInputPin::BreakConnect();
	if (FAILED(hr))	{
		return hr;
	}
}



	 //________________________________________
	//	Filter definitions
   //______

CBaseMultiIOFilter::CBaseMultiIOFilter(
	LPCTSTR pName, LPUNKNOWN pUnk, HRESULT *phr, REFCLSID clsid) :
	CBaseFilter(pName, pUnk, &m_csMultiIO, clsid, phr)
		//, abort(false),	streaming(false),eos(false),	output(NULL)
{
	// we will create starting pins in the constructor of derived class, unlike
	// with CTransformFilter class, in CTransformFilter :: GetPin()

}

CBaseMux::~CBaseMux()
{
	// delete all pins we might have, input and output
	RemovePins();
}

int CBaseMux::RemovePins()
{
	// delete all input pins
	for (unsigned int i=0; i<inputPins.size(); i++) {
		CBaseMuxInputPin *pin = inputPins[i];
		if (pin) {
			delete pin;
			pins[i] = NULL;
		}
	}
	inputPins.clear();
/*
	// delete all output pins
	for (unsigned int i=0; i<outputPins.size(); i++) {
		CBaseMuxInputPin *pin = outputPins[i];
		if (pin) {
			delete pin;
			outputPins[i] = NULL;
		}
	}
	outputPins.clear();
*/
	return 0;
}

int CBaseMux::GetPinCount()
{
	CAutoLock	lock(&m_csMultiIO);
	return inputPins.size() + outputPins.size();
}

CBasePin *CBaseMux::GetPin(int n)
{
	CAutoLock	lck(&lock_filter);

	// output pin
	if (output) {
		if (n == 0) return output;
		n -= 1;
	}

	// input pins
	if (n < 0 || n >= (int)inputPins.size()) return NULL;
	return inputPins[n];
}


/*
CBasePin *CBaseMux::GetPin(int n)
{
	CAutoLock	lck(&lock_filter);

	// output pins
	if(n >= 0 && n < outputPins.size())	{
		return outputPins[n];
	}
	// input pins
	if(n >= outputPins.size() && n < inputPins.size() + outputPins.size())	{
		return inputPins[n-outputPins.size()];
	}
	return NULL;

}
*/


//int CBaseMux::CreateInputPin(CBaseMultiIOInputPin **pin, LPCWSTR name)
int CBaseMux::CreatePin(CBaseMultiIOInputPin **pin, LPCWSTR name)
{
	// the derived class migh need to override this method
	// to provide custom pins

	if (!pin) return -1;

	HRESULT hr = NOERROR;
	CBaseMultiIOInputPin *new_pin = 
		new CBaseMultiIOInputPin(L"MultiIOInputPin", this, &hr, name);

	if (!new_pin) {
		*pin = NULL;
		return -1;
	}

	*pin = new_pin;
	return 0;
}

//int CBaseMux::AddInputPin()
int CBaseMux::AddPin()
{
	CBaseMultiIOInputPin *pin = NULL;
	WCHAR name[1024];

	// prepare a name for the pin
	_swprintf(name, L"In %d", (int)pins.size());

	int ret = CreatePin(&pin, name);
	if (ret < 0) return -1;

	// and append it into the list
	pin->index = (int)inputPins.size();		
	inputPins.push_back(pin);

	return 0;
}

/*
int CBaseMux::CreateOutputPin(CBaseMultiIOInputPin **pin, LPCWSTR name)
{
	// the derived class migh need to override this method
	// to provide custom pins

	if (!pin) return -1;

	HRESULT hr = NOERROR;
	CBaseMultiIOInputPin *new_pin = 
		new CBaseMultiIOInputPin(L"MultiIOOutputPin", this, &hr, name);

	if (!new_pin) {
		*pin = NULL;
		return -1;
	}

	*pin = new_pin;
	return 0;
}

int CBaseMux::AddOutputPin()
{
	CBaseMultiIOInputPin *pin = NULL;
	WCHAR name[1024];

	// prepare a name for the pin
	_swprintf(name, L"Out %d", (int)outputPins.size());

	int ret = CreatePin(&pin, name);
	if (ret < 0) return -1;

	// and append it into the list
	pin->index = (int)outputPins.size();		
	outputPins.push_back(pin);

	return 0;
}
*/


HRESULT CBaseMux::CompleteInputConnect(CBaseMultiIOInputPin *pin, IPin *pReceivePin)
{
	// if all input pins are connected, we should add a new one
	CAutoLock lock(&m_pMultiIO);

	if (AllInConnected()) {
		AddInputPin();
	}

	return NOERROR;
}

HRESULT CBaseMux::CompleteOutputConnect(IPin *pReceivePin)
{
	/*
	// if all output pins are connected, we should add a new one
	CAutoLock lock(&m_pMultiIO);

	if (AllOutConnected()) {
		AddOutputPin();
	}
*/
	HRESULT		hr;
	
	// we require the output pins to expose IStream interface
	hr = pReceivePin->QueryInterface(IID_IStream, (void**)&outstream);
	if (FAILED(hr)) return E_FAIL;

	return NOERROR;
}


//bool CBaseMux::AllInConnected()
bool CBaseMux::AllConnected()
{
	// if we don't have any input pins, we create a new one
	if (inputPins.size() == 0) return true;

	for (unsigned int i=0; i<inputPins.size(); i++) {
		CBaseMultiIOInputPin *pin = pins[i];
		if (pin->IsConnected() == FALSE) return false;
	}

	return true;
}

// ovo je bezvezna metoda i suvisna stovise
bool CBaseMux::AllDisconnected()
{
	// if we don't have any pins, we behave like all of them are connected
	// = we should create a new one
	if (inputPins.size() == 0) return true;

	for (unsigned int i=0; i<inputPins.size(); i++) {
		CBaseMuxInputPin *pin = pins[i];
		if (pin->IsConnected()) return false;
	}

	return true;
}



/*
bool CBaseMux::AllOutConnected()
{
	// if we don't have any pins, we create a new one
	if (outputPins.size() == 0) return true;

	for (unsigned int i=0; i<outputPins.size(); i++) {
		CBaseMultiIOOutputPin	*pin = pins[i];
		if (pin->IsConnected() == FALSE) return false;
	}

	return true;
}
*/


HRESULT CBaseMux::BreakInputConnect(CBaseMuxInputPin *pin)
{
	// to be overriden
	return NOERROR;
}

HRESULT CBaseMux::BreakOutputConnect()
{
	// get rid of the writer obejct
	outstream = NULL;
	return NOERROR;
}


	//____________________________________________________
   //	Pins directly related stuff ends here
  //_____________________



// pure virtual
/*HRESULT CBaseMux::GetMediaType(int pos, CMediaType *pmt)
{
	if (pos < 0) return E_INVALIDARG;
	if (pos > 0) return VFW_S_NO_MORE_ITEMS;

	// This is a very basic implementation.
	// If you need more specific options, you should consider
	// overrriding this method

	pmt->majortype = MEDIATYPE_Stream;
	pmt->subtype = MEDIASUBTYPE_None;
	pmt->formattype = MEDIASUBTYPE_None;
	pmt->lSampleSize = 512 * 1024;

	return NOERROR;
}
*/

// pure virtual
/*HRESULT CBaseMux::CheckInputType(const CMediaType *pmt)
{
	// by default we support everything
	return NOERROR;
}
*/

// called by the pin
HRESULT CBaseMux::SetInputType(CBaseMuxInputPin *pin, const CMediaType *pmt)
{
	// no big deal...
	return NOERROR;
}

HRESULT CBaseMux::CheckOutputType(const CMediaType *pmt)
{
	return NOERROR;
}

HRESULT CBaseMux::SetOutputType(const CMediaType *pmt)
{
	return NOERROR;
}


// will be pure virtual
//HRESULT CBaseMux::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProp)
/*{
	ALLOCATOR_PROPERTIES	act;
	HRESULT					hr;

	// by default we do something like this...
	pProp->cbAlign		= 1;
	pProp->cBuffers	= 1;
	pProp->cbBuffer	= output->CurrentMediaType().lSampleSize;
	pProp->cbPrefix	= 0;

	hr = pAlloc->SetProperties(pProp, &act);
	if (FAILED(hr)) return hr;

	// make sure the allocator is OK with it.
	if ((pProp->cBuffers > act.cBuffers)  ||
		(pProp->cbBuffer > act.cbBuffer) ||
		(pProp->cbAlign > act.cbAlign)) 
		return E_FAIL;

	return NOERROR;
}
*/


// next two methods are called in StartStreaming and StopStreaming, respectively
int CBaseMux::OnStartStreaming()
{
	// to be overriden
	return 0;
}

int CBaseMux::OnStopStreaming()
{
	// to be overriden
	return 0;
}


STDMETHODIMP CBaseMux::Stop()
{
    CAutoLock lck(&m_csFilter);
    if (m_State == State_Stopped) {
        return NOERROR;
    }

    ASSERT(m_pInput == NULL || m_pOutput != NULL);
    if (m_pInput == NULL || m_pInput->IsConnected() == FALSE ||
        m_pOutput->IsConnected() == FALSE) {
                m_State = State_Stopped;
                m_bEOSDelivered = FALSE;
                return NOERROR;
    }

    ASSERT(m_pInput);
    ASSERT(m_pOutput);

    // decommit the input pin before locking or we can deadlock
    m_pInput->Inactive();

    // synchronize with Receive calls

    CAutoLock lck2(&m_csReceive);
    m_pOutput->Inactive();

    // allow a class derived from CTransformFilter
    // to know about starting and stopping streaming

    HRESULT hr = StopStreaming();
    if (SUCCEEDED(hr)) {
	// complete the state transition
	m_State = State_Stopped;
	m_bEOSDelivered = FALSE;
    }
    return hr;
}

STDMETHODIMP CBaseMux::Pause()
{
    CAutoLock		lck(&lock_filter);
    FILTER_STATE	OldState = m_State;

    // Make sure there really is a state change
	if (m_State == State_Paused){	
		return NOERROR;
	}

	// If we are not totaly connected when we are
    // asked to pause we deliver an end of stream to the downstream filter.
    // This makes sure that it doesn't sit there forever waiting for
    // samples which we cannot ever deliver without an input connection.

    else if ( AllDisconnected() ) {
        if (m_pOutput && m_bEOSDelivered == FALSE) {
            m_pOutput->DeliverEndOfStream();
            m_bEOSDelivered = TRUE;
        }
        m_State = State_Paused;
    }

	// We may have an input connection but no output connection
    // However, if we have an input pin we do have an output pin
    else if (m_pOutput->IsConnected() == FALSE) {
        m_State = State_Paused;
    }

	else {
	if (m_State == State_Stopped) {
	    // allow a derived class
	    // to know about starting and stopping streaming
            CAutoLock lck2(&m_csReceive);
	    hr = StartStreaming();
	}
	if (SUCCEEDED(hr)) {
	    hr = CBaseFilter::Pause();
	}
    }

    m_bSampleSkipped = FALSE;
    m_bQualityChanged = FALSE;
    return hr;
}
/*
STDMETHODIMP CBaseMux::Run(REFERENCE_TIME StartTime)
{
    CAutoLock		lck(&lock_filter);
    FILTER_STATE	OldState = m_State;

    if (m_State == State_Running) return NOERROR;

    // just run the filter
    HRESULT hr = CBaseFilter::Run(StartTime);
    if (FAILED(hr)) return hr;

    return NOERROR;
}
*/


// we have to create a stream for each pin
void CBaseMux::StartStreaming()
{
	// scan through the pins
	for (unsigned int i=0; i<pins.size(); i++) {
		CBaseMuxInputPin	*pin = pins[i];

		// by default there is no stream associated with the pin
		pin->stream = NULL;

		if (pin->IsConnected()) {

			CMuxInterStream	*stream = NULL;
			CMediaType		&mt = pin->CurrentMediaType();
			int				ret;

			// apped the new stream
			ret = interleaver.AddStream(&stream);
			if (ret == 0) {
				pin->stream = stream;
				pin->stream->active = true;
				pin->stream->data = (void*)pin;	// associate it with the pin

				// ask the derived class if this stream should be interleaved
				pin->stream->is_interleaved = IsInterleaved(&mt);
			}
		}
	}

	// reset the output byte counter
	bytes_written = 0;

	// and then let the derived class do something about it
	OnStartStreaming();
}

void CBaseMux::StopStreaming()
{
	// let the derived class know
	OnStopStreaming();

	// and remove all streams from the interleaver
	interleaver.Clear();
	for (unsigned int i=0; i<pins.size(); i++) {
		CBaseMuxInputPin	*pin = pins[i];
		pin->stream = NULL;
	}
}

