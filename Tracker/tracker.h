#ifndef TRACKER_H
#define TRACKER_H

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

#ifndef KALMANTRACKING_H
	#include "kalmanTracking.h"
#endif

#ifndef SMART_PTR_HPP
	#include "boost\smart_ptr.hpp"
	#define SMART_PTR_HPP
#endif

#define FILTER_NAME L"Tracker"


// {0376084E-2A49-4103-A58F-D56B75BF9990}
DEFINE_GUID(CLSID_TRACKER, 
0x376084e, 0x2a49, 0x4103, 0xa5, 0x8f, 0xd5, 0x6b, 0x75, 0xbf, 0x99, 0x90);

// {C7654E95-DF70-47d9-BE15-42206C0F6DAC}
DEFINE_GUID(MEDIASUBTYPE_8BitMonochrome_internal, 
0xc7654e95, 0xdf70, 0x47d9, 0xbe, 0x15, 0x42, 0x20, 0x6c, 0xf, 0x6d, 0xac);

// {2DFA1B73-816E-46de-8226-382DB230BFFE}
DEFINE_GUID(CLSID_MotionTrackingCategory, 
0x2dfa1b73, 0x816e, 0x46de, 0x82, 0x26, 0x38, 0x2d, 0xb2, 0x30, 0xbf, 0xfe);


class CTrackerFilter : public CTransformFilter	{

	// allow access to pointers to pins to following classes:
	friend class CIPPAllocator_8u_C1;
	friend class CListAllocatorOut;
	friend class CPinInput;
	friend class CPinOutput;

	public:
		CTrackerFilter( TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr ) :
			CTransformFilter( tszName, punk, CLSID_TRACKER ),
				// additional allocs
			measurement(NULL),
			pInputCV_C1(NULL),
			pHelperCV_C1(NULL),
			contourStorage(NULL),
			pHelper_8u_C1(NULL)
{

			// the only output media subtype is MEDIASUBTYPE_CMovingObjects...
			// Our output MediaType is defined as follows:
			  //outputMediaType = new CMediaType(&MEDIASUBTYPE_CMovingObjects);
			  outputMediaType = new CMediaType();
			  outputMediaType->InitMediaType();
			  outputMediaType->AllocFormatBuffer(sizeof(FORMAT_VideoInfo));

              outputMediaType->SetType(&MEDIATYPE_Video);
			  outputMediaType->SetSubtype(&MEDIASUBTYPE_CMovingObjects);
			  outputMediaType->SetFormatType(&FORMAT_VideoInfo);
			  outputMediaType->bFixedSizeSamples = FALSE;
			  outputMediaType->SetTemporalCompression(FALSE);
			  outputMediaType->cbFormat = sizeof(VIDEOINFOHEADER);
		}

		~CTrackerFilter()	{
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

		float measurementD[2];	// all moving objects will share this temp. array
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
		// custom output pin with custom allocator gets created here
		CBasePin* GetPin( int n );

		// ALLOCATOR_PROPERTIES
		ALLOCATOR_PROPERTIES inputAllocProps;
		ALLOCATOR_PROPERTIES outputAllocProps;

	private:
		IppiSize frameSize;
		CvSize	frameSizeCV;
		int stride_8u_C1;
		int bytesPerPixel_8u_C1;
		int step_8u_C1;
		static const int DIBalignment = 4;
		// unique ID counter
		unsigned long uniqueID;	

		// We are using OpenCV that is compatibile with old Intel IPL. Although IPL is used no more,
		// we will be using IPL data types. 
		IplImage*	pInputCV_C1;
		// Some openCV functions (e.g. related to connected components) require preallocated memory of unknown size.
		CvMemStorage*	contourStorage;
		CvSeq* cont;	//growable sequence of elements

		IplImage*	pHelperCV_C1;
		BYTE* pHelper_8u_C1;

		HRESULT Transform( IMediaSample *pSource, IMediaSample *pDest );
		// Transform-related methods
		CvSeq* FindMovingObjects(IplImage*);
		void TrackObjects();

};

#endif