#ifndef TRACKER_MKII_H
#define TRACKER_MKII_H

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

#ifndef KALMANTRACKING_H
	#include "kalmanTracking.h"
#endif


#define FILTER_NAME L"Tracker mkII"

// {7FF6392E-B0EB-4278-B448-47813F9EC7C5}
DEFINE_GUID(CLSID_Tracker_mkII, 
0x7ff6392e, 0xb0eb, 0x4278, 0xb4, 0x48, 0x47, 0x81, 0x3f, 0x9e, 0xc7, 0xc5);

// {C7654E95-DF70-47d9-BE15-42206C0F6DAC}
DEFINE_GUID(MEDIASUBTYPE_8BitMonochrome_internal, 
0xc7654e95, 0xdf70, 0x47d9, 0xbe, 0x15, 0x42, 0x20, 0x6c, 0xf, 0x6d, 0xac);

// {2DFA1B73-816E-46de-8226-382DB230BFFE}
DEFINE_GUID(CLSID_MotionTrackingCategory, 
0x2dfa1b73, 0x816e, 0x46de, 0x82, 0x26, 0x38, 0x2d, 0xb2, 0x30, 0xbf, 0xfe);


// filter used for segmentation of moving objects by background subtraction
// TODO: if there is nothing on the luma input, work with the color input only, calculating
// the luma when needed
class CTrackerMkII : public CBaseMultiIOFilter	{

	friend class CIPPAllocator_8u_C3;
	friend class CIPPAllocatorOut_8u_C3;
	friend class CListAllocatorOut;
	friend class CPinOutput;
	friend class CPinInputLuma;
	friend class CPinInputColor;
	
public:

	CMediaType* outputMediaType;

	CTrackerMkII( TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr );
	~CTrackerMkII(void);

	// we provide custom pins
	HRESULT InitializePins();
	int GetPinCount();

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
	
	// initialize stuff such as strides and sample dimensions in pixels, everything
	// what our allocators or sample processing methods need
	HRESULT InitializeProcessParams(VIDEOINFOHEADER* pVih);

	// methods that collect samples and help to process them only when we have two of them 
	// with the same timestamps
	//
	HRESULT CollectSample(IMediaSample* sample, CPinInputLuma*);
	HRESULT CollectSample(IMediaSample* sample, CPinInputColor*);
	void EmptyLumaQueue();
	void EmptyColorQueue();
	queue<IMediaSample*> qpLuma;		// luma samples queue
	queue<IMediaSample*> qpColor;		// color samples queue

	typedef struct structParameters{
		int timeToLive;
		KalmanParameters initialKalman;	// initial Kalman filter parameters for each new object
		CvSize minimalSize;				// minimal size of the contour rect for it to be taken into account
		unsigned int bufferSize;		// if the dwnstrm filter proposes a no-sense buffersize for the allocator
										// it is set to this value
										// btw. output buffer size is the max number of CMovingObject objects
										// in the output allocators' buffer.
										
	} Parameters;
	Parameters m_params;

	float measurementD[1];	// all moving objects will share this temp. array
	CvMat* measurement;

	// for object (blob) tracking
	//
	std::list<SPMovingObject> objectsOld;
	std::list<SPMovingObject> objectsNew;			// objects that appear for the first time
	std::list<SPMovingObject> objectsWithFit;		// objects-candidates for inheriting the tracking filter & ID
	std::list<SPMovingObject> objectsSuccessors;	// objects that inherit ID & tracking filter from objectsOld;
													// through them tracking is continued from frame to frame
	std::list<SPMovingObject>::iterator iterNew;
	std::list<SPMovingObject>::iterator iterOld;
	
protected:
	ALLOCATOR_PROPERTIES inputAllocProps;
	ALLOCATOR_PROPERTIES outputAllocProps;

	// pins
	CPinOutput* outputPin;
	CPinInputLuma* inputPinLuma;
	CPinInputColor* inputPinColor;

	// flags used in creating output frames for our output pin
	BOOL m_bEOSDelivered;              // Have we sent EndOfStream
    BOOL m_bSampleSkipped;             // Did we just skip a frame
    BOOL m_bQualityChanged;            // Have we degraded?

	CBasePin* GetPin( int n );

private:
	// method for processing the input samples
	HRESULT Process( IMediaSample** pSamples );

	bool m_bOKToDeliverEndOfStream;

	/* stuff used in object tracking
	*/
	IppiSize frameSize;
	CvSize	frameSizeCV;
	int stride_8u_C1;
	int stride_8u_AC4;
	int stride_8u_C3;
	int step_8u_C1;
	int step_8u_AC4;
	int step_8u_C3;
	int bytesPerPixel_8u_C1;
	int bytesPerPixel_8u_C3;
	int bytesPerPixel_8u_AC4;
	static const int DIBalignment = 4;
	// unique ID counter
	unsigned long uniqueID;	

	// We are using OpenCV that is compatibile with old Intel IPL. Although IPL is used no more,
	// we will be using IPL data types. 
	IplImage*	pLumaCV_C1;
	IplImage*	pColorCV_AC4;
	IplImage*	pHelperCV_C1;
	IplImage*	pContoursCV_C1;
	IplImage*	pMaskCV_C1;
	IplImage*	pTemporaryBufferCV_C1;
	BYTE* pHelper_8u_C1;
	BYTE* pContours_8u_C1;
	BYTE* pMask_8u_C1;
	BYTE* pTemporaryBuffer_8u_C1;

	// Some openCV functions (e.g. related to connected components) require preallocated memory of unknown size.
	CvMemStorage*	contourStorageNew;
	CvMemStorage*	contourStorageOld;
	CvSeq* cont;	//growable sequence of elements

	HRESULT Transform( IMediaSample *pSource, IMediaSample *pDest );
	// Transform-related methods
	CvSeq* FindMovingObjects(IplImage*);
//	void TrackObjects();
};

#endif