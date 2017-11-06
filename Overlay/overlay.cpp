#ifndef MATH_H
	#include <math.h>
	#define MATH_H
#endif

#pragma warning(disable:4312)

#ifndef OVERLAY_H
	#include "overlay.h"
#endif

#ifndef KALMANTRACKING_H
	#include "../Tracker/kalmanTracking.h"
#endif


COverlay::COverlay( TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr ) :
	  CBaseMultiIOFilter( tszName, punk, CLSID_Overlay, 0, 0, 0, 0 ),
	  pOutputCV_C3(NULL),
	  m_bOKToDeliverEndOfStream(FALSE)
	  {

			InitializePins();

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

			  // init params here
	  }


COverlay::~COverlay(void){
	// kill the pins
	outputPin->Release();
	inputMainPin->Release();
	inputOverlayPin->Release();

	IncrementPinVersion();

	delete outputMediaType;
}

HRESULT COverlay::InitializeProcessParams(VIDEOINFOHEADER* pVih)	{

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
	frameSizeCV = cvSize(pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight);	//it would be better if we used pointers to the same integers here

	colorList[0] = CV_RGB(255,0,0);
	colorList[1] = CV_RGB(0,255,0);
	colorList[2] = CV_RGB(0,0,255);

	return NOERROR;
}


HRESULT COverlay::InitializePins()	{

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

	inputMainPin = new CPinInputMain(this, m_pLock, &hr, L"Main");
	if (FAILED(hr) || inputMainPin == NULL) {
		delete inputMainPin;
		return hr;
	}
	inputMainPin->AddRef();

	inputOverlayPin = new CPinInputOverlay(this, m_pLock, &hr, L"Overlay");
	if (FAILED(hr) || inputOverlayPin == NULL) {
		delete inputOverlayPin;
		return hr;
	}
	inputOverlayPin->AddRef();

	IncrementPinVersion();

	return S_OK;
}

CBasePin* COverlay::GetPin(int n)	{

	if ((n >= GetPinCount()) || (n < 0))
		return NULL;
	switch (n)	{
		case 0: return inputMainPin;
		case 1: return inputOverlayPin;
		case 2: return outputPin;
	}
	return NULL;
}

int COverlay::GetPinCount()
{
	ASSERT(outputPin != NULL || inputOverlayPin != NULL || inputMainPin != NULL);
	return (unsigned int)(3);
}

// TODO: alter the parent method to accept a pointer to caller pin, as in GetMediaType. without it we
// do not know which pin wants to check its mediatype (luckily we have only one output pin, problems otherwise).
// lazy.
HRESULT COverlay::CheckOutputType( const CMediaType* pMediaType )
{
	CAutoLock lock(&m_csMultiIO);

	if(pMediaType->subtype != MEDIASUBTYPE_RGB24)	{
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	DbgLog((LOG_TRACE,0,TEXT("CheckTransform OK")));

	return S_OK;
}


HRESULT COverlay::GetMediaType(int iPosition, CMediaType *pOutputMediaType, CBaseMultiIOOutputPin* caller)
{
	CAutoLock lock(&m_csMultiIO);

	// This method should be called only if at least one input pin is connected
   if( inputMainPin->IsConnected() == FALSE && inputOverlayPin->IsConnected() == FALSE )	{
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

	if( inputMainPin->IsConnected() )	{
		HRESULT hr = inputMainPin->ConnectionMediaType(pOutputMediaType);
		if (FAILED(hr))		{
			return hr;
		}
	} else	{
		HRESULT hr = inputOverlayPin->ConnectionMediaType(pOutputMediaType);
		if (FAILED(hr))		{
			return hr;
		}		
	}

    CheckPointer(pOutputMediaType,E_POINTER);
	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) pOutputMediaType->Format();

	pVih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pVih->bmiHeader.biPlanes = 1;
	pVih->bmiHeader.biBitCount = 24;
	pVih->bmiHeader.biCompression = BI_RGB;
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


HRESULT COverlay::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProp, CBaseMultiIOOutputPin* caller)
{
   	// this gets called by the only output pin we have

	DbgLog((LOG_TRACE,0,TEXT("DecideBufferSize budfg")));

	if (!inputMainPin->IsConnected() || !inputOverlayPin->IsConnected())	{
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

//
//HRESULT COverlay::CheckInputType( const CMediaType *mtIn )
//{
//	return S_OK;
//}

// responsible for delivering the notifications to the output pin;
// it does that directly, bypassing COverlay's related methods
HRESULT COverlay::EndOfStream( CBaseMultiIOInputPin* caller )
{
	return NOERROR;
}

HRESULT COverlay::BeginFlush( CBaseMultiIOInputPin* caller )
{
	return NOERROR;
}

HRESULT COverlay::EndFlush( CBaseMultiIOInputPin* caller )
{
	return NOERROR;
}

HRESULT COverlay::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate, CBaseMultiIOInputPin* caller )
{
	return NOERROR;
}

// Set up our output sample (as in CTransformFilter baseclass)
HRESULT COverlay::InitializeOutputSample(IMediaSample *pSample, IMediaSample **ppOutSample)	{

	// pin whose properties we will take as a basis for our output sample's properties
	CPinInputMain* m_pInput = inputMainPin;
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

    ASSERT(m_pOutput->m_pIPPAllocator != NULL);
    HRESULT hr = m_pOutput->m_pIPPAllocator->GetBuffer(
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

const AMOVIESETUP_FILTER sudOverlay =
{ 
	&CLSID_Overlay				// clsID
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
			&CLSID_Overlay,
			COverlay::CreateInstance,
			NULL,
			&sudOverlay
		}
    };

int g_cTemplates = sizeof(g_Templates)/sizeof(g_Templates[0]);


// Provide the way for COM to create a filter object
CUnknown* WINAPI COverlay::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
	CheckPointer(phr,NULL);
	
	COverlay* pNewObject = new COverlay( NAME("Overlay"), punk, phr );

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
		CLSID_Overlay,							// Filter CLSID. 
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
		FILTER_NAME, CLSID_Overlay);

	pFM2->Release();
	return hr;
}



HRESULT COverlay::CollectSample(IMediaSample* pSample, CPinInputMain*)
{
	HRESULT hr;
	// addreff sample so it doesn't get lost

	DbgLog((LOG_TRACE,0,TEXT("		addref main %d"), pSample));
	pSample->AddRef();

	// check if there is a corresponding sample at the other input pin. input pins are in sync so
	// no additional timestamps checking is needed
	if( !qpOverlay.empty() )
	{
		// both input pins have provided us with samples. we process the samples now

		IMediaSample* samples[2];
		// convention: samples[0] ... main pin;	samples[1] ... overlay pin
		samples[0] = pSample;
		samples[1] = qpOverlay.front();

		// let the filter class handle processing
		hr = Process(samples);

		// we can now release the samples
		DbgLog((LOG_TRACE,0,TEXT("		releasing main")));
		samples[0]->Release();
		DbgLog((LOG_TRACE,0,TEXT("		releasing overlay")));
		samples[1]->Release();
		DbgLog((LOG_TRACE,0,TEXT("popping overlay")));
		qpOverlay.pop();
		DbgLog((LOG_TRACE,0,TEXT("					buffered main = %d"), qpMain.size()));
	}
	else
	{
		// the corresponding sample from the other pin has not arrived yet. queue this sample -
		// it will be used later

		DbgLog((LOG_TRACE,0,TEXT("pushing main")));
		qpMain.push(pSample);
		hr = S_OK;
		DbgLog((LOG_TRACE,0,TEXT("					buffered main = %d"), qpMain.size()));
	}

	return hr;
}

HRESULT COverlay::CollectSample(IMediaSample* pSample, CPinInputOverlay*)
{
	HRESULT hr;
	// addreff sample so it doesn't get lost
	DbgLog((LOG_TRACE,0,TEXT("		addref overlay %d"), pSample));
	pSample->AddRef();

	// check if there is a corresponding sample at the other input pin. input pins are in sync so
	// no additional timestamps checking is needed
	if( !qpMain.empty() )
	{
		// both input pins have provided us with samples. we process the samples now

		IMediaSample* samples[2];
		// convention: samples[0] ... main pin;	samples[1] ... overlay pin
		samples[0] = qpMain.front();
		samples[1] = pSample;

		// let the filter class handle processing
		hr = Process(samples);

		// we can now release the samples
		DbgLog((LOG_TRACE,0,TEXT("		release main")));		
		samples[0]->Release();
		DbgLog((LOG_TRACE,0,TEXT("		release overlay")));
		samples[1]->Release();
		DbgLog((LOG_TRACE,0,TEXT("popping main")));		
		qpMain.pop();
		DbgLog((LOG_TRACE,0,TEXT("					buffered overlay= %d"), qpOverlay.size()));
	}
	else
	{
		// the corresponding sample from the other pin has not arrived yet. queue this sample -
		// it will be used later

		DbgLog((LOG_TRACE,0,TEXT("pushing overlay")));
		qpOverlay.push(pSample);
		hr = S_OK;
		DbgLog((LOG_TRACE,0,TEXT("					buffered overlay= %d"), qpOverlay.size()));
	}

	return hr;
}

// releases queued samples nad empties queues
void COverlay::EmptyMainQueue()
{
	// qpMain
	DbgLog((LOG_TRACE,0,TEXT("					buffered main = %d"), qpMain.size()));
	while(!qpMain.empty())
	{
		DbgLog((LOG_TRACE,0,TEXT("		releasing main")));
		qpMain.front()->Release();
		qpMain.pop();
	}
}

// releases queued samples nad empties queues
void COverlay::EmptyOverlayQueue()
{
	// qpOverlay
	DbgLog((LOG_TRACE,0,TEXT("					buffered overlay = %d"), qpOverlay.size()));
	while(!qpOverlay.empty())
	{
		DbgLog((LOG_TRACE,0,TEXT("		releasing overlay")));
		qpOverlay.front()->Release();
		qpOverlay.pop();
	}
}

HRESULT COverlay::Process(IMediaSample** samples)	{

	HRESULT hr = NOERROR;

	IppStatus status;
	BYTE* pMain_8u_AC4;
	BYTE* pOverlay;
	BYTE* pOut_8u_C3;

	// create a new media sample based on the sample from main input pin
	IMediaSample *pOutSample;

	DbgLog((LOG_TRACE,0,TEXT("before initializeoutputsample")));

	hr = InitializeOutputSample(samples[0], &pOutSample);
	if(FAILED(hr)) return hr;
	
	// get the addresses of the actual data (buffers) and set IPL headers
	samples[0]->GetPointer( &pMain_8u_AC4 );
	samples[1]->GetPointer( &pOverlay );
	pOutSample->GetPointer( &pOut_8u_C3 );
	cvSetData( pOutputCV_C3, pOut_8u_C3, step_8u_C3 );

		// copy main buffer to output
	status = ippiCopy_8u_AC4C3R(pMain_8u_AC4, step_8u_AC4, pOut_8u_C3, step_8u_C3, frameSize);

		// do overlay
	// we need an explicit cast to array
	CMovingObject* objects = ( CMovingObject* )pOverlay;

	// displaying ID'ed boxes with predictions
	for( int i = 0; i<inputAllocProps.cbBuffer; ++i )
	{
		// this marks the end of valid objects
		if (objects[i].rect.height == 0) break;


		CvScalar color = CV_RGB(0,0,0);
		CvPoint p, q;
		p.x = ( int )objects[i].rect.x;
		p.y = ( int )objects[i].rect.y;
		q.x = ( int )( objects[i].rect.x + objects[i].rect.width );
		q.y = ( int )( objects[i].rect.y + objects[i].rect.height );
		
		cvRectangle(  pOutputCV_C3, p, q, color, 2 );
		
		// ID tags
		CvPoint r;
		r.x = ( int )( objects[i].rect.x + objects[i].rect.width + 2);
		r.y = ( int )( objects[i].rect.y - 5 );
		CvFont font;
		cvInitFont( &font, CV_FONT_HERSHEY_PLAIN, 1.2, 1.2, 0, 2, 8 );
			// numbers: double hscale, double vscale, double shear, int thickness, int line_type
		char IDtag[5];
		_itoa( objects[i].ID, IDtag, 10 );
		cvPutText( pOutputCV_C3, IDtag, r, &font, color );	
		

		// we want red and green marks depending on a simple condition based on object speed
		if(objects[i].velocityX < 0)
			color = colorList[0];
		else
			if(objects[i].velocityX > 0)
				color = colorList[1];
			else
				color = colorList[2];

//		CvPoint p, q;
		p.x = ( int )objects[i].rect.x;
		p.y = ( int )objects[i].rect.y;
		q.x = ( int )( objects[i].rect.x + objects[i].rect.width );
		q.y = ( int )( objects[i].rect.y + objects[i].rect.height );
		
		cvRectangle(  pOutputCV_C3, p, q, color, 1 );
		
		// ID tags
//		CvPoint r;
		r.x = ( int )( objects[i].rect.x + objects[i].rect.width + 2);
		r.y = ( int )( objects[i].rect.y - 5 );
//		CvFont font;
		cvInitFont( &font, CV_FONT_HERSHEY_PLAIN, 1.2, 1.2, 0, 1, 8 );
			// numbers: double hscale, double vscale, double shear, int thickness, int line_type
		//char IDtag[5];
		//_itoa( objects[i].ID, IDtag, 10 );
		cvPutText( pOutputCV_C3, IDtag, r, &font, color );	
		
		// centers
		p.x = ( int )objects[i].center.x;
		p.y = ( int )objects[i].center.y;
		cvLine( pOutputCV_C3, p,p,  color, 2, CV_AA, 0 );
	
		// predictions
		q.x = ( int )objects[i].predictedX;		
		q.y = ( int )objects[i].predictedY;
		cvLine( pOutputCV_C3, p, q, color, 1, CV_AA, 0 );			
	}

	// it will get addreffed inside if needed
	hr = outputPin->Deliver(pOutSample);
	return hr;

}
/*
STDMETHODIMP COverlay::Stop()	{
	
	DbgLog((LOG_TRACE,0,TEXT("Stop: claiming section")));
	// claim the section before decommiting input pins immediately
    CAutoLock lock(&m_csMultiIO);

	  // stop if input pin or  at least one output pin disconnected
    if (inputOverlayPin == NULL || !inputOverlayPin->IsConnected() ||
		inputMainPin == NULL || !inputMainPin->IsConnected() ||
		outputPin == NULL || !outputPin->IsConnected() )
	{
		m_State = State_Stopped;
		m_bEOSDelivered = FALSE;

		DbgLog((LOG_TRACE,0,TEXT("Stop: disconnected")));

		return NOERROR;
    }

	DbgLog((LOG_TRACE,0,TEXT("Stop: stopping base")));
	HRESULT hr = CBaseFilter::Stop();
	DbgLog((LOG_TRACE,0,TEXT("Stop: stopping base out")));
	if(FAILED(hr)) return hr;
	DbgLog((LOG_TRACE,0,TEXT("Stop: stopping base out 2")));

    return NOERROR;
}


STDMETHODIMP COverlay::Pause()	{

	CAutoLock lock(&m_csMultiIO);

    HRESULT hr = NOERROR;

	if (SUCCEEDED(hr)) {
	    hr = CBaseFilter::Pause();
    }

    m_bSampleSkipped = FALSE;
    m_bQualityChanged = FALSE;
    return hr;
}

STDMETHODIMP COverlay::Run(REFERENCE_TIME tStart)		{

	CAutoLock lock(&m_csMultiIO);

	// remember the stream time offset
	m_tStart = tStart;

	if (m_State == State_Stopped){
		HRESULT hr = Pause();

		if (FAILED(hr)) {
			return hr;
		}
	}
	
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
*/


/*bla
STDMETHODIMP COverlay::Stop()	{

	DbgLog((LOG_TRACE,0,TEXT("		Stop")));

    CAutoLock lock(&m_csMultiIO);

	  // stop if we are not completely connected
    if (inputOverlayPin == NULL || !inputOverlayPin->IsConnected() ||
		inputMainPin == NULL || !inputMainPin->IsConnected() ||
		outputPin == NULL || !outputPin->IsConnected())	{
		m_State = State_Stopped;
		m_bEOSDelivered = FALSE;

		return NOERROR;
    }

	HRESULT hr = CBaseFilter::Stop();
	if(FAILED(hr)) return hr;

    return NOERROR;
}


STDMETHODIMP COverlay::Pause()	{

	DbgLog((LOG_TRACE,0,TEXT("		Pause")));

	CAutoLock lock(&m_csMultiIO);

    HRESULT hr = NOERROR;

    // If we have no input pin or it isn't yet connected then when we are
    // asked to pause we deliver an end of stream to the downstream filter.
    // This makes sure that it doesn't sit there forever waiting for
    // samples which we cannot ever deliver without an input connection.
/*
    if (inputOverlayPin == NULL || !inputOverlayPin->IsConnected() ||
		inputMainPin == NULL || !inputMainPin->IsConnected())	{

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
/*bla	if (SUCCEEDED(hr)) {
	    hr = CBaseFilter::Pause();
    }

    m_bSampleSkipped = FALSE;
    m_bQualityChanged = FALSE;
    return hr;
}

STDMETHODIMP COverlay::Run(REFERENCE_TIME tStart)		{

	DbgLog((LOG_TRACE,0,TEXT("		Run")));

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
*/