#ifndef MATH_H
	#include <math.h>
	#define MATH_H
#endif

#pragma warning(disable:4312)

#ifndef THRESHDIFF_H
	#include "threshdiff.h"
#endif


CThreshDiff :: CThreshDiff( TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr ) :
	  CBaseMultiIOFilter( tszName, punk, CLSID_THRESHDIFF, 0, 0, 0, 0 ){

			InitializePins();

			// the only output media subtype is MEDIASUBTYPE_8BitMonochrome_internal...
			// Our output MediaType is defined as follows:
			  outputMediaType = new CMediaType();
			  outputMediaType->InitMediaType();
			  outputMediaType->AllocFormatBuffer(sizeof(FORMAT_VideoInfo));

              outputMediaType->SetType(&MEDIATYPE_Video);
			  outputMediaType->SetSubtype(&MEDIASUBTYPE_8BitMonochrome_internal);			  
			  outputMediaType->SetFormatType(&FORMAT_VideoInfo);
			  outputMediaType->bFixedSizeSamples = TRUE;
			  outputMediaType->SetTemporalCompression(FALSE);
			  outputMediaType->cbFormat = sizeof(VIDEOINFOHEADER);

			  HRESULT ( CThreshDiff::*fp )( IMediaSample** pSamples ) =  &CThreshDiff::Process;
			  pSampleCollector = new CSampleCollector(this, fp);

			  // event handles
			  hHasDelivered[0] = CreateEvent(
							NULL,		//Security descriptor - default
							TRUE,		//manual reset
							TRUE,		//initial state, because we want the bgd
										//thread to wait when the thread is created
							NULL		//object name
							);
			  hHasDelivered[1] = CreateEvent(NULL, TRUE, TRUE, NULL);
			  hSafeToDecommit = CreateEvent(NULL, TRUE, TRUE, NULL);

			  // init params
			  m_params.threshold = 25;
	  }


CThreshDiff :: ~CThreshDiff(void){
	// kill the pins
	outputPin->Release();
	backgroundPin->Release();
	scenePin->Release();

	IncrementPinVersion();

	delete outputMediaType;
	delete pSampleCollector;

	CloseHandle( hHasDelivered[0] );
	CloseHandle( hHasDelivered[1] );
}

HRESULT CThreshDiff :: InitializeProcessParams(VIDEOINFOHEADER* pVih)	{
	bitsperpixel_8u_C1 = 8;
//	bitsperpixel_8u_C3 = 24;
	stride_8u_C1 = (pVih->bmiHeader.biWidth*bitsperpixel_8u_C1)%IPPalignment;
//	stride_8u_C3 = (pVih->bmiHeader.biWidth*bitsperpixel_8u_C3)%IPPalignment;
	step_8u_C1 = pVih->bmiHeader.biWidth*bitsperpixel_8u_C1/8+stride_8u_C1;
//	step_8u_C3 = pVih->bmiHeader.biWidth*bitsperpixel_8u_C3/8+stride_8u_C3;
//	step_32f_C1 = (step_8u_C1 - step_8u_C1%32)*4;
	frameSize.height = pVih->bmiHeader.biHeight;
	frameSize.width = pVih->bmiHeader.biWidth;

	return NOERROR;
}

HRESULT CThreshDiff :: InitializePins()	{

	CAutoLock lock(&m_csMultiIO);
	
	// important to initialize to NOERROR, because it doesn't get set in the parent class's
	// constructor unless it fails
	HRESULT hr = NOERROR;

	outputPin = new CPinOutput((CBaseMultiIOFilter*)this, m_pLock, &hr, L"Output");
	if (FAILED(hr) || outputPin == NULL) {
		delete outputPin;
		return hr;
	}
	outputPin->AddRef();

	backgroundPin = new CPinInputBackground(this, m_pLock, &hr, L"BackgroundIn");
	if (FAILED(hr) || backgroundPin == NULL) {
		delete backgroundPin;
		return hr;
	}
	backgroundPin->AddRef();

	scenePin = new CPinInputScene(this, m_pLock, &hr, L"SceneIn");
	if (FAILED(hr) || scenePin == NULL) {
		delete scenePin;
		return hr;
	}
	scenePin->AddRef();

	IncrementPinVersion();

	return S_OK;
}

CBasePin* CThreshDiff :: GetPin(int n)	{

	if ((n >= GetPinCount()) || (n < 0))
		return NULL;
	switch (n)	{
		case 0: return backgroundPin;
		case 1: return scenePin;
		case 2: return outputPin;
	}
	return NULL;
}

int CThreshDiff::GetPinCount()
{
	ASSERT(outputPin != NULL || scenePin != NULL || backgroundPin != NULL);
	return (unsigned int)(3);
}


HRESULT CThreshDiff :: GetMediaType(int iPosition, CMediaType *pOutputMediaType, CBaseMultiIOOutputPin* caller)	{

	CAutoLock lock(&m_csMultiIO);

	// This method should be called only if at least one input pin is connected
   if( backgroundPin->IsConnected() == FALSE && scenePin->IsConnected() == FALSE )	{
        return E_UNEXPECTED;
   }

    // This should never happen
    if (iPosition < 0) {
        return E_INVALIDARG;
    }

    // Do we have more items to offer
    if (iPosition > 0) {
        return VFW_S_NO_MORE_ITEMS;
    }

	if( backgroundPin->IsConnected() )	{
		HRESULT hr = backgroundPin->ConnectionMediaType(pOutputMediaType);
		if (FAILED(hr))		{
			return hr;
		}
	} else	{
		// scene pin must be connected if we're here
		HRESULT hr = scenePin->ConnectionMediaType(pOutputMediaType);
		if (FAILED(hr))		{
			return hr;
		}		
	}

    CheckPointer(pOutputMediaType,E_POINTER);
	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) pOutputMediaType->Format();

	// with same DIB width and height but only one (monochrome) channel
	pVih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pVih->bmiHeader.biPlanes = 1;
	pVih->bmiHeader.biBitCount = 8;
	pVih->bmiHeader.biCompression = BI_RGB;	// ?? no alternative, we treat it as uncompressed RGB
	pVih->bmiHeader.biSizeImage = DIBSIZE(pVih->bmiHeader);	
	pVih->bmiHeader.biClrImportant = 0;
	//SetRectEmpty(&(pVih->rcSource));
	//SetRectEmpty(&(pVih->rcTarget));

	// Our output MediaType is defined as follows:
	pOutputMediaType->SetType(outputMediaType->Type());
	pOutputMediaType->SetSubtype(outputMediaType->Subtype());
	pOutputMediaType->SetFormatType(&outputMediaType->formattype);
	pOutputMediaType->bFixedSizeSamples = outputMediaType->bFixedSizeSamples;
	pOutputMediaType->SetTemporalCompression(outputMediaType->bTemporalCompression);
	pOutputMediaType->lSampleSize = DIBSIZE(pVih->bmiHeader);
	pOutputMediaType->cbFormat = sizeof(VIDEOINFOHEADER);

	// a good time to fill the rest of outputMediaType
	outputMediaType->SetSampleSize( DIBSIZE(pVih->bmiHeader) );

    return NOERROR;
}



HRESULT CThreshDiff :: DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProp, CBaseMultiIOOutputPin* caller)	{

    return NOERROR;
}


HRESULT CThreshDiff :: CheckInputType( const CMediaType *mtIn )	{


	if ((mtIn->majortype != MEDIATYPE_Video) ||
		(mtIn->subtype != MEDIASUBTYPE_8BitMonochrome_internal) ||
		(mtIn->formattype != FORMAT_VideoInfo) ||		// is this really necessary?
		(mtIn->cbFormat < sizeof(VIDEOINFOHEADER)))		// do all CMediaTypes have a VIH format?
	{
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	// reinterpret cast is safe because we already checked formattype and cbFormat
	VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(mtIn->pbFormat);

	return S_OK;

}

// the CsceneInputPin is responsible for delivering the notifications to the output pin;
// it does that directly, bypassing CThreshDiff's related methods
HRESULT CThreshDiff::EndOfStream( CBaseMultiIOInputPin* caller )	{
	return NOERROR;
}

HRESULT CThreshDiff::BeginFlush( CBaseMultiIOInputPin* caller )	{
	return NOERROR;
}

HRESULT CThreshDiff::EndFlush( CBaseMultiIOInputPin* caller )	{
	return NOERROR;
}

HRESULT CThreshDiff::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate, CBaseMultiIOInputPin* caller )	{
	return NOERROR;
}

// Set up our output sample (as in CTransformFilter baseclass)
HRESULT CThreshDiff::InitializeOutputSample(IMediaSample *pSample, IMediaSample **ppOutSample)	{

	// pin whose properties we will take as a basis for our output sample's properties
	CPinInputScene* m_pInput = scenePin;
	CPinOutput* m_pOutput = outputPin;

    IMediaSample *pOutSample;

    // default - times are the same

	DbgLog((LOG_TRACE,0,TEXT("initializeoutputsample 0")));

    AM_SAMPLE2_PROPERTIES* const pProps = m_pInput->SampleProps();
    DWORD dwFlags = m_bSampleSkipped ? AM_GBF_PREVFRAMESKIPPED : 0;

	DbgLog((LOG_TRACE,0,TEXT("initializeoutputsample 1")));

    // This will prevent the image renderer from switching us to DirectDraw
    // when we can't do it without skipping frames because we're not on a
    // keyframe.  If it really has to switch us, it still will, but then we
    // will have to wait for the next keyframe
    if (!(pProps->dwSampleFlags & AM_SAMPLE_SPLICEPOINT)) {
		dwFlags |= AM_GBF_NOTASYNCPOINT;
    }

	DbgLog((LOG_TRACE,0,TEXT("initializeoutputsample 2")));

    ASSERT(m_pOutput->m_pIPPAllocator != NULL);
    HRESULT hr = m_pOutput->m_pIPPAllocator->GetBuffer(
             &pOutSample
             , pProps->dwSampleFlags & AM_SAMPLE_TIMEVALID ?
                   &pProps->tStart : NULL
             , pProps->dwSampleFlags & AM_SAMPLE_STOPVALID ?
                   &pProps->tStop : NULL
             , dwFlags
         );

	DbgLog((LOG_TRACE,0,TEXT("initializeoutputsample 2.1")));
    *ppOutSample = pOutSample;
    if (FAILED(hr)) {
		DbgLog((LOG_TRACE,0,TEXT("initializeoutputsample 2.2 failed!")));
        return hr;
    }

	DbgLog((LOG_TRACE,0,TEXT("initializeoutputsample 3")));

    ASSERT(pOutSample);
    IMediaSample2 *pOutSample2;
    if (SUCCEEDED(pOutSample->QueryInterface(IID_IMediaSample2,
                                             (void **)&pOutSample2))) {
        //  Modify it 
        AM_SAMPLE2_PROPERTIES OutProps;
        EXECUTE_ASSERT(SUCCEEDED(pOutSample2->GetProperties(
            FIELD_OFFSET(AM_SAMPLE2_PROPERTIES, tStart), (PBYTE)&OutProps)
        ));
        OutProps.dwTypeSpecificFlags = pProps->dwTypeSpecificFlags;
        OutProps.dwSampleFlags =
            (OutProps.dwSampleFlags & AM_SAMPLE_TYPECHANGED) |
            (pProps->dwSampleFlags & ~AM_SAMPLE_TYPECHANGED);
        OutProps.tStart = pProps->tStart;
        OutProps.tStop  = pProps->tStop;
        OutProps.cbData = FIELD_OFFSET(AM_SAMPLE2_PROPERTIES, dwStreamId);
        hr = pOutSample2->SetProperties(
            FIELD_OFFSET(AM_SAMPLE2_PROPERTIES, dwStreamId),
            (PBYTE)&OutProps
        );
        if (pProps->dwSampleFlags & AM_SAMPLE_DATADISCONTINUITY) {
            m_bSampleSkipped = FALSE;
        }
        pOutSample2->Release();
    } else {
        if (pProps->dwSampleFlags & AM_SAMPLE_TIMEVALID) {
            pOutSample->SetTime(&pProps->tStart,
                                &pProps->tStop);
        }
        if (pProps->dwSampleFlags & AM_SAMPLE_SPLICEPOINT) {
            pOutSample->SetSyncPoint(TRUE);
        }
        if (pProps->dwSampleFlags & AM_SAMPLE_DATADISCONTINUITY) {
            pOutSample->SetDiscontinuity(TRUE);
            m_bSampleSkipped = FALSE;
        }
        // Copy the media times

        LONGLONG MediaStart, MediaEnd;
        if (pSample->GetMediaTime(&MediaStart,&MediaEnd) == NOERROR) {
            pOutSample->SetMediaTime(&MediaStart,&MediaEnd);
        }
    }
    return S_OK;
}


// registration stuffzis

// setup data - allows the self-registration to work.
const AMOVIESETUP_MEDIATYPE sudPinTypes =
{
  &MEDIATYPE_NULL        // clsMajorType
, &MEDIASUBTYPE_NULL     // clsMinorType
};

const AMOVIESETUP_PIN psudPins[] =
{ { L"Input"            // strName
  , FALSE               // bRendered
  , FALSE               // bOutput
  , FALSE               // bZero
  , TRUE		        // bMany
  , &CLSID_NULL         // clsConnectsToFilter
  , L""                 // strConnectsToPin
  , 1                   // nTypes
  , &sudPinTypes        // lpTypes
  }
, { L"Output"           // strName
  , FALSE               // bRendered
  , TRUE                // bOutput
  , FALSE               // bZero
  , TRUE                // bMany
  , &CLSID_NULL         // clsConnectsToFilter
  , L""                 // strConnectsToPin
  , 1                   // nTypes
  , &sudPinTypes        // lpTypes
  }
};

const AMOVIESETUP_FILTER sudExtractY =
{ 
	&CLSID_THRESHDIFF				// clsID
	, FILTER_NAME				// strName
	, MERIT_DO_NOT_USE			// dwMerit
	, 2							// nPins
	, psudPins					// lpPin
};

REGFILTER2 rf2FilterReg = {
    1,                    // Version 1 (no pin mediums or pin category).
    MERIT_DO_NOT_USE,     // Merit.
    3,                    // Number of pins.
    psudPins              // Pointer to pin information.
};

// Class factory template - needed for the CreateInstance mechanism to
// register COM objects in our dynamic library
CFactoryTemplate g_Templates[]=
    {   
		{
			FILTER_NAME,
			&CLSID_THRESHDIFF,
			CThreshDiff::CreateInstance,
			NULL,
			&sudExtractY
		}
    };

int g_cTemplates = sizeof(g_Templates)/sizeof(g_Templates[0]);


// Provide the way for COM to create a filter object
CUnknown* WINAPI CThreshDiff :: CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
	CheckPointer(phr,NULL);
	
	CThreshDiff* pNewObject = new CThreshDiff( NAME("ThreshedDiff"), punk, phr );

	if (pNewObject == NULL)
	{
		*phr = E_OUTOFMEMORY;
	}

	return pNewObject;
} 


// DllEntryPoint
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, 
                      DWORD  dwReason, 
                      LPVOID lpReserved)	{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}


// Exported entry points for registration and unregistration 
// (in this case they only call through to default implementations).

// Self registration
// called  when the user runs the Regsvr32.exe tool
// we'll use FilterMapper2 interface for managing registration
STDAPI DllRegisterServer()
{
	HRESULT hr;
	IFilterMapper2 *pFM2 = NULL;

	hr = AMovieDllRegisterServer2(TRUE);
	if (FAILED(hr))
		return hr;

	hr = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
		IID_IFilterMapper2, (void **)&pFM2);

	if (FAILED(hr))
		return hr;

	// we will create our category, we'll call it "Somehow"
	pFM2->CreateCategory( CLSID_MotionTrackingCategory, MERIT_NORMAL, L"Motion Tracking");

	hr = pFM2->RegisterFilter(
		CLSID_THRESHDIFF,						// Filter CLSID. 
		FILTER_NAME,							// Filter name.
		NULL,								    // Device moniker. 
		&CLSID_MotionTrackingCategory,			// category.
		FILTER_NAME,							// Instance data.
		&rf2FilterReg							// Pointer to filter information.
		);
	pFM2->Release();
	return hr;

}

 //Self unregistration
 //called  when the user runs the Regsvr32.exe tool with -u command.
STDAPI DllUnregisterServer()
{	
	HRESULT hr;
	IFilterMapper2 *pFM2 = NULL;

	hr = AMovieDllRegisterServer2(FALSE);
	if (FAILED(hr))
		return hr;

	hr = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
		IID_IFilterMapper2, (void **)&pFM2);

	if (FAILED(hr))
		return hr;

	hr = pFM2->UnregisterFilter(&CLSID_MotionTrackingCategory, 
		FILTER_NAME, CLSID_THRESHDIFF);

	pFM2->Release();
	return hr;
}



CThreshDiff :: CSampleCollector :: CSampleCollector(
	CThreshDiff* pFilt,
	HRESULT ( CThreshDiff::*fp )( IMediaSample** pSamples ))	{

	this->pFilt = pFilt;
	fpProcess = fp;

	samples[0] = NULL;
	samples[1] = NULL;
	
	available[0] = false;
	available[1] = false;

	DbgLog((LOG_TRACE,0,TEXT("sample collector constructor")));

}

CThreshDiff :: CSampleCollector ::~CSampleCollector(){

}


HRESULT CThreshDiff :: CSampleCollector :: AddSample(IMediaSample* sample, CPinInputBackground*)	{

	sample->AddRef();
	samples[0] = sample;
	available[0] = TRUE;

	HRESULT hr = NOERROR;
	if(AllAvailable())	{
		DbgLog((LOG_TRACE,0,TEXT("BCKGND add sample")));
		hr = ( pFilt->*fpProcess )( samples );
		// reset the flags and release the samples
		Reset();
		SetEvent(pFilt->hSafeToDecommit);
	}

	return hr;
}

HRESULT CThreshDiff :: CSampleCollector :: AddSample(IMediaSample* sample, CPinInputScene*)	{

	sample->AddRef();
	samples[1] = sample;
	available[1] = TRUE;

	HRESULT hr = NOERROR;
	if(AllAvailable())	{
		// process the samples
		DbgLog((LOG_TRACE,0,TEXT("SCENE add sample")));
		hr = ( pFilt->*fpProcess )( samples );
		// reset the flags and release the samples
		Reset();
		SetEvent(pFilt->hSafeToDecommit);
	}

	return hr;
}

//	bastard version

/*
HRESULT CThreshDiff :: CSampleCollector :: AddSample(IMediaSample* sample, CPinInputBackground*)	{

	if(available[0]) samples[0]->Release();
	samples[0] = sample;
	samples[0]->AddRef();
	available[0] = TRUE;

	HRESULT hr = NOERROR;
	if(AllAvailable())	{
		DbgLog((LOG_TRACE,0,TEXT("BCKGND add sample")));
		hr = ( pFilt->*fpProcess )( samples );
		//Reset();
		SetEvent(pFilt->hSafeToDecommit);
	}

	return NOERROR;
}

HRESULT CThreshDiff :: CSampleCollector :: AddSample(IMediaSample* sample, CPinInputScene*)	{

	samples[1] = sample;

	// check timestamps. we want scene sample at least as new as bacground sample
	REFERENCE_TIME pTimeStart[2];
    REFERENCE_TIME pTimeEnd[2];

	samples[0]->GetTime(&pTimeStart[0], &pTimeEnd[0]);	// bkgnd
	samples[1]->GetTime(&pTimeStart[1], &pTimeEnd[1]);	// scene
	if(pTimeStart[1]<pTimeStart[0])	{
		available[1] = FALSE;
		return NOERROR;
	}

	samples[1]->AddRef();
	//available[1] = TRUE;	no need for the flag, actually

	HRESULT hr = NOERROR;
	if(AllAvailable())	{
		// process the samples
		DbgLog((LOG_TRACE,0,TEXT("SCENE add sample")));
		hr = ( pFilt->*fpProcess )( samples );
		//Reset();
		available[1] = FALSE;
		samples[1]->Release();
		SetEvent(pFilt->hSafeToDecommit);
	}

	return hr;
}
*/


bool CThreshDiff :: CSampleCollector :: AllAvailable(void)	{

	if( !available[0] ) return false;
	if( !available[1] ) return false;
	return true;
}

void CThreshDiff :: CSampleCollector :: Reset(void)	{	

	CAutoLock lock(&pFilt->m_csReceive);
	if(available[0])	samples[0] -> Release();
	if(available[1])	samples[1] -> Release();
	available[0] = false;
	available[1] = false;
}

HRESULT CThreshDiff ::Process(IMediaSample** samples)	{

	HRESULT hr = NOERROR;

	IppStatus status;
	BYTE* pBackground_8u_C1;
	BYTE* pScene_8u_C1;
	BYTE* pOut_8u_C1;

	// create a new media sample based on the sample from scene input
	IMediaSample *pOutSample;

	DbgLog((LOG_TRACE,0,TEXT("before initializeoutputsample")));

	hr = InitializeOutputSample(samples[1], &pOutSample);
	if(FAILED(hr)) return hr;
	
	// get the addresses of the actual data (buffers)
	samples[0]->GetPointer( &pBackground_8u_C1 );
	samples[1]->GetPointer( &pScene_8u_C1 );
	pOutSample->GetPointer( &pOut_8u_C1 );

	DbgLog((LOG_TRACE,0,TEXT("pointer %d"), pOut_8u_C1));

	// absolute difference of the two frames
	status = ippiAbsDiff_8u_C1R( pBackground_8u_C1, step_8u_C1, pScene_8u_C1,
				step_8u_C1, pOut_8u_C1, step_8u_C1, frameSize );
	// let's say 200. whatever
	status = ippiThreshold_LTValGTVal_8u_C1IR( pOut_8u_C1, step_8u_C1, frameSize,
				m_params.threshold, 0, m_params.threshold, 200);
	
	DbgLog((LOG_TRACE,0,TEXT("ajmo na deliver")));

	// it will get addreffed inside if needed
	hr = outputPin->Deliver(pOutSample);
	return hr;

}


STDMETHODIMP CThreshDiff::Stop()	{

	DbgLog((LOG_TRACE,0,TEXT("Stop: waiting")));
	// wait for both the receives needed to produce the output.
	// claim the section before decommiting input pins immediately

	// WaitForSingleObject(hSafeToDecommit, INFINITE);
	//CAutoLock lock2(&m_csReceive);

    CAutoLock lock(&m_csMultiIO);

	  // stop if we are not completely connected
    if (scenePin == NULL || !scenePin->IsConnected() ||
		backgroundPin == NULL || !backgroundPin->IsConnected() ||
		outputPin == NULL || !outputPin->IsConnected())	{
		m_State = State_Stopped;
		m_bEOSDelivered = FALSE;

		return NOERROR;
    }

	HRESULT hr = CBaseFilter::Stop();
	if(FAILED(hr)) return hr;

    return NOERROR;
}


STDMETHODIMP CThreshDiff::Pause()	{

	// we won't let the graph pause unless both input samples arrived
	// lock the receive section
	//DbgLog((LOG_TRACE,0,TEXT("Incative: waiting...")));
	//WaitForSingleObject(hSafeToDecommit, INFINITE);

	CAutoLock lock(&m_csMultiIO);

    HRESULT hr = NOERROR;


	// KO JE TU STAVIO EOX?
    // If we have no input pin or it isn't yet connected then when we are
    // asked to pause we deliver an end of stream to the downstream filter.
    // This makes sure that it doesn't sit there forever waiting for
    // samples which we cannot ever deliver without an input connection.
/*
    if (scenePin == NULL || !scenePin->IsConnected() ||
		backgroundPin == NULL || !backgroundPin->IsConnected())	{

        if (outputPin && m_bEOSDelivered == FALSE) {
            outputPin->DeliverEndOfStream();
            m_bEOSDelivered = TRUE;
        }
        m_State = State_Paused;
    }
*/
    // We may have an input connection but no output connection
    // However, if we have an input pin we do have an output pin
/*    else if (outputPin->IsConnected() == FALSE) {
        m_State = State_Paused;
    }
*/
	if (SUCCEEDED(hr)) {
	    hr = CBaseFilter::Pause();
    }

    m_bSampleSkipped = FALSE;
    m_bQualityChanged = FALSE;
    return hr;
}

STDMETHODIMP CThreshDiff::Run(REFERENCE_TIME tStart)		{

	CAutoLock lock(&m_csMultiIO);

	// remember the stream time offset
	m_tStart = tStart;

	if (m_State == State_Stopped){
		HRESULT hr = Pause();

		if (FAILED(hr)) {
			return hr;
		}
	}
	
	// lock the receive section
	CAutoLock lck2(&m_csReceive);

	DbgLog((LOG_TRACE,0,TEXT("Run: making everything active")));
	
	// notify all pins of the change to active state
	if (m_State != State_Running) {
		int cPins = GetPinCount();
		for (int c = 0; c < cPins; c++) {

			CBasePin *pPin = GetPin(c);

		// Disconnected pins are not activated - this saves pins
		// worrying about this state themselves
	
			if (pPin->IsConnected()) {
				HRESULT hr = pPin->Run(tStart);
				if (FAILED(hr)) {
					return hr;
				}
			}
		}
	}
	else	{
		m_State = State_Running;
	}

    return S_OK;
}