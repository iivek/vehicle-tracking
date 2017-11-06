#pragma once

#include <Streams.h>    // DirectShow (includes windows.h)
#include <initguid.h>   // declares DEFINE_GUID to declare an EXTERN_C const.
#include <ipp.h>

#define FILTER_NAME L"Extract Y"


// {6B55428F-3F95-4671-A776-DC0D31CC966B}
DEFINE_GUID(CLSID_ExtractY, 
0x6b55428f, 0x3f95, 0x4671, 0xa7, 0x76, 0xdc, 0xd, 0x31, 0xcc, 0x96, 0x6b);

// {C7654E95-DF70-47d9-BE15-42206C0F6DAC}
DEFINE_GUID(MEDIASUBTYPE_8BitMonochrome_internal, 
0xc7654e95, 0xdf70, 0x47d9, 0xbe, 0x15, 0x42, 0x20, 0x6c, 0xf, 0x6d, 0xac);

// {2DFA1B73-816E-46de-8226-382DB230BFFE}
DEFINE_GUID(CLSID_MotionTrackingCategory, 
0x2dfa1b73, 0x816e, 0x46de, 0x82, 0x26, 0x38, 0x2d, 0xb2, 0x30, 0xbf, 0xfe);


class CExtractY : public CTransformFilter	{

	// allow access to pointers to pins to following classes:
	friend class CIPPAllocator_8u_C1;
	friend class CIPPAllocator_8u_AC4;
	friend class CPinInput;
	friend class CPinOutput;

	public:
		CExtractY( TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr ) :
		  CTransformFilter( tszName, punk, CLSID_ExtractY ){

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

		}

		~CExtractY()	{
			delete outputMediaType;
		}

		HRESULT CheckInputType( const CMediaType *mtIn );
		HRESULT GetMediaType( int iPosition, CMediaType *pMediaType );
		HRESULT CheckTransform( const CMediaType *mtIn, const CMediaType *mtOut );
		HRESULT DecideBufferSize( IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *ppropInputRequest );
		HRESULT InitializeOutputSample(IMediaSample *pSample, IMediaSample **ppOutSample);

		static CUnknown* WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

		CMediaType* outputMediaType;
		
	protected:
		// custom output pin with custom allocator gets created here
		CBasePin* GetPin( int n );

		ALLOCATOR_PROPERTIES outputAllocProps;
		ALLOCATOR_PROPERTIES inputAllocProps;

	private:
		CCritSec lock;
		int stride_8u_C1, stride_8u_AC4;
		int bytesPerPixel_8u_C1, bytesPerPixel_8u_AC4;
		int step_8u_C1, step_8u_AC4;
		static const int DIBalignment = 4;
		static const int IPPalignment = 32;
		IppiSize frameSize;

		HRESULT Transform( IMediaSample *pSource, IMediaSample *pDest );
		//HRESULT SetOutputIMediaSample(IMediaSample *pSource, IMediaSample *pDest);

};