#ifndef MORPHOLOGICALS_H
	#include "morphologicals.h"
#endif

#ifndef PINS_H
	#include "pins.h"
#endif

#ifndef MATH_H
	#include <math.h>
	#define MATH_H
#endif


HRESULT CMorphologicalsFilter :: InitializeProcessParams(VIDEOINFOHEADER* pVih)	{
	
	// these will be returned by the ippiMalloc in the allocator, but we need them before the allocation
	// our internal monochrome datatype is aligned to 32 bits, so we have to calculate strides and everything
	bytesPerPixel_8u_C1 = 1;
	int remainder = (pVih->bmiHeader.biWidth*bytesPerPixel_8u_C1)%(DIBalignment);
	if (remainder != 0)
		stride_8u_C1 = DIBalignment - remainder;
	else
		stride_8u_C1 = 0;
	step_8u_C1 = pVih->bmiHeader.biWidth*bytesPerPixel_8u_C1+stride_8u_C1;

	frameSize.height = pVih->bmiHeader.biHeight;
	frameSize.width = pVih->bmiHeader.biWidth;
	frameSizeCV = cvSize(pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight);	//it would perhaps be better if we used pointers to the same integers here

	DbgLog((LOG_TRACE,0,TEXT("step = %d"), step_8u_C1));

	return NOERROR;
}

HRESULT CMorphologicalsFilter :: CheckInputType(const CMediaType *mtIn)
{	
	CheckPointer(mtIn,E_POINTER);

	DbgLog((LOG_TRACE,0,TEXT("CheckInputType")));

	if ((mtIn->majortype != MEDIATYPE_Video) ||
		(mtIn->subtype != MEDIASUBTYPE_8BitMonochrome_internal) ||
		(mtIn->formattype != FORMAT_VideoInfo) ||		// is this really necessary?
		(mtIn->cbFormat < sizeof(VIDEOINFOHEADER)))		// do all CMediaTypes have a VIH format?
	{
		DbgLog((LOG_TRACE,0,TEXT("Probljem1")));
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	// reinterpret cast is safe because we already checked formattype and cbFormat
	VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(mtIn->pbFormat);

	InitializeProcessParams(pVih);

	DbgLog((LOG_TRACE,0,TEXT("PROSHLO!")));
	return S_OK;
}


HRESULT CMorphologicalsFilter :: GetMediaType(int iPosition, CMediaType *pOutputMediaType)	{

	DbgLog((LOG_TRACE,0,TEXT("GetMediaType")));

	// This method should be called only if input pin is connected
    if (m_pInput->IsConnected() == FALSE) {
        return E_UNEXPECTED;
    }

    // This should never happen
    if (iPosition < 0) {
        return E_INVALIDARG;
    }

    // Do we have more items to offer
    if (iPosition > 0) {
		DbgLog((LOG_TRACE,0,TEXT("No more items")));
        return VFW_S_NO_MORE_ITEMS;
    }

	HRESULT hr = m_pInput->ConnectionMediaType(pOutputMediaType);
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

	DbgLog((LOG_TRACE,0,TEXT("yes more items")));

    return NOERROR;
}


CBasePin* CMorphologicalsFilter :: GetPin( int n )	{

/*	HRESULT hr;

	//DbgLog((LOG_TRACE,0,TEXT("GetPin")));

	switch ( n )	{
		case 0:
			// Create an input pin if necessary
			if ( m_pInput == NULL )	{
				m_pInput = new CPinInput( this, &hr );      
				if ( m_pInput == NULL )	{
					return NULL;
				}
				DbgLog((LOG_TRACE,0,TEXT("InputPin created")));
				return m_pInput;
			}
			else	{
				return m_pInput;
			}
			break;
		case 1:
			// Create an output pin if necessary
			if ( m_pOutput == NULL )	{
				m_pOutput = new CPinOutput( this, &hr );      
				if ( m_pOutput == NULL )	{
					return NULL;
				}
				DbgLog((LOG_TRACE,0,TEXT("OutputPin created")));
				return m_pOutput;
			}
			else	{
				return m_pOutput;
			}
			break;
		default:
			return NULL;
      }
	  */
		// we need to have input pin and output pin at the same time, so this upper commented stuff
	// might not work. here's the way it's done in CTransformFilter::GetPin(int)
	HRESULT hr = S_OK;

    // Create an input pin if necessary

    if (m_pInput == NULL) {

        m_pInput = new CPinInput( this, &hr );     

        //  Can't fail
        ASSERT(SUCCEEDED(hr));
        if (m_pInput == NULL) {
            return NULL;
        }
		m_pOutput = new CPinOutput( this, &hr );

        // Can't fail
        ASSERT(SUCCEEDED(hr));
        if (m_pOutput == NULL) {
            delete m_pInput;
            m_pInput = NULL;
        }
		
    }

    // Return the appropriate pin

    if (n == 0) {
        return m_pInput;
    } else
    if (n == 1) {
        return m_pOutput;
    } else {
        return NULL;
    }
}

// output ALLOCATOR_PROPERTIES will also be set here, depending on input DIB format and 8 bit
// monochrome submediaformat we want to force as output
HRESULT CMorphologicalsFilter :: CheckTransform( const CMediaType *mtIn, const CMediaType *mtOut )	{

	// our terms of connection: input has to be a valid and DIBs have to be of same dimensions

    CheckPointer(mtIn,E_POINTER);
    CheckPointer(mtOut,E_POINTER);

	DbgLog((LOG_TRACE,0,TEXT("CheckTransform")));

	VIDEOINFOHEADER* pVihIn = (VIDEOINFOHEADER*) mtIn->Format();
	VIDEOINFOHEADER* pVihOut = (VIDEOINFOHEADER*) mtOut->Format();
	if(pVihIn == NULL)	{
		DbgLog((LOG_TRACE,0,TEXT("Probljem2")));
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	if(mtIn->subtype != MEDIASUBTYPE_8BitMonochrome_internal)	{
		DbgLog((LOG_TRACE,0,TEXT("Probljem3")));
		return VFW_E_TYPE_NOT_ACCEPTED;
	}
	if(mtOut->subtype != MEDIASUBTYPE_8BitMonochrome_internal)	{
		DbgLog((LOG_TRACE,0,TEXT("Probljem4")));
		return VFW_E_TYPE_NOT_ACCEPTED;
	}
	
	if(pVihIn->bmiHeader.biWidth > pVihOut->bmiHeader.biWidth
		&& pVihIn->bmiHeader.biHeight > pVihOut->bmiHeader.biHeight)	{

		DbgLog((LOG_TRACE,0,TEXT("Probljem5, %d, %d, %d, %d"),
			pVihIn->bmiHeader.biWidth, pVihIn->bmiHeader.biHeight, 
			pVihOut->bmiHeader.biWidth,pVihOut->bmiHeader.biHeight
			));
		return VFW_E_TYPE_NOT_ACCEPTED;
		}

	DbgLog((LOG_TRACE,0,TEXT("CheckTransform OK")));

	return S_OK;
}


HRESULT CMorphologicalsFilter :: DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProp)	{

	DbgLog((LOG_TRACE,0,TEXT("DecideBufferSize")));

	CheckPointer(pAlloc,E_POINTER);
	CheckPointer(pProp,E_POINTER);

	if (!m_pInput->IsConnected())	{
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
        return hr;
    }

    if (pProp->cBuffers != Actual.cBuffers || pProp->cbBuffer > Actual.cbBuffer) {
                return E_FAIL;
    }
	
    return NOERROR;
}


HRESULT CMorphologicalsFilter :: Transform( IMediaSample* pSource, IMediaSample* pDest )	{

	CAutoLock lock(&m_csReceive);

	Ipp8u* pSourceBuffer;
	Ipp8u* pDestBuffer;
	// pointer to the buffers
	pSource->GetPointer( &pSourceBuffer );
	pDest->GetPointer( &pDestBuffer );

	cvSetData( pInputCV_C1, pSourceBuffer, step_8u_C1 );
	cvSetData( pOutputCV_C1, pDestBuffer, step_8u_C1 );

	// open to remove small objects
	cvMorphologyEx( pInputCV_C1, pHelperCV_C1, pHelper2CV_C1, kernelCV, CV_MOP_OPEN, m_params.iterationsOpen );
	// close to melt blobs together
	cvMorphologyEx( pHelperCV_C1, pOutputCV_C1, pHelper2CV_C1, kernelCV, CV_MOP_CLOSE, m_params.iterationsClose );

/*	ippiThreshold_GTVal_8u_C1IR(	pDestBuffer,
									step_8u_C1,
									frameSize,
									//m_params.threshold,
									20,
									255);
*/
	return NOERROR;
}


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
	&CLSID_MORPHOLOGICALS				// clsID
	, FILTER_NAME				// strName
	, MERIT_DO_NOT_USE			// dwMerit
	, 2							// nPins
	, psudPins					// lpPin
};

REGFILTER2 rf2FilterReg = {
    1,                    // Version 1 (no pin mediums or pin category).
    MERIT_DO_NOT_USE,     // Merit.
    2,                    // Number of pins.
    psudPins              // Pointer to pin information.
};

// Class factory template - needed for the CreateInstance mechanism to
// register COM objects in our dynamic library
CFactoryTemplate g_Templates[]=
    {   
		{
			FILTER_NAME,
			&CLSID_MORPHOLOGICALS,
			CMorphologicalsFilter::CreateInstance,
			NULL,
			&sudExtractY
		}
    };

int g_cTemplates = sizeof(g_Templates)/sizeof(g_Templates[0]);


// Provide the way for COM to create a filter object
CUnknown* WINAPI CMorphologicalsFilter :: CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
	CheckPointer(phr,NULL);
	
	CMorphologicalsFilter* pNewObject = new CMorphologicalsFilter( NAME("Morhps"), punk, phr );

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
		CLSID_MORPHOLOGICALS,							// Filter CLSID. 
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
		FILTER_NAME, CLSID_MORPHOLOGICALS);

	pFM2->Release();
	return hr;
}