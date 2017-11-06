#ifndef MORPHOLOGICALS_H
#define MORPHOLOGICALS_H

#ifndef STREAMS_H
	#include <Streams.h>    // DirectShow (includes windows.h)
	#define STREAMS_H
#endif

#ifndef INITGUID_H
	#include <initguid.h>   // declares DEFINE_GUID to declare an EXTERN_C const.
	#define INITGUID_H
#endif

#ifndef IPP_H
	#include <ipp.h>
	#define IPP_H
#endif

#ifndef CV_H
	#include <cv.h>			// OpenCV
	#define CV_H
#endif

#define FILTER_NAME L"Morphologicals"

// {C51C2FC5-69C8-4233-B2B5-CDA18B86A872}
DEFINE_GUID(CLSID_MORPHOLOGICALS, 
0xc51c2fc5, 0x69c8, 0x4233, 0xb2, 0xb5, 0xcd, 0xa1, 0x8b, 0x86, 0xa8, 0x72);

// {C7654E95-DF70-47d9-BE15-42206C0F6DAC}
DEFINE_GUID(MEDIASUBTYPE_8BitMonochrome_internal, 
0xc7654e95, 0xdf70, 0x47d9, 0xbe, 0x15, 0x42, 0x20, 0x6c, 0xf, 0x6d, 0xac);

// {2DFA1B73-816E-46de-8226-382DB230BFFE}
DEFINE_GUID(CLSID_MotionTrackingCategory, 
0x2dfa1b73, 0x816e, 0x46de, 0x82, 0x26, 0x38, 0x2d, 0xb2, 0x30, 0xbf, 0xfe);


class CMorphologicalsFilter : public CTransformFilter	{

	// allow access to pointers to pins to following classes:
	friend class CIPPAllocator_8u_C1;
	friend class CIPPAllocatorOut_8u_C1;
	friend class CPinInput;
	friend class CPinOutput;

	public:
		CMorphologicalsFilter( TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr ) :
		  CTransformFilter( tszName, punk, CLSID_MORPHOLOGICALS ),
		  	// allocator inits
			pInputCV_C1(NULL),
			pOutputCV_C1(NULL),
			pHelperCV_C1(NULL),
			pHelper2CV_C1(NULL),
			pHelper_8u_C1(NULL),
			pHelper2_8u_C1(NULL),
			kernelCV(NULL)
		  {

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

			  // parameters - should be set from the property pages
			  m_params.iterationsOpen = 1;
			  m_params.iterationsClose = 3;
		}

		~CMorphologicalsFilter()	{
			delete outputMediaType;
		}

		HRESULT CheckInputType( const CMediaType *mtIn );
		HRESULT GetMediaType( int iPosition, CMediaType *pMediaType );
		HRESULT CheckTransform( const CMediaType *mtIn, const CMediaType *mtOut );
		HRESULT DecideBufferSize( IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *ppropInputRequest );

		// initialize stuff such as strides and sample dimensions in pixels, everythingž
		// what our allocators or sample processing methods need
		HRESULT InitializeProcessParams(VIDEOINFOHEADER* pVih);

		static CUnknown* WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

		CMediaType* outputMediaType;

		typedef struct {
			int iterationsOpen;
			int iterationsClose;
			// put kernel parameters here also

		} Parameters;
		Parameters m_params;
		
	protected:
		// custom output pin with custom allocator gets created here
		CBasePin* GetPin( int n );

		// ALLOCATOR_PROPERTIES
		ALLOCATOR_PROPERTIES inputAllocProps;
		ALLOCATOR_PROPERTIES outputAllocProps;

	private:
		IppiSize frameSize;
		CvSize	frameSizeCV;
		int stride_8u_C1, stride_8u_C3;
		int bytesPerPixel_8u_C1;
		int step_8u_C1;
		static const int IPPalignment = 32;
		static const int DIBalignment = 4;

		// We are using OpenCV that is compatibile with old Intel IPL. Although IPL is used no more,
		// we will be using IPL data types. 
		IplImage*	pInputCV_C1;
		IplImage*	pOutputCV_C1;
		IplImage*	pHelperCV_C1;
		IplImage*	pHelper2CV_C1;
		// for morphological operations
		IplConvKernel* kernelCV;

		// additional alloc
		BYTE* pHelper_8u_C1;
		BYTE* pHelper2_8u_C1;

		HRESULT Transform( IMediaSample *pSource, IMediaSample *pDest );
};

#endif