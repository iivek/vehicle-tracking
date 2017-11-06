#ifndef FAME2D_H
#define FAME2D_H

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

#ifndef PINS_H
	#include "pins.h"
#endif

#ifndef BASEMULTIIOFILTER_H
	#include "../BaseMultiIOFilter/BaseMultiIOFilter.h"
#endif

#ifndef CV_H
	#include <cv.h>			// OpenCV
	#define CV_H
#endif

#ifndef CXCORE_H
	#include <cxcore.h>		// OpenCV
	#define CXCORE_H
#endif

#ifndef QUEUE
	#include <queue>
	#define QUEUE
#endif


#define FILTER_NAME L"Overlay"

// {819D5ADA-BF45-4095-B795-87F44E5E7394}
DEFINE_GUID(CLSID_Overlay, 
0x819d5ada, 0xbf45, 0x4095, 0xb7, 0x95, 0x87, 0xf4, 0x4e, 0x5e, 0x73, 0x94);

// {2DFA1B73-816E-46de-8226-382DB230BFFE}
DEFINE_GUID(CLSID_MotionTrackingCategory, 
0x2dfa1b73, 0x816e, 0x46de, 0x82, 0x26, 0x38, 0x2d, 0xb2, 0x30, 0xbf, 0xfe);



// filter used for segmentation of moving objects by background subtraction
class COverlay : public CBaseMultiIOFilter	{

	friend class CIPPAllocator_8u_C3;
	friend class CIPPAllocatorOut_8u_C3;
	friend class CPinOutput;
	friend class CPinInputMain;
	friend class CPinInputOverlay;
	
public:

	CMediaType* outputMediaType;

	COverlay( TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr );
	~COverlay(void);

	// we provide custom pins
	HRESULT InitializePins();
	int GetPinCount();
	CBasePin* GetPin(int n);

	HRESULT DecideBufferSize(
		IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pRequestProperties, CBaseMultiIOOutputPin* caller);
	HRESULT GetMediaType(int iPosition, CMediaType* pmt, CBaseMultiIOOutputPin* caller);
	HRESULT CheckOutputType( const CMediaType* pMediaType );
	//HRESULT CheckInputType( const CMediaType *mtIn );	// each pin takes care of itself	

	 // Standard setup for output sample
    HRESULT InitializeOutputSample(IMediaSample *pSample, IMediaSample **ppOutSample);

	static CUnknown* WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

	STDMETHODIMP EndOfStream(CBaseMultiIOInputPin* caller);
	STDMETHODIMP BeginFlush(CBaseMultiIOInputPin* caller);
	STDMETHODIMP EndFlush(CBaseMultiIOInputPin* caller);
	STDMETHODIMP NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate, CBaseMultiIOInputPin* caller);
/*
	STDMETHODIMP Stop();
	STDMETHODIMP Pause();
	STDMETHODIMP Run(REFERENCE_TIME tStart);
*/	
	// initialize stuff such as strides and sample dimensions in pixels, everythingž
	// what our allocators or sample processing methods need
	HRESULT InitializeProcessParams(VIDEOINFOHEADER* pVih);

	typedef struct {
			//int threshold;
			
		} Parameters;
	Parameters m_params;

	// methods that collect samples and help to process them only when we have two of them 
	// with the same timestamps
	//
	HRESULT CollectSample(IMediaSample* sample, CPinInputMain*);
	HRESULT CollectSample(IMediaSample* sample, CPinInputOverlay*);
	void EmptyMainQueue();
	void EmptyOverlayQueue();

	queue<IMediaSample*> qpMain;		// main samples queue
	queue<IMediaSample*> qpOverlay;		// overlay samples queue
	
protected:
	// ALLOCATOR_PROPERTIES
	ALLOCATOR_PROPERTIES inputAllocProps;
	ALLOCATOR_PROPERTIES outputAllocProps;

	// pins
	CPinOutput* outputPin;
	CPinInputMain* inputMainPin;
	CPinInputOverlay* inputOverlayPin;

	CvScalar colorList[3];

	// flags used in creating output frames for our output pin
	BOOL m_bEOSDelivered;              // have we sent EndOfStream
    BOOL m_bSampleSkipped;             // Did we just skip a frame
    BOOL m_bQualityChanged;            // Have we degraded?

private:
	
	// method for processing the input samples
	HRESULT Process( IMediaSample** pSamples );

	IppiSize frameSize;
	int stride_8u_C1, stride_8u_C3, stride_8u_AC4;
	int bytesPerPixel_8u_C1, bytesPerPixel_8u_C3, bytesPerPixel_8u_AC4;
	int step_8u_C1, step_8u_C3, step_8u_AC4;
	static const int DIBalignment = 4;

	bool m_bOKToDeliverEndOfStream;

	CvSize	frameSizeCV;
	// IPL datatype used by OpenCV
	IplImage*	pOutputCV_C3;

};

#endif