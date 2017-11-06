#ifndef FAME2D_H
	#include "fame2D.h"
#endif

#ifndef PINS_H
	#include "pins.h"
#endif

#ifndef MATH_H
	#include <math.h>
	#define MATH_H
#endif


HRESULT CFame2D :: InitializeProcessParams(VIDEOINFOHEADER* pVih)	{
	bitsperpixel_8u_C1 = 8;
	bitsperpixel_32f_C1 = 32;
//	bitsperpixel_8u_C3 = 24;
	stride_8u_C1 = (pVih->bmiHeader.biWidth*bitsperpixel_8u_C1)%IPPalignment;
//	stride_8u_C3 = (pVih->bmiHeader.biWidth*bitsperpixel_8u_C3)%IPPalignment;
	step_8u_C1 = pVih->bmiHeader.biWidth*bitsperpixel_8u_C1/8+stride_8u_C1;
//	step_8u_C3 = pVih->bmiHeader.biWidth*bitsperpixel_8u_C3/8+stride_8u_C3;
	step_32f_C1 = (step_8u_C1 - step_8u_C1%32)*4;
	stride_32f_C1 = 0;
	frameSize.height = pVih->bmiHeader.biHeight;
	frameSize.width = pVih->bmiHeader.biWidth;

	return NOERROR;
}

HRESULT CFame2D :: CheckInputType(const CMediaType *mtIn)
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


HRESULT CFame2D :: GetMediaType(int iPosition, CMediaType *pOutputMediaType)	{

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


CBasePin* CFame2D :: GetPin( int n )	{

	HRESULT hr;

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
}

// output ALLOCATOR_PROPERTIES will also be set here, depending on input DIB format and 8 bit
// monochrome submediaformat we want to force as output
HRESULT CFame2D :: CheckTransform( const CMediaType *mtIn, const CMediaType *mtOut )	{

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


HRESULT CFame2D :: DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProp)	{

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


HRESULT CFame2D :: Transform( IMediaSample* pSource, IMediaSample* pDest )	{

	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) this->m_pInput->CurrentMediaType().Format();

	CAutoLock lock(&m_csReceive);

	BYTE* pSourceData;
	BYTE* pDestData;
	// pointer to the buffers
	pSource->GetPointer( &pSourceData );
	pDest->GetPointer( &pDestData );

	DbgLog((LOG_TRACE,0,TEXT("transformishem, %d, %d"), pSource, pSourceData));

	( this->*fpDecideFAME )( pSourceData, pDestData );

	DbgLog((LOG_TRACE,0,TEXT("transforsdfgsdfmishem, %d, %d"), pSource, pSourceData));

	ippiCopy_8u_C1R( pBackground_8u_C1, step_8u_C1, pDestData, step_8u_C1, frameSize );

	return NOERROR;
}


// first part of FAME, Fast Algorithm for Median Estimation
HRESULT CFame2D::FAME_initialization( Ipp8u* pSourceData, Ipp8u* pDestData )	{

	BYTE* pSrcTemp = pSourceData;
	Ipp32f* pStepTemp = pStep;
	Ipp32f* pBackgroundTemp_32f_C1 = pBackground_32f_C1;

	for( int row=0; row<frameSize.height; ++row )
	{
		for( int col=0; col<frameSize.width; ++col)
		{

			*pBackgroundTemp_32f_C1 = (float)*pSrcTemp;
			step2 = (float)( *pSrcTemp/2 );
			if(  step2 < params.minStep )
				*pStepTemp = params.minStep;
			else
				*pStepTemp = step2;

			++pStepTemp;
			++pSrcTemp;
			++pBackgroundTemp_32f_C1;
		}
		pSrcTemp += stride_8u_C1;
		pBackgroundTemp_32f_C1 += stride_32f_C1;
	}

	ippiConvert_32f8u_C1R( pBackground_32f_C1, step_32f_C1,
							pBackground_8u_C1, step_8u_C1, 
							frameSize, ippRndNear);

	// set function pointer to the itarative part of the algorithm
	fpDecideFAME = &CFame2D::FAME_iteration;

	return S_OK;
}


// second part of FAME, Fast Algorithm for Median Estimation
HRESULT CFame2D::FAME_iteration( Ipp8u* pSourceData, Ipp8u* pDestData )	{
	
	BYTE* pSrcTemp = pSourceData;
	float* pStepTemp = pStep;
	float* pBackgroundTemp_32f_C1 = pBackground_32f_C1;

	for( int row=0; row<frameSize.height; ++row )	{
		for( int col=0; col<frameSize.width; ++col)	{
			if( *pBackgroundTemp_32f_C1 > *pSrcTemp )
				*pBackgroundTemp_32f_C1 -= *pStepTemp;
			else
				if( *pBackgroundTemp_32f_C1 < *pSrcTemp ) 
					*pBackgroundTemp_32f_C1 += *pStepTemp;

			if( fabs( *pSrcTemp-*pBackgroundTemp_32f_C1 ) < *pStepTemp )
				*pStepTemp = *pStepTemp/2;

			// give the algorithm a windowing flavor
			*pStepTemp *= float( 1+params.epsilon );

			++pStepTemp;
			++pSrcTemp;
			++pBackgroundTemp_32f_C1;
		}
		pSrcTemp += stride_8u_C1;
	}

	ippiConvert_32f8u_C1R( pBackground_32f_C1, step_32f_C1,
							pBackground_8u_C1, step_8u_C1, 
							frameSize, ippRndNear);

	return S_OK;
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
	&CLSID_FAME2D				// clsID
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
			&CLSID_FAME2D,
			CFame2D::CreateInstance,
			NULL,
			&sudExtractY
		}
    };

int g_cTemplates = sizeof(g_Templates)/sizeof(g_Templates[0]);


// Provide the way for COM to create a filter object
CUnknown* WINAPI CFame2D :: CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
	CheckPointer(phr,NULL);
	
	CFame2D* pNewObject = new CFame2D( NAME("MEDIAN"), punk, phr );

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
		CLSID_FAME2D,							// Filter CLSID. 
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
		FILTER_NAME, CLSID_FAME2D);

	pFM2->Release();
	return hr;
}