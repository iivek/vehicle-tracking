#ifndef MATH_H
	#include <math.h>
	#define MATH_H
#endif

#pragma warning(disable:4312)

#ifndef TRACKER_MKII_H
	#include "tracker mkII.h"
#endif

#ifndef KALMANTRACKING_H
	#include "../Tracker/kalmanTracking.h"
#endif


CTrackerMkII::CTrackerMkII( TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr ) :
	  CBaseMultiIOFilter( tszName, punk, CLSID_Tracker_mkII, 0, 0, 0, 0 ),
			m_bOKToDeliverEndOfStream(FALSE),
			measurement(NULL),
			pLumaCV_C1(NULL),
			pColorCV_AC4(NULL),
			pHelperCV_C1(NULL),
			pContoursCV_C1(NULL),
			pMaskCV_C1(NULL),
			pTemporaryBufferCV_C1(NULL),			
			contourStorageNew(NULL),
			contourStorageOld(NULL),
			pHelper_8u_C1(NULL),
			pContours_8u_C1(NULL),
			pMask_8u_C1(NULL),
			pTemporaryBuffer_8u_C1(NULL)
	  {
		InitializePins();

		// the only output media subtype is MEDIASUBTYPE_CMovingObjects...
		// Our output MediaType is defined as follows:  
		//outputMediaType = new CMediaType(&MEDIASUBTYPE_CMovingObjects);
/*test		outputMediaType = new CMediaType();
		outputMediaType->InitMediaType();
		outputMediaType->AllocFormatBuffer(sizeof(FORMAT_VideoInfo));

		outputMediaType->SetType(&MEDIATYPE_Video);
		outputMediaType->SetSubtype(&MEDIASUBTYPE_CMovingObjects);
		outputMediaType->SetFormatType(&FORMAT_VideoInfo);
		outputMediaType->bFixedSizeSamples = FALSE;
		outputMediaType->SetTemporalCompression(FALSE);
		outputMediaType->cbFormat = sizeof(VIDEOINFOHEADER);
		*/

					// the only output media subtype is MEDIASUBTYPE_RGB24...
			// Our output MediaType is defined as follows:
			  outputMediaType = new CMediaType();
			  outputMediaType->InitMediaType();
			  outputMediaType->AllocFormatBuffer(sizeof(FORMAT_VideoInfo));

              outputMediaType->SetType(&MEDIATYPE_Video);
			  outputMediaType->SetSubtype(&MEDIASUBTYPE_RGB24);			  
			  outputMediaType->SetFormatType(&FORMAT_VideoInfo);
			  outputMediaType->bFixedSizeSamples = TRUE;
			  outputMediaType->SetTemporalCompression(FALSE);
			  outputMediaType->cbFormat = sizeof(VIDEOINFOHEADER);
	  }


CTrackerMkII::~CTrackerMkII(void){
	// kill the pins
	outputPin->Release();
	inputPinLuma->Release();
	inputPinColor->Release();
	IncrementPinVersion();

	delete outputMediaType;
}

HRESULT CTrackerMkII::InitializeProcessParams(VIDEOINFOHEADER* pVih)	{

	// these will be returned by ippimalloc, but we cheat and want them right away...

	// our internal monochrome datatype is aligned to 32 bits, so we have to calculate strides and everything
	bytesPerPixel_8u_C1 = 1;
	int remainder = (pVih->bmiHeader.biWidth*bytesPerPixel_8u_C1)%DIBalignment;
	if (remainder != 0)
		stride_8u_C1 = DIBalignment - remainder;
	else
		stride_8u_C1 = 0;
	step_8u_C1 = pVih->bmiHeader.biWidth*bytesPerPixel_8u_C1+stride_8u_C1;

	// same with RGB24
	bytesPerPixel_8u_C3 = 3;
	remainder = (pVih->bmiHeader.biWidth*bytesPerPixel_8u_C3)%DIBalignment;
	if (remainder != 0)
		stride_8u_C3 = DIBalignment - remainder;
	else
		stride_8u_C3 = 0;
	step_8u_C3 = pVih->bmiHeader.biWidth*bytesPerPixel_8u_C3+stride_8u_C3;

	// RGB32
	bytesPerPixel_8u_AC4 = 4;	// we know that
/*	step_8u_AC4 = (pVih->bmiHeader.biWidth * (pVih->bmiHeader.biBitCount / 8) + 3) & ~3;	// we calculate that
	stride_8u_AC4 = step_8u_AC4 - pVih->bmiHeader.biWidth*bytesPerPixel_8u_AC4;
*/
	remainder = (pVih->bmiHeader.biWidth*bytesPerPixel_8u_AC4)%DIBalignment;
	if (remainder != 0)
		stride_8u_AC4 = DIBalignment - remainder;
	else
		stride_8u_AC4 = 0;
	step_8u_AC4 = pVih->bmiHeader.biWidth*bytesPerPixel_8u_AC4+stride_8u_AC4;

	frameSize.height = pVih->bmiHeader.biHeight;
	frameSize.width = pVih->bmiHeader.biWidth;
	frameSizeCV = cvSize(pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight);	//it would perhaps be better if we used pointers to the same integers here

	// Kalman initial estimates, one Kalman for each cordinate
	//	
	// states:			{	x,		position
	//						v,		velocity
	//						a	}	acceleration
	//
	// measurements:	{	x	}	position
	
	float _TransitionMatrixD[4]	=	{	1,		1,
										0,		1
	};
	float _MeasurementMatrixD[2]	=	{	1,	0
	};
	float _ProcessNoise_covD[4]	=	{	100,	0,
										0,		100
	};
	float _ErrorPost_covD[4]		=	{	10000,	0,
											0,		10000
	};
	float _MeasurementNoise_covD[1]	=	{	2500
	};
	float _State_PostD[2]	=	{	0,		// location
									0		// velocity
	};

	m_params.initialKalman.TransitionMatrix		= cvCreateMatHeader( 2, 2, CV_32FC1 );
	m_params.initialKalman.MeasurementMatrix	= cvCreateMatHeader( 1, 2, CV_32FC1 );
	m_params.initialKalman.ProcessNoise_cov		= cvCreateMatHeader( 2, 2, CV_32FC1 );
	m_params.initialKalman.ErrorPost_cov		= cvCreateMatHeader( 2, 2, CV_32FC1 );
	m_params.initialKalman.MeasurementNoise_cov	= cvCreateMatHeader( 1, 1, CV_32FC1 );
	m_params.initialKalman.State_post			= cvCreateMatHeader( 2, 1, CV_32FC1 );

	memcpy( m_params.initialKalman.TransitionMatrixD,
		_TransitionMatrixD,		4*sizeof( float ) );
	memcpy( m_params.initialKalman.MeasurementMatrixD,
		_MeasurementMatrixD,	2*sizeof( float ) );
	memcpy( m_params.initialKalman.ProcessNoise_covD,
		_ProcessNoise_covD,		4*sizeof( float ) );
	memcpy( m_params.initialKalman.ErrorPost_covD,
		_ErrorPost_covD,		4*sizeof( float ) );
	memcpy( m_params.initialKalman.MeasurementNoise_covD,
		_MeasurementNoise_covD,	1*sizeof( float ) );
	memcpy( m_params.initialKalman.State_postD,
		_State_PostD,	2*sizeof( float ) );

	cvSetData( m_params.initialKalman.TransitionMatrix,
		m_params.initialKalman.TransitionMatrixD, 2*sizeof(float) );
	cvSetData( m_params.initialKalman.MeasurementMatrix,
		m_params.initialKalman.MeasurementMatrixD, 2*sizeof(float) );
	cvSetData( m_params.initialKalman.ProcessNoise_cov,
		m_params.initialKalman.ProcessNoise_covD, 2*sizeof(float) );
	cvSetData( m_params.initialKalman.ErrorPost_cov,
		m_params.initialKalman.ErrorPost_covD, 2*sizeof(float) );
	cvSetData( m_params.initialKalman.MeasurementNoise_cov,
		m_params.initialKalman.MeasurementNoise_covD, 1*sizeof(float) );
	cvSetData( m_params.initialKalman.State_post,
		m_params.initialKalman.State_postD, 1*sizeof(float) );

	// other parameters, to be set via property pages
	m_params.timeToLive = 0;
	m_params.minimalSize.height = 15;
	m_params.minimalSize.width = 15;
	m_params.bufferSize = 50;

	return NOERROR;
}


HRESULT CTrackerMkII::InitializePins()	{

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

	inputPinLuma = new CPinInputLuma(this, m_pLock, &hr, L"Luma");
	if (FAILED(hr) || inputPinLuma == NULL) {
		delete inputPinLuma;
		return hr;
	}
	inputPinLuma->AddRef();

	inputPinColor = new CPinInputColor(this, m_pLock, &hr, L"Color");
	if (FAILED(hr) || inputPinColor == NULL) {
		delete inputPinColor;
		return hr;
	}
	inputPinColor->AddRef();

	IncrementPinVersion();

	return S_OK;
}

CBasePin* CTrackerMkII::GetPin(int n)	{

	if ((n >= GetPinCount()) || (n < 0))
		return NULL;
	switch (n)	{
		case 0: return inputPinLuma;
		case 1: return inputPinColor;
		case 2: return outputPin;
	}
	return NULL;
}

int CTrackerMkII::GetPinCount()
{
	ASSERT(outputPin != NULL || inputPinColor != NULL || inputPinLuma != NULL);
	return (unsigned int)(3);
}

// TODO: alter the parent's method to accept a pointer to caller pin, as in GetMediaType. without it we
// do not know which pin wants to check its mediatype (luckily we have only one output pin, problems otherwise).
// lazy.
HRESULT CTrackerMkII::CheckOutputType( const CMediaType* pMediaType )
{
	CAutoLock lock(&m_csMultiIO);

	if(pMediaType->subtype != MEDIASUBTYPE_CMovingObjects)	{
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	DbgLog((LOG_TRACE,0,TEXT("CheckOutputType OK")));

	return S_OK;
}


HRESULT CTrackerMkII::GetMediaType(int iPosition, CMediaType *pOutputMediaType, CBaseMultiIOOutputPin* caller)
{
	CAutoLock lock(&m_csMultiIO);

	// This method should be called only if at least one input pin is connected
   if( inputPinLuma->IsConnected() == FALSE && inputPinColor->IsConnected() == FALSE )	{
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

	if( inputPinLuma->IsConnected() )	{
		HRESULT hr = inputPinLuma->ConnectionMediaType(pOutputMediaType);
		if (FAILED(hr))		{
			return hr;
		}
	} else	{
		HRESULT hr = inputPinColor->ConnectionMediaType(pOutputMediaType);
		if (FAILED(hr))		{
			return hr;
		}		
	}

    CheckPointer(pOutputMediaType,E_POINTER);
	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) pOutputMediaType->Format();

		// with same DIB width and height but only one (monochrome) channel
	pVih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pVih->bmiHeader.biPlanes = 1;
	//testpVih->bmiHeader.biBitCount = 8;
	pVih->bmiHeader.biBitCount = 24;
	pVih->bmiHeader.biCompression = BI_RGB;	// ?? no alternative, we treat it as uncompressed RGB
	pVih->bmiHeader.biSizeImage = DIBSIZE(pVih->bmiHeader);	
	pVih->bmiHeader.biClrImportant = 0;
	//SetRectEmpty(&(pVih->rcSource));
	//SetRectEmpty(&(pVih->rcTarget));

	// preferred output media subtype is MEDIASUBTYPE_CMovingObjects...
	// Our output MediaType is defined as follows: (irrelevant)
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

// we will accept the downstream filters proposals regarding the buffer size and number of buffers,
// unless they are nonsense.
// we will force our allocator, don't care about the proposed one
HRESULT CTrackerMkII::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProp, CBaseMultiIOOutputPin* caller)
{
   	// this gets called by the only output pin we have
	DbgLog((LOG_TRACE,0,TEXT("DecideBufferSize.")));

	if (!inputPinColor->IsConnected() || !inputPinLuma->IsConnected())	{
		DbgLog((LOG_TRACE,0,TEXT("DecideBufferSize - unexpected")));
		return E_UNEXPECTED;
	}

	CheckPointer(pAlloc,E_POINTER);
    CheckPointer(pProp,E_POINTER);
    HRESULT hr = S_OK;
	
	// we set cbBuffer to the number of CMovingObjects in the output buffer/array
	if (pProp->cbBuffer <= 0)
		pProp->cbBuffer = outputAllocProps.cbBuffer = m_params.bufferSize;
	// else don't change anything
	if (pProp->cBuffers <= 0)
		pProp->cBuffers = outputAllocProps.cBuffers = 5;	// 5 should also be a parameter -> a TODO :)
	// pProp->cbAlign = outputAllocProps.cbAlign;			// irrelevant
	// pProp->cbPrefix = outputAllocProps.cbPrefix;		// irrelevant
    
	ASSERT(pProp->cbBuffer);

	ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProp,&Actual);
    if (FAILED(hr)) {
		DbgLog((LOG_TRACE,0,TEXT("DecideBufferSize - setproperties failed")));
        return hr;
    }

    //if (pProp->cBuffers != Actual.cBuffers || pProp->cbBuffer > Actual.cbBuffer)	return E_FAIL;
	DbgLog((LOG_TRACE,0,TEXT("DecideBufferSize - returns noerror")));
    return NOERROR;
}

/*
HRESULT CTrackerMkII::CheckInputType( const CMediaType *mtIn )
{
	return S_OK;
}
*/

// responsible for delivering the notifications to the output pin;
// it does that directly, bypassing CTrackerMkII's related methods
HRESULT CTrackerMkII::EndOfStream( CBaseMultiIOInputPin* caller )
{
	return NOERROR;
}

HRESULT CTrackerMkII::BeginFlush( CBaseMultiIOInputPin* caller )
{
	return NOERROR;
}

HRESULT CTrackerMkII::EndFlush( CBaseMultiIOInputPin* caller )
{
	return NOERROR;
}

HRESULT CTrackerMkII::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate, CBaseMultiIOInputPin* caller )
{
	return NOERROR;
}

// Set up our output sample (as in CTransformFilter baseclass)
HRESULT CTrackerMkII::InitializeOutputSample(IMediaSample *pSample, IMediaSample **ppOutSample)	{

	// pin whose properties we will take as a basis for our output sample's properties
	CPinInputLuma* m_pInput = inputPinLuma;
	CPinOutput* m_pOutput = outputPin;

    IMediaSample *pOutSample;

    // default - times are the same

    AM_SAMPLE2_PROPERTIES* const pProps = m_pInput->SampleProps();
    DWORD dwFlags = m_bSampleSkipped ? AM_GBF_PREVFRAMESKIPPED : 0;

    // This will prevent the image renderer from switching us to DirectDraw
    // when we can't do it without skipping frames because we're not on a
    // keyframe. If it really has to switch us, it still will, but then we
    // will have to wait for the next keyframe
    if (!(pProps->dwSampleFlags & AM_SAMPLE_SPLICEPOINT)) {
		dwFlags |= AM_GBF_NOTASYNCPOINT;
    }

    ASSERT(m_pOutput->m_pListAllocator != NULL);
    HRESULT hr = m_pOutput->m_pListAllocator->GetBuffer(
             &pOutSample
             , pProps->dwSampleFlags & AM_SAMPLE_TIMEVALID ?
                   &pProps->tStart : NULL
             , pProps->dwSampleFlags & AM_SAMPLE_STOPVALID ?
                   &pProps->tStop : NULL
             , dwFlags
         );

    *ppOutSample = pOutSample;
    if (FAILED(hr)) {
        return hr;
    }

    ASSERT(pOutSample);
    IMediaSample2 *pOutSample2;
    if (SUCCEEDED(pOutSample->QueryInterface(IID_IMediaSample2,
                                             (void **)&pOutSample2)))	
	{
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
		DbgLog((LOG_TRACE,0,TEXT("interface gotten 2")));
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
        if (pSample->GetMediaTime(&MediaStart,&MediaEnd) == NOERROR)
		{
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
{ 
	{ L"Color"				  // strName
		, FALSE               // bRendered
		, FALSE               // bOutput
		, FALSE               // bZero
		, TRUE				  // bMany
		, &CLSID_NULL         // clsConnectsToFilter
		, L""                 // strConnectsToPin
		, 1                   // nTypes
		, &sudPinTypes        // lpTypes
	},
	{ L"Luma"				  // strName
		, FALSE               // bRendered
		, FALSE               // bOutput
		, FALSE               // bZero
		, TRUE		          // bMany
		, &CLSID_NULL         // clsConnectsToFilter
		, L""                 // strConnectsToPin
		, 1                   // nTypes
		, &sudPinTypes        // lpTypes
	},
	{ L"Output"				  // strName
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

const AMOVIESETUP_FILTER sudTracker =
{ 
	&CLSID_Tracker_mkII		// clsID
	, FILTER_NAME				// strName
	, MERIT_DO_NOT_USE			// dwMerit
	, 3							// nPins
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
			&CLSID_Tracker_mkII,
			CTrackerMkII::CreateInstance,
			NULL,
			&sudTracker
		}
    };

int g_cTemplates = sizeof(g_Templates)/sizeof(g_Templates[0]);


// Provide the way for COM to create a filter object
CUnknown* WINAPI CTrackerMkII::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
	CheckPointer(phr,NULL);
	
	CTrackerMkII* pNewObject = new CTrackerMkII( NAME("color"), punk, phr );

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
		CLSID_Tracker_mkII,							// Filter CLSID. 
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
		FILTER_NAME, CLSID_Tracker_mkII);

	pFM2->Release();
	return hr;
}



HRESULT CTrackerMkII::CollectSample(IMediaSample* pSample, CPinInputLuma*)
{
	HRESULT hr;
	// addreff sample so it doesn't get lost
	pSample->AddRef();

	// check if there is a corresponding sample at the other input pin. input pins are in sync so
	// no additional timestamps checking is needed
	if( !qpColor.empty() )
	{
		// both input pins have provided us with samples. we process the samples now

		IMediaSample* samples[2];
		// convention: samples[0] ... luma pin;	samples[1] ... color pin
		samples[0] = pSample;
		samples[1] = qpColor.front();

		// let the filter class handle processing
		hr = Process(samples);

		// we can now release the samples
		DbgLog((LOG_TRACE,0,TEXT("		releasing luma")));
		samples[0]->Release();
		DbgLog((LOG_TRACE,0,TEXT("		releasing color")));
		samples[1]->Release();
		DbgLog((LOG_TRACE,0,TEXT("popping color")));
		qpColor.pop();
		DbgLog((LOG_TRACE,0,TEXT("					buffered luma = %d"), qpLuma.size()));
	}
	else
	{
		// the corresponding sample from the other pin has not arrived yet. queue this sample -
		// it will be used later

		DbgLog((LOG_TRACE,0,TEXT("pushing luma")));
		qpLuma.push(pSample);
		hr = S_OK;
		DbgLog((LOG_TRACE,0,TEXT("					buffered luma = %d"), qpLuma.size()));
	}

	return hr;
}

HRESULT CTrackerMkII::CollectSample(IMediaSample* pSample, CPinInputColor*)
{
	HRESULT hr;
	// addreff sample so it doesn't get lost
	DbgLog((LOG_TRACE,0,TEXT("		addref color %d"), pSample));
	pSample->AddRef();

	// check if there is a corresponding sample at the other input pin. input pins are in sync so
	// no additional timestamps checking is needed
	if( !qpLuma.empty() )
	{
		// both input pins have provided us with samples. we process the samples now

		IMediaSample* samples[2];
		// convention: samples[0] ... luma pin;	samples[1] ... color pin
		samples[0] = qpLuma.front();
		samples[1] = pSample;

		// let the filter class handle processing
		hr = Process(samples);

		// we can now release the samples
		DbgLog((LOG_TRACE,0,TEXT("		release luma")));		
		samples[0]->Release();
		DbgLog((LOG_TRACE,0,TEXT("		release color")));
		samples[1]->Release();
		DbgLog((LOG_TRACE,0,TEXT("popping luma")));		
		qpLuma.pop();
		DbgLog((LOG_TRACE,0,TEXT("					buffered color= %d"), qpColor.size()));
	}
	else
	{
		// the corresponding sample from the other pin has not arrived yet. queue this sample -
		// it will be used later

		DbgLog((LOG_TRACE,0,TEXT("pushing color")));
		qpColor.push(pSample);
		hr = S_OK;
		DbgLog((LOG_TRACE,0,TEXT("					buffered color= %d"), qpColor.size()));
	}

	return hr;
}

// releases queued samples nad empties queues
void CTrackerMkII::EmptyLumaQueue()
{
	// qpLuma
	DbgLog((LOG_TRACE,0,TEXT("					buffered luma = %d"), qpLuma.size()));
	while(!qpLuma.empty())
	{
		DbgLog((LOG_TRACE,0,TEXT("		releasing luma")));
		qpLuma.front()->Release();
		qpLuma.pop();
	}
}

// releases queued samples nad empties queues
void CTrackerMkII::EmptyColorQueue()
{
	// qpColor
	DbgLog((LOG_TRACE,0,TEXT("					buffered color = %d"), qpColor.size()));
	while(!qpColor.empty())
	{
		DbgLog((LOG_TRACE,0,TEXT("		releasing color")));
		qpColor.front()->Release();
		qpColor.pop();
	}
}

HRESULT CTrackerMkII::Process(IMediaSample** samples)	{

	CAutoLock lock(&m_csReceive);

	HRESULT hr = NOERROR;

	IppStatus status;
	BYTE* pColor_8u_AC4;
	BYTE* pLuma_8u_C1;
	BYTE* pOut_list_raw;
	// create a new media sample based on the sample from luma input pin
	IMediaSample *pOutSample;
	DbgLog((LOG_TRACE,0,TEXT("before initializeoutputsample")));

	hr = InitializeOutputSample(samples[1], &pOutSample);
	if(FAILED(hr)) return hr;
	
	// get the addresses of the actual data (buffers)
	samples[0]->GetPointer( &pLuma_8u_C1 );
	samples[1]->GetPointer( &pColor_8u_AC4 );
	pOutSample->GetPointer( &pOut_list_raw );
//test	CMovingObject* pOut_list = (CMovingObject*) pOut_list_raw;
	// set headers
	cvSetData( pLumaCV_C1, pLuma_8u_C1, step_8u_C1 );
	cvSetData( pColorCV_AC4, pColor_8u_AC4, step_8u_AC4 );
	// we copy the pLumaCV_C1 and work with pHelperCV_C1 because we don't want to mess it up... we need it to preserve
	// (and display) the image from the upstream filter
	cvCopy (pLumaCV_C1, pHelperCV_C1);

	/* Real work gets done here
	*/
	ippiSet_8u_C1R( 0, pContours_8u_C1, step_8u_C1, frameSize );
	CvSeq* contour = FindMovingObjects(pHelperCV_C1);

	status = ippiDup_8u_C1C3R(pContours_8u_C1, step_8u_C1, pOut_list_raw, step_8u_C3, frameSize );
//	TrackObjects();

		// copy the CMovingObjects' data to the output buffer (output array has already been initialized)
/*test
	if(objectsOld.size() >= m_params.bufferSize)
	{
		// copy only bufferSize CMovingObjects to output to prevent writing on unallocated mem.
		unsigned int i;
		for( iterOld = objectsOld.begin(), i = 0; i<m_params.bufferSize ; ++iterOld, ++i )
		{
			pOut_list[i].ID = iterOld->get()->ID;
			pOut_list[i].rect = iterOld->get()->rect;
			// object center
			pOut_list[i].center.x = iterOld->get()->center.x;
			pOut_list[i].center.y = iterOld->get()->center.y;
			// predictions
			pOut_list[i].predictedX = iterOld->get()->predictedX;
			pOut_list[i].predictedY = iterOld->get()->predictedY;
			// velocities
			pOut_list[i].velocityX = iterOld->get()->velocityX;
			pOut_list[i].velocityY = iterOld->get()->velocityY;
		}
	}
	else
	{
		unsigned int i;
		for( iterOld = objectsOld.begin(), i = 0; iterOld != objectsOld.end(); ++iterOld, ++i )
		{
			pOut_list[i].ID = iterOld->get()->ID;
			pOut_list[i].rect = iterOld->get()->rect;
			// object center
			pOut_list[i].center.x = iterOld->get()->center.x;
			pOut_list[i].center.y = iterOld->get()->center.y;
			// predictions
			pOut_list[i].predictedX = iterOld->get()->predictedX;
			pOut_list[i].predictedY = iterOld->get()->predictedY;
			// velocities
			pOut_list[i].velocityX = iterOld->get()->velocityX;
			pOut_list[i].velocityY = iterOld->get()->velocityY;
			
		}
		
		// set the rect values of the object that is next in the output buffer to 0. this signals that
		// the objects that follow in the buffer are not valid
		pOut_list[i].rect.height = 0;
		pOut_list[i].rect.width = 0;
	}
	*/
	// cleanup after this iteration
/*	if( contour != NULL )	{		
		cvClearSeq( contour );
	}
	cvClearMemStorage( contourStorageNew );
*/
	hr = outputPin->Deliver(pOutSample);
	return hr;
}

// we make an abstraction of each connected component as a moving object, and sort it in an appropriate list
//	- if it is considered to be an object from the last frame in objectsWithFit
//	- if it is considered to be an entirely new object in objectsNew
CvSeq* CTrackerMkII :: FindMovingObjects(IplImage* pLumaCV_C1)
{
	// find contours
	CvSeq* contour;
	int numContours = cvFindContours(	pLumaCV_C1, contourStorageNew,
										&contour, sizeof( CvContour ), CV_RETR_EXTERNAL,
										CV_CHAIN_APPROX_NONE );
			
	CvSeq* contourIter = contour;		// so we don't lose the original pointer
	int i = 0;
	for( ; contourIter; contourIter = contourIter->h_next  )
	{
		// contour must satisfy a minimum dimensions condition
		CvRect contourRect = cvBoundingRect( contourIter, 0);
		if( ( contourRect.width <= m_params.minimalSize.width ) &&
			( contourRect.width <= m_params.minimalSize.height ) )	{	continue;	}

		++i;

		/* Superpose contour to where we want all contours
		 */
		CvScalar color_hole = CV_RGB(0, 0, 0);
		CvScalar color_fill = CV_RGB(i, i, i);
		cvDrawContours(pContoursCV_C1, contourIter, color_fill, color_hole, 1, CV_FILLED);
	}
	// i now contains the number of new blobs

	DbgLog((LOG_TRACE,0,TEXT("i = %d"), i));

	/* For each old object, calculate the contour of its prediction and find all contours
	   from pContours_8u_C1 (contours of newly found MOs) that it overlaps with. 
	 */
	CvScalar color_hole = CV_RGB(0, 0, 0);
	CvScalar color_fill = CV_RGB(1, 1, 1);

	/*	Features used in tracking:
			- location of the center of bounding box of the MO's blob
			- size of the MO's blob
			- the size of overlapping region of old MO's predicted blob and some newly found blob,
				the only criterion used to detect occlusions.
			- color histogram of the MO
			- (pronounced corners of the MO)
	*/

	for( iterOld = objectsOld.begin(); iterOld != objectsOld.end(); ++iterOld )	{

		DbgLog((LOG_TRACE,0,TEXT("					insider")));
		CvSeq* objectContour = iterOld->get()->contour;
		DbgLog((LOG_TRACE,0,TEXT("					insider2")));
		CvPoint offset;
//		offset.x = iterOld->get()->predictedX - iterOld->get()->center.x;
//		offset.y = iterOld->get()->predictedY - iterOld->get()->center.y;
		offset.x = 0;
		offset.y = 0;
		CvRect roi = cvBoundingRect(objectContour, 0);
		DbgLog((LOG_TRACE,0,TEXT("					roi, %d, %d, %d, %d"), roi.x, roi.y, roi.height, roi.width));
		DbgLog((LOG_TRACE,0,TEXT("					insider3")));
		cvSetImageROI(pContoursCV_C1, roi);
		cvSetImageROI(pTemporaryBufferCV_C1, roi);
		cvSetImageROI(pMaskCV_C1, roi);
		DbgLog((LOG_TRACE,0,TEXT("					insider3.5")));
		cvDrawContours(pMaskCV_C1, objectContour, color_fill, color_hole, 1, CV_FILLED, 8, offset);
		DbgLog((LOG_TRACE,0,TEXT("					insider4")));
		cvCopy(pContoursCV_C1, pTemporaryBufferCV_C1, pMaskCV_C1);
		/*
			The masked histogram of pTemporaryBufferCV_C1 tells us how big the overlapping region of
			our old object's predicted blob and new object's blobs is.
		*/
		DbgLog((LOG_TRACE,0,TEXT("					insider5")));
		int histSize[] = {256};
		CvHistogram* hist = cvCreateHist(1, histSize, CV_HIST_ARRAY, NULL, 1);
		DbgLog((LOG_TRACE,0,TEXT("					insider5.5")));
		cvCalcHist(&pTemporaryBufferCV_C1, hist, 0, pMaskCV_C1);
		DbgLog((LOG_TRACE,0,TEXT("					insider6")));
		int overlappingPixels_max = 0;
		int overlappingContour_max = 0;
		for(int j = 1; j<=255; ++j)	{
			if (cvQueryHistValue_1D(hist, j) > overlappingPixels_max)	{	// number of overlapping pixels
				DbgLog((LOG_TRACE,0,TEXT("					insider7")));
				DbgLog((LOG_TRACE,0,TEXT("					dsfkjalskdfja %d"), overlappingContour_max));
				overlappingContour_max = j;
				overlappingPixels_max = cvQueryHistValue_1D(hist, j);
			}
		}

		/*
		 *	Ad hoc inheriting
		 */
		if(overlappingContour_max == 0)	{
			// create a CMovingObject out of each contour
			SPMovingObject	spNewObject	(new CMovingObject(0,			// we will assign ID later
										contourRect,
										m_params.timeToLive,
										contourIter)
									);
			objectsNew.push_front( spNewObject );
		}
		//objectsOld.splice( objectsOld.begin(), objectsNew, iterTmp );
		else	{
			iterOld->get()->contour = (CvSeq*) cvGetSeqElem(contour, overlappingContour_max-1);
		}

		cvResetImageROI(pContoursCV_C1);
		cvResetImageROI(pTemporaryBufferCV_C1);
		cvResetImageROI(pMaskCV_C1);

		/*
		 * Draw a histogram on the pContoursCV_C1
		 */
/*
		int bin_w = cvRound((double)pContoursCV_C1->width/histSize);
		for( i = 0; i < histSize; i++ ) {
			//draw the histogram data onto the histogram image
			cvRectangle( pContoursCV_C1, cvPoint(i*bin_w, pContoursCV_C1->height),
				cvPoint((i+1)*bin_w, 
				pContoursCV_C1->height - cvRound(cvGetReal1D(hist->bins,i))),
				cvScalarAll(0), -1, 8, 0 );
			//get the value at the current histogram bucket
			float* bins = cvGetHistValue_1D(hist,i);
		}
*/
		/*
		 *	Masking the color input frame with pMaskCV_C1
		 */
	}

	CvMemStorage* swap = contourStorageOld;
	contourStorageOld = contourStorageNew;
	contourStorageNew = swap;
	cvClearMemStorage( contourStorageNew );
	objectsOld.splice( objectsOld.begin(), objectsNew );


	

	return contour;
}

/*
// we want to associate the objectOld with the element of objectsWithFit which is closest
// to objectOld's Kalman prediction of location. Unassociated elements of objectNew will
// be stored in objectsNew, and are considered appearing for the first time.
// We do ti in a greedy way - in each iteration we take the closest object. Alternatively,
// it is posssible to minimize some global measure, e.g. we could assign new objects to old ones
// in a way that minimizes the sum of distances between assigned pairs.
void CTrackerMkII :: TrackObjects()	{
	
	for( iterNew = objectsWithFit.begin(); iterNew != objectsWithFit.end(); ++iterNew )	{
		// sort them by distances from predicted location.
		iterNew->get()->fitsTo.sort();
	}

	double distance;

	for( ; !objectsWithFit.empty(); )
	{
		std::list <SPMovingObject>::iterator iterCount;
		distance = INT_MAX;	// so that every other distance that makes sense is smaller
		iterNew = NULL;

		std::list <CFitsTo>::iterator iterCount2;	// iterator that passes through CFitsTo list of each new Object

		DbgLog((LOG_TRACE,0,TEXT("evo tucam6")));

		for( iterCount = objectsWithFit.begin(); iterCount != objectsWithFit.end(); ++iterCount )
		{
			if( iterCount->get()->fitsTo.begin()->distance <= distance )
			{
				iterNew = iterCount;
				distance = iterCount->get()->fitsTo.begin()->distance;
			}
		}

		DbgLog((LOG_TRACE,0,TEXT("evo tucam 7")));
		// we move &iterNew to objectsSuccessors
		objectsSuccessors.splice( objectsSuccessors.begin(), objectsWithFit, iterNew );

		// remove all fitsTo that have iterNew's ID. This means that we have found a fit with an old Object
		// that has this ID and we exclude it from further search.

		DbgLog((LOG_TRACE,0,TEXT("evo tucam")));

		for( iterCount = objectsWithFit.begin(); iterCount != objectsWithFit.end(); ++iterCount )
		{		
			DbgLog((LOG_TRACE,0,TEXT("evo tucam2")));
			for(	iterCount2 = iterCount->get()->fitsTo.begin();
					iterCount2 != iterCount->get()->fitsTo.end();
					++iterCount2 )
			{
				DbgLog((LOG_TRACE,0,TEXT("evo tucam3")));
				if( iterCount2->ID == iterNew->get()->fitsTo.begin()->ID )
				{
					std::list <CFitsTo>::iterator iterTmp = iterCount2--;
					iterCount->get()->fitsTo.erase( iterTmp );
					break;
				}
			}

			// check if we have objects with empty fitsTo lists. if so, move them to objectsNew because
			// they really don't fit to any of the old objects; a better fit has been found
			DbgLog((LOG_TRACE,0,TEXT("evo tucam4")));
			if( iterCount->get()->fitsTo.empty() )
			{	
				std::list <SPMovingObject>::iterator iterTmp = iterCount--;
				objectsNew.splice( objectsNew.begin(), objectsWithFit, iterTmp );
			}
			DbgLog((LOG_TRACE,0,TEXT("evo tucam5")));

		}
	}
	// this all is not the fastest way in the world to do this but we don't expect a large number of objects
	// (short lists) so it should work OK

	// Checkpoint - at this moment we have:
	// in objectsOld		- old objects, fitted and unfitted
	// in objectsNew		- only objects that appeared for the first time
	// in objectsWithFit	- new objects that have a fit

	// for each element of objectsSuccessors, we do an update (location, kalman) and copy them into objectsOld.
	// we locate objectOld by ID. (storing iterators would be somewhat faster for a list. or using hashmaps?)
	for( iterNew = objectsSuccessors.begin(); iterNew != objectsSuccessors.end(); ++iterNew )
	{
		for( iterOld = objectsOld.begin(); iterOld != objectsOld.end(); ++iterOld)
		{
			if( iterNew->get()->fitsTo.begin()->ID == iterOld->get()->ID )
			{
				// Kalman state measuring & update:
				// we combine prediction and measurement to get Kalman gain and the final estimate
				iterNew->get()->fitsTo.clear();
				iterNew->get()->kalmanX = iterOld->get()->kalmanX;
				iterNew->get()->kalmanY = iterOld->get()->kalmanY;
				iterOld->get()->kalmanX = NULL;				// we don't release the kalman structure - recycle for a greener world
				iterOld->get()->kalmanY = NULL;
				iterNew->get()->ID = iterOld->get()->ID;

				objectsOld.erase( iterOld );			// erase old object... 
			
				// x
				// make a measurement. location only.
				measurementD[0] = ( float )( iterNew->get()->center.x );
				DbgLog((LOG_TRACE,0,TEXT("Measuring x: %d"), iterNew->get()->center.x));
				cvKalmanCorrect( iterNew->get()->kalmanX, measurement );
				// predict next state
				const CvMat* predictionX = cvKalmanPredict( iterNew->get()->kalmanX, NULL );
				iterNew->get()->predictedX = predictionX->data.fl[0];
				DbgLog((LOG_TRACE,0,TEXT("Predicting x: %g"), iterNew->get()->predictedX));
				// take and store velocity component
				iterNew->get()->velocityX = iterNew->get()->kalmanX->state_post->data.fl[1];

				// y
				// make a measurement. location only.
				measurementD[0] = ( float )( iterNew->get()->center.y );
				DbgLog((LOG_TRACE,0,TEXT("Measuring y: %d"), iterNew->get()->center.y));
				cvKalmanCorrect( iterNew->get()->kalmanY, measurement );
				// predict next state
				const CvMat* predictionY = cvKalmanPredict( iterNew->get()->kalmanY, NULL );
				iterNew->get()->predictedY = predictionY->data.fl[0];
				DbgLog((LOG_TRACE,0,TEXT("Predicting y: %g"), iterNew->get()->predictedY));
				// take and store velocity component
				iterNew->get()->velocityY = iterNew->get()->kalmanY->state_post->data.fl[1];

				break;
			}
		}
	}

	// Checkpoint - now each of the objectsOld which had an assigned successor has had its Kalman filter's
	// state updated and lives on in ObjectsSuccessors. objectsOld now contains only old objects which do not
	// have a successor. What to do with them?

	// decrease TTL from each remaining object of objectsOld. If its TTL expires, we kill them
	for( iterOld = objectsOld.begin(); iterOld != objectsOld.end(); )
	{
		if( (iterOld->get()->TTL)-- <= 0 )
		{
			DbgLog((LOG_TRACE,0,TEXT("Killing: %d"), iterOld->get()->ID));
			objectsOld.erase( iterOld++ );
		}
		/*else
		{
			// feed Kalman filter with its last a posteriori state estimation as measurements
			// we cheat KF, so we should increase variance at least
			measurementD[0] = ( float )( iterOld->kalman->state_post->data.fl[0] +
											iterOld->kalman->state_post->data.fl[2] );
			measurementD[1] = ( float )( iterOld->kalman->state_post->data.fl[1] +
											iterOld->kalman->state_post->data.fl[3] );
			cvKalmanCorrect( iterOld->kalman, Measurement );

			// predict next state
			const CvMat* prediction = cvKalmanPredict( iterOld->kalman, NULL );
			iterOld->predictedX = prediction->data.fl[0];
			iterOld->predictedY = prediction->data.fl[1];

			// if object (its center has gotten out of the frame), delete object
			if( ( iterOld->predictedX<0 ) || ( iterOld->predictedX>=FrameSize.width ) )
			{
				objectsOld.erase( iterOld++ );
			}
			else
			{
				if( ( iterOld->predictedY<0 ) || ( iterOld->predictedY>=FrameSize.height ) )
				{
					objectsOld.erase( iterOld++ );
				}
				else
				{
					++iterOld;
				}
			}
		}*/
/*		++iterOld;		// comment this row and uncomment above section to feed kalman with prediction instead of freezing it
	}

	// move objects from objectsSuccessors to oldObjects
	objectsOld.splice( objectsOld.end(), objectsSuccessors );
	
	// finally, we assign ID to every object from objectsNew and move it to oldObjects.
	for( ; !objectsNew.empty(); )
	{
		iterNew = objectsNew.begin();
		// Kalman initialization and update for new objects
		iterNew->get()->InitializeKalmanLocation(m_params.initialKalman);

		// we have to initialize predictions in some way
		// x

		const CvMat* predictionX = cvKalmanPredict( iterNew->get()->kalmanX, NULL );
		iterNew->get()->predictedX	= predictionX->data.fl[0];
		DbgLog((LOG_TRACE,0,TEXT("Initial location: %d"), iterNew->get()->center.x));
		DbgLog((LOG_TRACE,0,TEXT("Initial prediction: %g, %g, %g"), predictionX->data.fl[0],
			predictionX->data.fl[1], predictionX->data.fl[2]));
						
		iterNew->get()->velocityX = iterNew->get()->kalmanX->state_post->data.fl[1];
		// y
		const CvMat* predictionY = cvKalmanPredict( iterNew->get()->kalmanY, NULL );
		iterNew->get()->predictedY	= predictionY->data.fl[0];
		DbgLog((LOG_TRACE,0,TEXT("Initial location: %d"), iterNew->get()->center.y));
		DbgLog((LOG_TRACE,0,TEXT("Initial prediction: %g, %g, %g"),
			predictionY->data.fl[0], predictionY->data.fl[1], predictionY->data.fl[2]));
		iterNew->get()->velocityY = iterNew->get()->kalmanY->state_post->data.fl[1];

		iterNew->get()->ID = ++(uniqueID);

		objectsOld.splice( objectsOld.end(), objectsNew, iterNew );	// move that new object to old objects;
	}
}
*/