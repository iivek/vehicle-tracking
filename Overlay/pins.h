#ifndef PINS_H
#define PINS_H

#ifndef ALLOCATORS_H
	#include "allocators.h"
#endif

#ifndef OVERLAY_H
	#include "overlay.h"
#endif

// forward declarations
class CIPPAllocatorOut_8u_C3;
class COverlay;


// one output pin
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
	HRESULT CheckMediaType( const CMediaType* pMediaType );

	HRESULT Active();
	HRESULT Inactive();

	CIPPAllocatorOut_8u_C3* m_pIPPAllocator;	// our pin allocator object

			// seeking stuff
	// implement IMediaPosition by passing upstream
    IUnknown * m_pPosition;
	// overridden to expose seeking related interfaces
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);
	
private:
	HRESULT InitializeAllocatorRequirements();

	// we override this method to instance our IPP allocator
	HRESULT InitAllocator(IMemAllocator **ppAlloc);

	COverlay* m_pFilter;
};


class CPinInputMain : public CBaseMultiIOInputPin	{

	friend class CPinOutput;	// so it can access quality sink the easy way

public:
	CPinInputMain(CBaseMultiIOFilter* pFilter,
		CCritSec* pLock,
		HRESULT* phr,
		LPCWSTR pName) :
			CBaseMultiIOInputPin(pFilter, pLock, phr, pName, 0),
			m_pFilter((COverlay*) pFilter){	}
	~CPinInputMain()	{  }

	HRESULT CheckMediaType(const CMediaType *pmt);

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

	COverlay* m_pFilter;

};

// overlay input pin
class CPinInputOverlay : public CBaseMultiIOInputPin	{

	friend class CPinOutput;	// so it can access quality sink the easy way

public:

	HRESULT Inactive(void);

	CPinInputOverlay(CBaseMultiIOFilter* pFilter,
		CCritSec* pLock,
		HRESULT* phr,
		LPCWSTR pName) :
			CBaseMultiIOInputPin(pFilter, pLock, phr, pName, 1),
			m_pFilter((COverlay*) pFilter){	}

	~CPinInputOverlay()	{}

	HRESULT InitializeAllocatorRequirements();
	// overriden to enable the filter to get the upsrtm pin's allocator properties
    STDMETHODIMP NotifyAllocator(IMemAllocator * pAllocator, BOOL bReadOnly);	
	HRESULT CheckMediaType(const CMediaType *pmt);

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
	STDMETHODIMP ReceiveCanBlock()	{return S_OK;}
	
	// we could make an effort to propose our allocator
	//STDMETHODIMP GetAllocator (IMemAllocator** ppAllocator);
	STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps);
	
	COverlay* m_pFilter;
};

#endif