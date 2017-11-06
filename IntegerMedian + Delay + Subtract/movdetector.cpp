#ifndef MATH_H
	#include <math.h>
	#define MATH_H
#endif

#pragma warning(disable:4312)

#ifndef MOVDETECTOR_H
	#include "movdetector.h"
#endif

#ifndef PINS_H
	#include "pins.h"
#endif

#include "..\BaseMultiIOFilter\BaseMultiInputPin.h"
#include "..\BaseMultiIOFilter\BaseMultiOutputPin.h"



CMovDetectorFilter :: CMovDetectorFilter( TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr ) :
	  CBaseMultiIOFilter( tszName, punk, CLSID_MOVDETECTOR, 0, 0, 0, 0 ),
	  additionalAllocated(false)
{
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

	// init params
	m_params.windowSize = 201;
	m_params.threshold = 20;
}


CMovDetectorFilter :: ~CMovDetectorFilter(void){
	// kill the pins
	pBackgroundPin->Release();
	pMovementPin->Release();
	pInputPin->Release();

	IncrementPinVersion();

	delete outputMediaType;
}

HRESULT CMovDetectorFilter :: InitializeProcessParams(VIDEOINFOHEADER* pVih)	{

	// our internal monochrome datatype is aligned to 32 bits, so we have to calculate strides and everything
	bytesPerPixel_8u_C1 = 1;
	int remainder = (pVih->bmiHeader.biWidth*bytesPerPixel_8u_C1)%DIBalignment;
	if (remainder != 0)
		stride_8u_C1 = DIBalignment - remainder;
	else
		stride_8u_C1 = 0;
	step_8u_C1 = pVih->bmiHeader.biWidth*bytesPerPixel_8u_C1+stride_8u_C1;

	frameSize.height = pVih->bmiHeader.biHeight;
	frameSize.width = pVih->bmiHeader.biWidth;

	DbgLog((LOG_TRACE,0,TEXT("Initializeprocessparams: height = %d, step = %d"),frameSize.height,step_8u_C1 ));


	return NOERROR;
}

HRESULT CMovDetectorFilter :: InitializePins()	{

	CAutoLock lock(&m_csMultiIO);
	
	// important to initialize to NOERROR, because it doesn't get set in the parent class's
	// constructor unless it fails
	HRESULT hr = NOERROR;

	pBackgroundPin = new CPinOutputBackground((CBaseMultiIOFilter*)this, m_pLock, &hr, L"Background");
	if (FAILED(hr) || pBackgroundPin == NULL) {
		delete pBackgroundPin;
		return hr;
	}
	pBackgroundPin->AddRef();

	pInputPin = new CPinInput(this, m_pLock, &hr, L"Input");
	if (FAILED(hr) || pInputPin == NULL) {
		delete pInputPin;
		return hr;
	}
	pInputPin->AddRef();

	pMovementPin = new CPinOutputMovement(this, m_pLock, &hr, L"Movement");
	if (FAILED(hr) || pMovementPin == NULL) {
		delete pMovementPin;
		return hr;
	}
	pMovementPin->AddRef();

	IncrementPinVersion();

	return S_OK;
}

CBasePin* CMovDetectorFilter :: GetPin(int n)	{

	if ((n >= GetPinCount()) || (n < 0))
		return NULL;
	switch (n)	{
		case 0: return pInputPin;
		case 1: return pBackgroundPin;
		case 2: return pMovementPin;
	}
	return NULL;
}

int CMovDetectorFilter::GetPinCount()
{
	ASSERT(pInputPin != NULL || pBackgroundPin != NULL || pMovementPin != NULL);
	return (unsigned int)(3);
}


HRESULT CMovDetectorFilter :: GetMediaType(int iPosition, CMediaType *pOutputMediaType, CBaseMultiIOOutputPin* caller)	{

	CAutoLock lock(&m_csMultiIO);

	// This method should be called only if the input pin is connected
   if( pInputPin->IsConnected() == FALSE )	{
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

	if( pInputPin->IsConnected() )	{
		HRESULT hr = pInputPin->ConnectionMediaType(pOutputMediaType);
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



HRESULT CMovDetectorFilter :: DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProp, CBaseMultiIOOutputPin* caller)	{

	// both our output pins will have allocator instances of the same class and with same parameters,
	// so, *caller will not be used

	DbgLog((LOG_TRACE,0,TEXT("DecideBufferSize")));

	if (!pInputPin->IsConnected())	{
		DbgLog((LOG_TRACE,0,TEXT("DecideBufferSize - unexpected")));
		return E_UNEXPECTED;
	}

	CheckPointer(pAlloc,E_POINTER);
    CheckPointer(pProp,E_POINTER);
    HRESULT hr = S_OK;
	
	pProp->cbBuffer = outputAllocProps.cbBuffer;
	pProp->cBuffers = outputAllocProps.cBuffers;
	pProp->cbAlign = outputAllocProps.cbAlign;
	pProp->cbPrefix = outputAllocProps.cbPrefix;
    
	ASSERT(pProp->cbBuffer > 0);

	ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProp,&Actual);
    if (FAILED(hr)) {
		DbgLog((LOG_TRACE,0,TEXT("DecideBufferSize - setproperties failed")));
        return hr;
    }

	if (pProp->cBuffers > Actual.cBuffers || pProp->cbBuffer > Actual.cbBuffer)	return E_FAIL;
	DbgLog((LOG_TRACE,0,TEXT("DecideBufferSize - returns noerror")));
    return NOERROR;
}


HRESULT CMovDetectorFilter :: CheckInputType( const CMediaType *mtIn )	{

	if ((mtIn->majortype != MEDIATYPE_Video) ||
		(mtIn->subtype != MEDIASUBTYPE_8BitMonochrome_internal) ||
		(mtIn->formattype != FORMAT_VideoInfo) ||		// is this really necessary?
		(mtIn->cbFormat < sizeof(VIDEOINFOHEADER)))		// do all CMediaTypes have a VIH format?
	{
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	return S_OK;

}

// we don't want the filter class to act as a mediator for state changes and notifications; next 4 functions
// will never get called
HRESULT CMovDetectorFilter::EndOfStream( CBaseMultiIOInputPin* caller )	{
	return NOERROR;
}

HRESULT CMovDetectorFilter::BeginFlush( CBaseMultiIOInputPin* caller )	{
	return NOERROR;
}

HRESULT CMovDetectorFilter::EndFlush( CBaseMultiIOInputPin* caller )	{
	return NOERROR;
}

HRESULT CMovDetectorFilter::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate, CBaseMultiIOInputPin* caller )	{
	return NOERROR;
}

// Set up our output sample (adapted from from CTransformFilter baseclass)
HRESULT CMovDetectorFilter::InitializeOutputSampleBackground(IMediaSample *pSample, IMediaSample **ppOutSample)	{

    IMediaSample *pOutSample;

    // default - times are the same

	DbgLog((LOG_TRACE,0,TEXT("initializeoutputsample 0")));

    AM_SAMPLE2_PROPERTIES* const pProps = pInputPin->SampleProps();
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

    ;

		// pBackgroundPin sample
	ASSERT(pBackgroundPin->m_pIPPAllocator != NULL);
    HRESULT hr = pBackgroundPin->m_pIPPAllocator->GetBuffer(
             &pOutSample
             , pProps->dwSampleFlags & AM_SAMPLE_TIMEVALID ?
                   &pProps->tStart : NULL
             , pProps->dwSampleFlags & AM_SAMPLE_STOPVALID ?
                   &pProps->tStop : NULL
             , dwFlags
         );
	if (FAILED(hr)) {
		DbgLog((LOG_TRACE,0,TEXT("output alloc decommitted)")));
        return hr;
	}

	DbgLog((LOG_TRACE,0,TEXT("initializeoutputsample 2.1")));
    *ppOutSample = pOutSample;
    if (FAILED(hr)) {
		DbgLog((LOG_TRACE,0,TEXT("initializeoutputsample 2.2 failed!")));
        return hr;
    }
	
	SetSample(pSample, pOutSample, pProps);

	DbgLog((LOG_TRACE,0,TEXT("initializeoutputsample 3")));
    
    return S_OK;
}

// Set up our output sample (adapted from from CTransformFilter baseclass)
HRESULT CMovDetectorFilter::InitializeOutputSampleMovement(IMediaSample *pSample, IMediaSample **ppOutSample)	{

    IMediaSample *pOutSample;

    // default - times are the same

	DbgLog((LOG_TRACE,0,TEXT("initializeoutputsample 0")));

    AM_SAMPLE2_PROPERTIES* const pProps = pInputPin->SampleProps();
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

    ;

		// pMotionPin sample
	ASSERT(pMovementPin->m_pIPPAllocator != NULL);
    HRESULT hr = pMovementPin->m_pIPPAllocator->GetBuffer(
             &pOutSample
             , pProps->dwSampleFlags & AM_SAMPLE_TIMEVALID ?
                   &pProps->tStart : NULL
             , pProps->dwSampleFlags & AM_SAMPLE_STOPVALID ?
                   &pProps->tStop : NULL
             , dwFlags
         );
	if (FAILED(hr)) {
		DbgLog((LOG_TRACE,0,TEXT("output alloc decommitted)")));
        return hr;
	}
	

	DbgLog((LOG_TRACE,0,TEXT("initializeoutputsample 2.1")));
    *ppOutSample = pOutSample;
    if (FAILED(hr)) {
		DbgLog((LOG_TRACE,0,TEXT("initializeoutputsample 2.2 failed!")));
        return hr;
    }

	// set presentation time and everything
	SetSample(pSample, pOutSample, pProps);

	DbgLog((LOG_TRACE,0,TEXT("initializeoutputsample 3")));
    
    return S_OK;
}

void CMovDetectorFilter::SetSample(	IMediaSample *pSample,
									IMediaSample *pOutSample,
									AM_SAMPLE2_PROPERTIES* const &pProps )
{
	HRESULT hr;
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
	
	return;
}

/*
void CMovDetectorFilter::Process(IMediaSample* pInputSample,
									IMediaSample* pOutMovementSample,
									IMediaSample* pOutBackgroundSample)
{
	Ipp8u* pMovement_8u_C1;
	Ipp8u* pBackground_8u_C1;
	Ipp8u* pInput_8u_C1;

	// get the buffer pointers
	pOutBackgroundSample->GetPointer(&pBackground_8u_C1);
	pOutMovementSample->GetPointer(&pMovement_8u_C1);
	pInputSample->GetPointer(&pInput_8u_C1);

	// the processing
	Ipp8u* pMiddleOne_8u_C1;
	Ipp8u* pMedian_8u_C1 = pMedian->Push(pInput_8u_C1, &pMiddleOne_8u_C1);
	ippiCopy_8u_C1R(pMedian_8u_C1, step_8u_C1, pBackground_8u_C1, step_8u_C1, frameSize);

	// subtract the background from the delayed input frame - the one at the half of median window
	// rounded towards the ceiling
	//IppStatus status =
	ippiAbsDiff_8u_C1R( pMiddleOne_8u_C1, step_8u_C1, pMedian_8u_C1,
				step_8u_C1, pMovement_8u_C1, step_8u_C1, frameSize );
}
*/

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
, { L"Movement"           // strName
  , FALSE               // bRendered
  , TRUE                // bOutput
  , FALSE               // bZero
  , TRUE                // bMany
  , &CLSID_NULL         // clsConnectsToFilter
  , L""                 // strConnectsToPin
  , 1                   // nTypes
  , &sudPinTypes        // lpTypes
  }
, { L"Background"           // strName
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

const AMOVIESETUP_FILTER sudMovDetection =
{ 
	&CLSID_MOVDETECTOR			// clsID
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
			&CLSID_MOVDETECTOR,
			CMovDetectorFilter::CreateInstance,
			NULL,
			&sudMovDetection
		}
    };

int g_cTemplates = sizeof(g_Templates)/sizeof(g_Templates[0]);


// Provide the way for COM to create a filter object
CUnknown* WINAPI CMovDetectorFilter :: CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
	CheckPointer(phr,NULL);
	
	CMovDetectorFilter* pNewObject = new CMovDetectorFilter( NAME("ThreshedDiff"), punk, phr );

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
		CLSID_MOVDETECTOR,						// Filter CLSID. 
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
		FILTER_NAME, CLSID_MOVDETECTOR);

	pFM2->Release();
	return hr;
}


STDMETHODIMP CMovDetectorFilter::Stop()	{

	// claim the section before decommiting input pins immediately
    CAutoLock lock(&m_csMultiIO);

	  // stop if input pin or  at least one output pin disconnected
    if (	pInputPin == NULL || !pInputPin->IsConnected() ||
			(	(pBackgroundPin == NULL || !pBackgroundPin->IsConnected()) &&
				(pMovementPin == NULL || !pMovementPin->IsConnected())	)
		)	
	{
		m_State = State_Stopped;
		m_bEOSDelivered = FALSE;

		return NOERROR;
    }

	HRESULT hr = CBaseFilter::Stop();
	if(FAILED(hr)) return hr;

    return NOERROR;
}


STDMETHODIMP CMovDetectorFilter::Pause()	{

	// we won't let the graph pause unless both input samples arrived
	// lock the receive section
	CAutoLock lock(&m_csMultiIO);

    HRESULT hr = NOERROR;

	if (SUCCEEDED(hr)) {
	    hr = CBaseFilter::Pause();
    }

    m_bSampleSkipped = FALSE;
    m_bQualityChanged = FALSE;
    return hr;
}

STDMETHODIMP CMovDetectorFilter::Run(REFERENCE_TIME tStart)		{

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