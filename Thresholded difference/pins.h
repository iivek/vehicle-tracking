#ifndef PINS_H
#define PINS_H

#ifndef ALLOCATORS_H
	#include "allocators.h"
#endif

#ifndef THRESHDIFF_H
	#include "threshdiff.h"
#endif

// forward declarations
class CIPPAllocatorOut_8u_C1;
class CThreshDiff;


// a pin that outputs the thresholded absolute difference between the two input samples
class CPinOutput : public CBaseMultiIOOutputPin	{

	friend class CBaseMultiIOInputPin;

public:
	CPinOutput(CBaseMultiIOFilter* pFilter,
		CCritSec* pLock,
		HRESULT* phr,
		LPCWSTR pName);

	~CPinOutput();

	HRESULT DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc);
	STDMETHODIMP Notify(IBaseFilter * pSender, Quality q);
	// so we can commit/decommit our allocator
	HRESULT Active();
	HRESULT Inactive();
	HRESULT BreakConnect();

	CIPPAllocatorOut_8u_C1* m_pIPPAllocator;	// our pin allocator object
	
private:
	HRESULT InitializeAllocatorRequirements();

	// we override this method to instance our IPP allocator
	HRESULT InitAllocator(IMemAllocator **ppAlloc);

	CThreshDiff* m_pFilter;
};


// Pin that receives the background estimation. Background estimator is generally slow and may
// output samples asynchronously, so we will modify this pin to store the sample, store it
// and use it repetitively until a new background estimation arrives in order not to make
// the graph wait for the upstream filter... well at least in the future
class CPinInputBackground : public CBaseMultiIOInputPin	{

	friend class CPinOutput;	// so it can access quality sink the easy way

public:
	CPinInputBackground(CBaseMultiIOFilter* pFilter,
		CCritSec* pLock,
		HRESULT* phr,
		LPCWSTR pName) :
			CBaseMultiIOInputPin(pFilter, pLock, phr, pName, 0),
			m_pFilter((CThreshDiff*) pFilter){	}
	~CPinInputBackground()	{  }


		//TESTNO
	HRESULT Inactive(void);


	// IMemInputPin interface	
	virtual STDMETHODIMP Receive(IMediaSample *pMediaSample);
	// we block.
	STDMETHODIMP ReceiveCanBlock()	{return S_OK;};
	
	// IPin interface
	STDMETHODIMP BeginFlush();
    STDMETHODIMP EndFlush();
	STDMETHODIMP EndOfStream();
	STDMETHODIMP NewSegment(
		REFERENCE_TIME tStart,
		REFERENCE_TIME tStop,
		double dRate);

	// we could make an effort to propose our allocator
	//STDMETHODIMP GetAllocator (IMemAllocator** ppAllocator);
	//STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps);

	//CIPPAllocatorIn_8u_C1* m_pIPPAllocator;	// our pin allocator object
	CThreshDiff* m_pFilter;

};

// second pin that receives the dynamic scene. It will be responsible for delivering
// notifications to the output pin
class CPinInputScene : public CBaseMultiIOInputPin	{

	friend class CPinOutput;	// so it can access quality sink the easy way

public:


			//TESTNO
	HRESULT Inactive(void);


	CPinInputScene(CBaseMultiIOFilter* pFilter,
		CCritSec* pLock,
		HRESULT* phr,
		LPCWSTR pName) :
			CBaseMultiIOInputPin(pFilter, pLock, phr, pName, 1),
			m_pFilter((CThreshDiff*) pFilter){	}

	~CPinInputScene()	{}

	// IMemInputPin interface	
	virtual STDMETHODIMP Receive(IMediaSample *pMediaSample);
	// IPin interface
	STDMETHODIMP BeginFlush();
    STDMETHODIMP EndFlush();
	STDMETHODIMP EndOfStream();
	STDMETHODIMP NewSegment(
		REFERENCE_TIME tStart,
		REFERENCE_TIME tStop,
		double dRate);
	// we block.
	STDMETHODIMP ReceiveCanBlock()	{return S_OK;};
	
	// we could make an effort to propose our allocator
	//STDMETHODIMP GetAllocator (IMemAllocator** ppAllocator);
	//STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps);
	
	//CIPPAllocator_8u_C1 *m_pIPPAllocator;	// our pin allocator object
	CThreshDiff* m_pFilter;
};

#endif