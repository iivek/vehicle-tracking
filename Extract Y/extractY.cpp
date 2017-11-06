#include "extracty.h"
#include "Pins.h"


HRESULT CExtractY :: CheckInputType(const CMediaType *mtIn)
{	
	CAutoLock cAutolock(&lock);

	DbgLog((LOG_TRACE,0,TEXT("CheckInputType")));

	CheckPointer(mtIn,E_POINTER);

	if (*mtIn->FormatType() != FORMAT_VideoInfo)	{
		DbgLog((LOG_TRACE,0,TEXT("CheckInputType probljem 1")));
		return E_INVALIDARG;
	}
		
	if ((mtIn->majortype != MEDIATYPE_Video) ||
		(mtIn->subtype != MEDIASUBTYPE_RGB32) )
	{
		DbgLog((LOG_TRACE,0,TEXT("CheckInputType probljem 2")));
		DbgLog((LOG_TRACE,0,TEXT("Probljem14")));
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	// reinterpret cast is safe because we already checked formattype and cbFormat
	VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(mtIn->pbFormat);
	if ((pVih->bmiHeader.biBitCount != 32) ||
		(pVih->bmiHeader.biCompression != BI_RGB))
	{
		DbgLog((LOG_TRACE,0,TEXT("CheckInputType probljem 3")));
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	// at this timepoint it's convenient to calculate strides and what our allocators might need

	// step 8u_C1 and step 8u_AC4 will be calculated and retuned by ippiMalloc from the allocator class,
	// but we need to have them before that
	
	// our internal monochrome datatype is aligned to 32 bits, so we have to calculate strides and everything
	bytesPerPixel_8u_C1 = 1;
	int remainder = (pVih->bmiHeader.biWidth*bytesPerPixel_8u_C1)%DIBalignment;
	if (remainder != 0)
		stride_8u_C1 = DIBalignment - remainder;
	else
		stride_8u_C1 = 0;
	step_8u_C1 = pVih->bmiHeader.biWidth*bytesPerPixel_8u_C1+stride_8u_C1;

	DbgLog((LOG_TRACE,0,TEXT("step 8u_C1 = %d"), step_8u_C1));
	
	// we force use of RGB32, but we have to find out what the stride is
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
	
	DbgLog((LOG_TRACE,0,TEXT("step_8u_AC4 = %d, stride = %d"), step_8u_AC4, stride_8u_AC4));

	frameSize.height = pVih->bmiHeader.biHeight;
	frameSize.width = pVih->bmiHeader.biWidth;

	return S_OK;
}


// this gets called by our COutputPin::GetMediaType
// our output pin will have a MEDIASUBTYPE_8BitMonochrome_internal
HRESULT CExtractY :: GetMediaType(int iPosition, CMediaType *pOutputMediaType)	{

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


CBasePin* CExtractY :: GetPin( int n )	{

/*	HRESULT hr;

	switch ( n )	{
		case 0:
			// Create an input pin if necessary
			if ( m_pInput == NULL )	{
				m_pInput = new CPinInput( (CTransformFilter*)this, &hr );      
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
				m_pOutput = new CPinOutput( (CTransformFilter*)this, &hr );      
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

        m_pInput = new CPinInput( (CTransformFilter*)this, &hr );     

        //  Can't fail
        ASSERT(SUCCEEDED(hr));
        if (m_pInput == NULL) {
            return NULL;
        }
		m_pOutput = new CPinOutput( (CTransformFilter*)this, &hr );

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


HRESULT CExtractY :: CheckTransform( const CMediaType *mtIn, const CMediaType *mtOut )	{

	DbgLog((LOG_TRACE,0,TEXT("CheckTransform")));

	// input has to be a valid UYVY and DIBs have to be of same dimensions

	VIDEOINFOHEADER* pVihIn = (VIDEOINFOHEADER*) mtIn->Format();
	VIDEOINFOHEADER* pVihOut = (VIDEOINFOHEADER*) mtIn->Format();
	if(pVihIn == NULL)	{
		DbgLog((LOG_TRACE,0,TEXT("Probljem10")));
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	if(mtIn->subtype != MEDIASUBTYPE_RGB32)	{
		DbgLog((LOG_TRACE,0,TEXT("Probljem11")));
		return VFW_E_TYPE_NOT_ACCEPTED;
	}
	if(mtOut->subtype != MEDIASUBTYPE_8BitMonochrome_internal)	{
		DbgLog((LOG_TRACE,0,TEXT("Probljem12")));
		return VFW_E_TYPE_NOT_ACCEPTED;
	}
	
	if(pVihIn->bmiHeader.biWidth > pVihOut->bmiHeader.biWidth
		&& pVihIn->bmiHeader.biHeight > pVihOut->bmiHeader.biHeight){
			DbgLog((LOG_TRACE,0,TEXT("Probljem13")));
		return VFW_E_TYPE_NOT_ACCEPTED;
		}

	return S_OK;
}

// output ALLOCATOR_PROPERTIES will be set here, depending on input DIB format and 8 bit
// monochrome submediaformat we want to force as output
HRESULT CExtractY :: DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProp)	{

	DbgLog((LOG_TRACE,0,TEXT("DecideBufferSize")));

	if (!m_pInput->IsConnected())	{
		DbgLog((LOG_TRACE,0,TEXT("unconnected failed")));
		return E_UNEXPECTED;
	}

	CheckPointer(pAlloc,E_POINTER);
	DbgLog((LOG_TRACE,0,TEXT("DecideBufferSize2")));
    CheckPointer(pProp,E_POINTER);
	DbgLog((LOG_TRACE,0,TEXT("DecideBufferSize3")));

	
    HRESULT hr = NOERROR;
	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) this->m_pInput->CurrentMediaType().Format();

	pProp->cbBuffer = outputAllocProps.cbBuffer = pVih->bmiHeader.biHeight*step_8u_C1;;
	pProp->cBuffers = outputAllocProps.cBuffers = 1;
	pProp->cbAlign = outputAllocProps.cbAlign = 32;
	pProp->cbPrefix = outputAllocProps.cbPrefix = 0;

	ASSERT(pProp->cbBuffer);
	DbgLog((LOG_TRACE,0,TEXT("bla, %d"), pProp->cbBuffer));


	// in SetProperties we will set every entry as proposed here
	ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProp,&Actual);
    if (FAILED(hr)) {
		DbgLog((LOG_TRACE,0,TEXT("setprops failed")));
        return hr;
    }

   /* if (pProp->cBuffers > Actual.cBuffers ||
            pProp->cbBuffer > Actual.cbBuffer) {
                return E_FAIL;
				// will never happen because our allocator is able to give us what we requested
    }*/
	
    return NOERROR;
}


HRESULT CExtractY :: Transform( IMediaSample *pSource, IMediaSample *pDest )	{

	CAutoLock cAutolock(&lock);

	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) this->m_pInput->CurrentMediaType().Format();

	BYTE* pSourceData;
	BYTE* pDestData;
	// pointer to the buffers
	pSource->GetPointer( &pSourceData );
	pDest->GetPointer( &pDestData );

	DbgLog((LOG_TRACE,0,TEXT("transformishem, %d, %d"), pDest, pDestData));

	IppStatus status = ippiRGBToGray_8u_AC4C1R(pSourceData, step_8u_AC4,
							pDestData, step_8u_C1, frameSize);
	
	DbgLog((LOG_TRACE,0,TEXT("%s %d"), ippGetStatusString(status), stride_8u_AC4));
						
	ASSERT(status == ippStsNoErr);

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
  , FALSE               // bMany
  , &CLSID_NULL         // clsConnectsToFilter
  , L""                 // strConnectsToPin
  , 1                   // nTypes
  , &sudPinTypes        // lpTypes
  }
, { L"Output"           // strName
  , FALSE               // bRendered
  , TRUE                // bOutput
  , FALSE               // bZero
  , TRUE               // bMany
  , &CLSID_NULL         // clsConnectsToFilter
  , L""                 // strConnectsToPin
  , 1                   // nTypes
  , &sudPinTypes        // lpTypes
  }
};

const AMOVIESETUP_FILTER sudExtractY =
{ 
	&CLSID_ExtractY		// clsID
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
			&CLSID_ExtractY,
			CExtractY::CreateInstance,
			NULL,
			&sudExtractY
		}
    };

int g_cTemplates = sizeof(g_Templates)/sizeof(g_Templates[0]);


// Provide the way for COM to create a filter object
CUnknown* WINAPI CExtractY :: CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
	CheckPointer(phr,NULL);
	
	CExtractY* pNewObject = new CExtractY( NAME("YUV2Y"), punk, phr );

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
		CLSID_ExtractY,							// Filter CLSID. 
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
		FILTER_NAME, CLSID_ExtractY);

	pFM2->Release();
	return hr;
}