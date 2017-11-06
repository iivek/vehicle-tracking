#ifndef DELAY_H
	#define DELAY_H

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

#include "ippiBuffer_8u_AC4.h"
// propsstuff
// #include "IDelayProperties.h"



#define FILTER_NAME L"Delay"


// {89C329C3-2947-4450-B550-26D0D674D9A1}
DEFINE_GUID(CLSID_Delay, 
0x89c329c3, 0x2947, 0x4450, 0xb5, 0x50, 0x26, 0xd0, 0xd6, 0x74, 0xd9, 0xa1);

// {C7654E95-DF70-47d9-BE15-42206C0F6DAC}
DEFINE_GUID(MEDIASUBTYPE_8BitMonochrome_internal, 
0xc7654e95, 0xdf70, 0x47d9, 0xbe, 0x15, 0x42, 0x20, 0x6c, 0xf, 0x6d, 0xac);

// {2DFA1B73-816E-46de-8226-382DB230BFFE}
DEFINE_GUID(CLSID_MotionTrackingCategory, 
0x2dfa1b73, 0x816e, 0x46de, 0x82, 0x26, 0x38, 0x2d, 0xb2, 0x30, 0xbf, 0xfe);


class CDelayFilter :	public CTransformFilter//,
//						public IDelayProperties,
//						public ISpecifyPropertyPages
{

	// allow access to pointers to pins to following classes:
	friend class CIPPAllocator_8u_C1;
	friend class CIPPAllocator_8u_AC4;
	friend class CPinInput;
	friend class CPinOutput;

	public:
		CDelayFilter( TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr ) :
		  CTransformFilter( tszName, punk, CLSID_Delay ),
		  pDelayBuffer(NULL)
		  {

			// the only output media subtype is MEDIASUBTYPE_8BitMonochrome_internal...
			// Our output MediaType is defined as follows:
			  outputMediaType = new CMediaType();
			  outputMediaType->InitMediaType();
			  outputMediaType->AllocFormatBuffer(sizeof(FORMAT_VideoInfo));

              outputMediaType->SetType(&MEDIATYPE_Video);
			  outputMediaType->SetSubtype(&MEDIASUBTYPE_RGB32);
			  outputMediaType->SetFormatType(&FORMAT_VideoInfo);
			  outputMediaType->bFixedSizeSamples = TRUE;
			  outputMediaType->SetTemporalCompression(FALSE);
			  outputMediaType->cbFormat = sizeof(VIDEOINFOHEADER);

			  m_params.delay = 100;

		}

		~CDelayFilter()	{
			delete outputMediaType;
		}

		typedef struct {
			int delay;
		} Parameters;
		Parameters m_params;

		HRESULT CheckInputType( const CMediaType *mtIn );
		HRESULT GetMediaType( int iPosition, CMediaType *pMediaType );
		HRESULT CheckTransform( const CMediaType *mtIn, const CMediaType *mtOut );
		HRESULT DecideBufferSize( IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *ppropInputRequest );
		HRESULT InitializeOutputSample(IMediaSample *pSample, IMediaSample **ppOutSample);

		// _______IDelayProperties implementation
//		STDMETHODIMP GetDelay( int* pDelay );
//		STDMETHODIMP SetDelay( int pDelay );

		// _______ISpecifyPropertyPages implementation
//		STDMETHODIMP GetPages( CAUUID *pPages );

		// Overriding CUnknown::NonDelegatingQueryInterface, to check IIDs of both
		// ISpecifyPropertyPages and IProperties
//		STDMETHODIMP NonDelegatingQueryInterface( REFIID riid, void** ppv );

//		DECLARE_IUNKNOWN;	// IUnknown Methods Macro: declare QueryInterface, AddRef and Release
		static CUnknown* WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

		CMediaType* outputMediaType;
		
	protected:
		// custom output pin with custom allocator gets created here
		CBasePin* GetPin( int n );

		ALLOCATOR_PROPERTIES outputAllocProps;
		ALLOCATOR_PROPERTIES inputAllocProps;

	private:
		CCritSec lock;
		int stride_8u_AC4;
		int bytesPerPixel_8u_AC4;
		int step_8u_AC4;
		static const int DIBalignment = 4;
		static const int IPPalignment = 32;
		IppiSize frameSize;
		CCircularIppiBuffer* pDelayBuffer;

		HRESULT Transform( IMediaSample *pSource, IMediaSample *pDest );
		//HRESULT SetOutputIMediaSample(IMediaSample *pSource, IMediaSample *pDest);

};

#endif