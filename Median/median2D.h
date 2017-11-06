#ifndef MEDIAN2D_H

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

#ifndef MOVMEDIAN_H
	#include "movmedian.h"
	#define MOVMEDIAN_H
#endif

#define FILTER_NAME L"Median2D"

// {26CDFD42-B0B9-4e10-9117-C1C1090040B7}
DEFINE_GUID(CLSID_Median2D,  
0x26cdfd42, 0xb0b9, 0x4e10, 0x91, 0x17, 0xc1, 0xc1, 0x9, 0x0, 0x40, 0xb7);

// {C7654E95-DF70-47d9-BE15-42206C0F6DAC}
DEFINE_GUID(MEDIASUBTYPE_8BitMonochrome_internal, 
0xc7654e95, 0xdf70, 0x47d9, 0xbe, 0x15, 0x42, 0x20, 0x6c, 0xf, 0x6d, 0xac);

// {2DFA1B73-816E-46de-8226-382DB230BFFE}
DEFINE_GUID(CLSID_MotionTrackingCategory, 
0x2dfa1b73, 0x816e, 0x46de, 0x82, 0x26, 0x38, 0x2d, 0xb2, 0x30, 0xbf, 0xfe);


class CMedian2D : public CTransformFilter	{

	// allow access to pointers to pins to following classes:
	friend class CIPPAllocator_8u_C1;
	friend class CPinInput;
	friend class CPinOutput;

	public:
		CMedian2D( TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr ) :
		  CTransformFilter( tszName, punk, CLSID_Median2D ){

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



			  // this should be set from property pages
			  m_MovingMedianOrder = 3;
		}

		~CMedian2D()	{
			delete outputMediaType;
		}

		HRESULT CheckInputType( const CMediaType *mtIn );
		HRESULT GetMediaType( int iPosition, CMediaType *pMediaType );
		HRESULT CheckTransform( const CMediaType *mtIn, const CMediaType *mtOut );
		HRESULT DecideBufferSize( IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *ppropInputRequest );

		static CUnknown* WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

		CMediaType* outputMediaType;
		
	protected:
		// custom output pin with custom allocator gets created here
		CBasePin* GetPin( int n );

		// ALLOCATOR_PROPERTIES
		ALLOCATOR_PROPERTIES inputAllocProps;
		ALLOCATOR_PROPERTIES outputAllocProps;

		// transformation stuff
		CMedianFilter*	pMovingMedians;
		int m_MovingMedianOrder;

	private:
		CCritSec lock;
		int stride_8u_C1, stride_8u_C3;
		int bytesperpixel_8u_C1;
		int step_8u_C1;
		static const int IPPalignment = 32;

		HRESULT Transform( IMediaSample *pSource, IMediaSample *pDest );

};

#endif