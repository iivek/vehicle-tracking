#pragma warning(disable:4312)

#ifndef TEST_H
	#include "test.h"
#endif


HRESULT CTest :: GetMediaType(int iPosition, CMediaType *pOutputMediaType, CBaseMultiIOOutputPin caller)	{

	//bla
	CAutoLock lock(m_pLock);
	//DbgLog((LOG_TRACE,0,TEXT("GetMediaType")));

	// This method should be called only if input pin is connected
    if (m_inputPins[0]->IsConnected() == FALSE) {
        return E_UNEXPECTED;
    }

    // This should never happen
    if (iPosition < 0) {
        return E_INVALIDARG;
    }

    // Do we have more items to offer
    if (iPosition > 0) {
		//DbgLog((LOG_TRACE,0,TEXT("No more items")));
        return VFW_S_NO_MORE_ITEMS;
    }

	HRESULT hr = m_inputPins[0]->ConnectionMediaType(pOutputMediaType);
	if (FAILED(hr))		{
		return hr;
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

	// preferred output media subtype is MEDIASUBTYPE_8BitMonochrome_internal...
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

	//DbgLog((LOG_TRACE,0,TEXT("yes more items")));

    return NOERROR;
}



HRESULT CTest :: DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProp, CBaseMultiIOOutputPin caller)	{

	//CAutoLock lock(m_pLock);
	if (!m_inputPins[0]->IsConnected())	{
		return E_UNEXPECTED;
	}

	CheckPointer(pAlloc,E_POINTER);
    CheckPointer(pProp,E_POINTER);
    HRESULT hr = NOERROR;

	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) m_inputPins[0]->CurrentMediaType().Format();

	bytesperpixel_8u_C1 = 8;
	stride_8u_C1 = (pVih->bmiHeader.biWidth*bytesperpixel_8u_C1)%IPPalignment;
	stride_32f_C1 = 0;
	step_8u_C1 = pVih->bmiHeader.biWidth*bytesperpixel_8u_C1/8+stride_8u_C1;
	step_32f_C1 = (step_8u_C1 - step_8u_C1%32)*4;

	pProp->cbBuffer = outputAllocProps.cbBuffer = pVih->bmiHeader.biHeight*
		(pVih->bmiHeader.biWidth * bytesperpixel_8u_C3 + stride_8u_C3);	

	pProp->cBuffers = outputAllocProps.cBuffers = 1;
	pProp->cbAlign = outputAllocProps.cbAlign = 32;
	pProp->cbPrefix = outputAllocProps.cbPrefix = 0;
	// if upstream filter doesn't ask us for our allocator requirements, inputAllocProps
	// won't get set up (inftee doesn't do that, for example)
	// pProp->cbAlign = outputAllocProps.cbAlign = inputAllocProps.cbAlign;
	// pProp->cbPrefix = outputAllocProps.cbPrefix = inputAllocProps.cbPrefix;
	//
    
	ASSERT(pProp->cbBuffer);
   
	ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProp,&Actual);
    if (FAILED(hr)) {
        return hr;
    }

    if (pProp->cBuffers > Actual.cBuffers ||
            pProp->cbBuffer > Actual.cbBuffer) {
                return E_FAIL;
				// will never happen because our allocator is able to give us what we requested
    }

	DbgLog((LOG_TRACE,0,TEXT("DecideBufferSize no error")));
	
    return NOERROR;
}

HRESULT CTest :: CheckInputType( const CMediaType *mtIn )	{

	CAutoLock lock(m_pLock);
	//DbgLog((LOG_TRACE,0,TEXT("CheckInputType")));

	if ((mtIn->majortype != MEDIATYPE_Video) ||
		(mtIn->subtype != MEDIASUBTYPE_RGB24) ||
		(mtIn->formattype != FORMAT_VideoInfo) ||		// is this really necessary?
		(mtIn->cbFormat < sizeof(VIDEOINFOHEADER)))		// do all CMediaTypes have a VIH format?
	{
		//DbgLog((LOG_TRACE,0,TEXT("Probljem1")));
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	// reinterpret cast is safe because we already checked formattype and cbFormat
	VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(mtIn->pbFormat);

	// at this timepoint it's convenient to calculate strides and what our allocators might need
	bytesperpixel_8u_C1 = 8;
	bytesperpixel_8u_C3 = 24;
	stride_8u_C1 = (pVih->bmiHeader.biWidth*bytesperpixel_8u_C1)%IPPalignment;
	stride_8u_C3 = (pVih->bmiHeader.biWidth*bytesperpixel_8u_C3)%IPPalignment;
	step_8u_C1 = pVih->bmiHeader.biWidth*bytesperpixel_8u_C1/8+stride_8u_C1;
	step_8u_C3 = pVih->bmiHeader.biWidth*bytesperpixel_8u_C3/8+stride_8u_C3;

	// this an ok place to initialize our alocator requirements

	DbgLog((LOG_TRACE,0,TEXT("PROSHLO!")));
	return S_OK;

}

// we will implement an inftee-like filter that passes streams from the first input pin
// to all output pins
HRESULT CTest :: Receive(IMediaSample *pSample, int nIndex )	{

	CAutoLock lock(m_pLock);

	HRESULT hr = NOERROR;

	DbgLog((LOG_TRACE,0,TEXT("Receive, index = %d"), nIndex));

	if(nIndex == 0)	{

		OUTPUT_PINS::iterator iter = m_outputPins.begin();
		while(iter != m_outputPins.end()) {
			if(iter->second != NULL)	{
				if(iter->second->IsConnected())	{
					hr = iter->second->Deliver(pSample);
					if(FAILED(hr))	{
						DbgLog((LOG_TRACE,0,TEXT("failed")));
						return hr;
					}
				}
			}
			++iter;
		}
	}
	

	return hr;	
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
	&CLSID_TEST				// clsID
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
			&CLSID_TEST,
			CTest::CreateInstance,
			NULL,
			&sudExtractY
		}
    };

int g_cTemplates = sizeof(g_Templates)/sizeof(g_Templates[0]);


// Provide the way for COM to create a filter object
CUnknown* WINAPI CTest :: CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
	CheckPointer(phr,NULL);
	
	CTest* pNewObject = new CTest( NAME("TEST"), punk, phr );

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
		CLSID_TEST,							// Filter CLSID. 
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
		FILTER_NAME, CLSID_TEST);

	pFM2->Release();
	return hr;
}