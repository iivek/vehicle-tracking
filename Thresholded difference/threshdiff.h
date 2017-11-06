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


#define FILTER_NAME L"Segmentation by subtraction"

// {0A8607FB-050B-4c7d-A160-58693516D5DB}
DEFINE_GUID(CLSID_THRESHDIFF, 
0xa8607fb, 0x50b, 0x4c7d, 0xa1, 0x60, 0x58, 0x69, 0x35, 0x16, 0xd5, 0xdb);

// {C7654E95-DF70-47d9-BE15-42206C0F6DAC}
DEFINE_GUID(MEDIASUBTYPE_8BitMonochrome_internal, 
0xc7654e95, 0xdf70, 0x47d9, 0xbe, 0x15, 0x42, 0x20, 0x6c, 0xf, 0x6d, 0xac);

// {2DFA1B73-816E-46de-8226-382DB230BFFE}
DEFINE_GUID(CLSID_MotionTrackingCategory, 
0x2dfa1b73, 0x816e, 0x46de, 0x82, 0x26, 0x38, 0x2d, 0xb2, 0x30, 0xbf, 0xfe);




// filter used for segmentation of moving objects by background subtraction
class CThreshDiff : public CBaseMultiIOFilter	{

	friend class CIPPAllocator_8u_C1;
	friend class CIPPAllocatorOut_8u_C1;
	friend class CPinOutput;
	friend class CPinInputBackground;
	friend class CPinInputScene;
	
public:

	CMediaType* outputMediaType;

	CThreshDiff( TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr );
	~CThreshDiff(void);

	// we provide custom pins
	HRESULT InitializePins();
	int GetPinCount();
	CBasePin* GetPin(int n);

	HRESULT DecideBufferSize(
		IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pRequestProperties, CBaseMultiIOOutputPin* caller);
	HRESULT GetMediaType(int iPosition, CMediaType* pmt, CBaseMultiIOOutputPin* caller);
	HRESULT CheckInputType( const CMediaType *mtIn );

	 // Standard setup for output sample
    HRESULT InitializeOutputSample(IMediaSample *pSample, IMediaSample **ppOutSample);

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
			int threshold;
			
		} Parameters;
	Parameters m_params;

	// helper class that collects input samples, and does sth with them when all become available
	// pretty much ad-hoc, primitive synchronization. we will use the main filter class's section
	// lock rather than defining some events
	class CSampleCollector	{
	public:
		CSampleCollector(CThreshDiff* pFilt, HRESULT ( CThreshDiff::*fp )( IMediaSample** pSamples ));
		~CSampleCollector();
	
		HRESULT AddSample(IMediaSample* sample, CPinInputBackground*);
		HRESULT AddSample(IMediaSample* sample, CPinInputScene*);
		bool AllAvailable(void);
		void Reset(void);

		// by convention, samples[0] corresponds to background, samples[1] to scene sample
		IMediaSample* samples[2];
		boolean available[2];
		
		// pointer to a function in the main filter class that processes the samples
		HRESULT ( CThreshDiff::*fpProcess )( IMediaSample** pSamples );

	protected:
		CThreshDiff* pFilt;
		CCritSec* m_csReceive;

	};

	// Two event handles, one for each pin. We want to change filter state
	// only when both samples from the input pins will have arrived (we want
	// the samples to arrive in pairs).
	// When we are inactive, our downstream filters are also inactive, but upstream
	// filters don't have to be inactive yet. With those handles we want to control
	// receival of samples
	HANDLE	hBackgroundAvailable;	
	// to signal if it's safe to stop/pause. again, we want to stop/pause after
	// we received as many samples from the bkgnd pin as from the scene pin
	HANDLE	hHasDelivered[2];		//[0]... background;	[1]...scene
	HANDLE	hSafeToDecommit;

protected:
	// ALLOCATOR_PROPERTIES
	ALLOCATOR_PROPERTIES inputAllocProps;
	ALLOCATOR_PROPERTIES outputAllocProps;

	// pins
	CPinOutput* outputPin;
	CPinInputBackground* backgroundPin;
	CPinInputScene* scenePin;

	CSampleCollector* pSampleCollector;

	// additional allocs
	Ipp8u* pAbsDiff_8u_C1;

	// flags used in creating output frames for our output pin
	BOOL m_bEOSDelivered;              // have we sent EndOfStream
    BOOL m_bSampleSkipped;             // Did we just skip a frame
    BOOL m_bQualityChanged;            // Have we degraded?

private:
	
	// method for processing the input samples
	HRESULT Process( IMediaSample** pSamples );

	IppiSize frameSize;
	int stride_8u_C1, stride_8u_C3, stride_32f_C1;
	int bitsperpixel_8u_C1;
	int bitsperpixel_8u_C3;
	int step_8u_C1, step_8u_C3, step_32f_C1;
	static const int IPPalignment = 32;

};

#endif