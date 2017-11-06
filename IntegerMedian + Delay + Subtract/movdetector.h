#ifndef FAME2D_H
#define FAME2D_H

#ifndef IPPIMOVDETECTOR_H
	#include "ippimedian.h"
#endif

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

#ifndef BASEMULTIIOFILTER_H
	#include "../BaseMultiIOFilter/BaseMultiIOFilter.h"
#endif

#define FILTER_NAME L"Mov. Detector"


// {AE6A03EF-D769-4c0d-9D6C-9375685CBD5A}
DEFINE_GUID(CLSID_MOVDETECTOR, 
0xae6a03ef, 0xd769, 0x4c0d, 0x9d, 0x6c, 0x93, 0x75, 0x68, 0x5c, 0xbd, 0x5a);

// {C7654E95-DF70-47d9-BE15-42206C0F6DAC}
DEFINE_GUID(MEDIASUBTYPE_8BitMonochrome_internal, 
0xc7654e95, 0xdf70, 0x47d9, 0xbe, 0x15, 0x42, 0x20, 0x6c, 0xf, 0x6d, 0xac);

// {2DFA1B73-816E-46de-8226-382DB230BFFE}
DEFINE_GUID(CLSID_MotionTrackingCategory, 
0x2dfa1b73, 0x816e, 0x46de, 0x82, 0x26, 0x38, 0x2d, 0xb2, 0x30, 0xbf, 0xfe);

#ifndef IPP_H
	#include <ipp.h>
	#define IPP_H
#endif

#ifndef MATH_H
	#include <math.h>
	#define MATH_H
#endif




// filter used for segmentation of moving objects by background subtraction
class CMovDetectorFilter : public CBaseMultiIOFilter	{

	friend class CIPPAllocator_8u_C1;
	friend class CIPPAllocatorMovement_8u_C1;
	friend class CIPPAllocatorBackground_8u_C1;
	friend class CPinOutputBackground;
	friend class CPinOutputMovement;
	friend class CPinInput;
	
public:

	CMediaType* outputMediaType;

	CMovDetectorFilter( TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr );
	~CMovDetectorFilter(void);

	// we provide custom pins
	HRESULT InitializePins();
	int GetPinCount();
	CBasePin* GetPin(int n);

	HRESULT DecideBufferSize(
		IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pRequestProperties, CBaseMultiIOOutputPin* caller);
	HRESULT GetMediaType(int iPosition, CMediaType* pmt, CBaseMultiIOOutputPin* caller);
	HRESULT CheckInputType( const CMediaType *mtIn );

	// Standard setup for output sample
    HRESULT InitializeOutputSampleBackground(	IMediaSample *pSample, IMediaSample **ppOutSample );
	HRESULT InitializeOutputSampleMovement(	IMediaSample *pSample, IMediaSample **ppOutSample );
	void SetSample( IMediaSample* pSample, IMediaSample *pOutSample, AM_SAMPLE2_PROPERTIES* const &pProps	);

	static CUnknown* WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

	STDMETHODIMP EndOfStream(CBaseMultiIOInputPin* caller);
	STDMETHODIMP BeginFlush(CBaseMultiIOInputPin* caller);
	STDMETHODIMP EndFlush(CBaseMultiIOInputPin* caller);
	STDMETHODIMP NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate, CBaseMultiIOInputPin* caller);

	STDMETHODIMP Stop();
	STDMETHODIMP Pause();
	STDMETHODIMP Run(REFERENCE_TIME tStart);
	
	// initialize stuff such as strides and sample dimensions in pixels, everythingž
	// what our allocators or sample processing methods need
	HRESULT InitializeProcessParams(VIDEOINFOHEADER* pVih);

	typedef struct {
			int windowSize;
			unsigned int threshold;
		} Parameters;
	Parameters m_params;

protected:
	// ALLOCATOR_PROPERTIES
	ALLOCATOR_PROPERTIES inputAllocProps;
	ALLOCATOR_PROPERTIES outputAllocProps;
	boolean additionalAllocated;	// if one output allocator hasn't allocated the CIppiMedian_8u_C1 and similar
									// stuff, the other has to do it. and we want exactly one instance of
									// CIppiMedian_8u_C1

	// pins
	CPinInput* pInputPin;
	CPinOutputBackground* pBackgroundPin;
	CPinOutputMovement* pMovementPin;


	// flags used in creating output frames for our output pin
	BOOL m_bEOSDelivered;              // have we sent EndOfStream
    BOOL m_bSampleSkipped;             // Did we just skip a frame
    BOOL m_bQualityChanged;            // Have we degraded?

	IppiSize frameSize;
	int stride_8u_C1;
	int bytesPerPixel_8u_C1;
	int step_8u_C1;
	static const int DIBalignment = 4;


private:
	
	// where the smart stuff is done
/*	void Process(IMediaSample* pInputSample,
					IMediaSample* pOutMovementSample,
					IMediaSample* pOutBackgroundSample);
*/
	CIppiMedian_8u_C1* pMedian;
};

#endif