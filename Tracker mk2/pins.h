#ifndef PINS_H
#define PINS_H

#ifndef ALLOCATORS_H
	#include "allocators.h"
#endif

#ifndef TRACKER_MKII_H
	#include "trackermkII.h"
#endif

#ifndef BASEMULTIIOFILTER_H
	#include "../BaseMultiIOFilter/BaseMultiIOFilter.h"
#endif

#ifndef STREAMS_H
	#include <Streams.h>    // DirectShow (includes windows.h)
	#define STREAMS_H
#endif

// forward declarations
class CListAllocatorOut;
class CIPPAllocatorOut_8u_C3;
class CTrackerMkII;

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

//test	CListAllocatorOut* m_pListAllocator;	// our pin allocator object
	CIPPAllocatorOut_8u_C3* m_pListAllocator;	// our pin allocator object

			// seeking stuff
	// implement IMediaPosition by passing upstream
    IUnknown * m_pPosition;
	// overridden to expose seeking related interfaces
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);
	
private:
	HRESULT InitializeAllocatorRequirements();

	// we override this method to instance our IPP allocator
	HRESULT InitAllocator(IMemAllocator **ppAlloc);

	CTrackerMkII* m_pFilter;
};


class CPinInputLuma : public CBaseMultiIOInputPin	{

	friend class CPinOutput;	// so it can access quality sink the easy way

public:
	CPinInputLuma(CBaseMultiIOFilter* pFilter,
		CCritSec* pLock,
		HRESULT* phr,
		LPCWSTR pName) :
			CBaseMultiIOInputPin(pFilter, pLock, phr, pName, 0),
			m_pFilter((CTrackerMkII*) pFilter){	}
	~CPinInputLuma()	{  }

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

	CTrackerMkII* m_pFilter;

};

// color input pin
class CPinInputColor : public CBaseMultiIOInputPin	{

	friend class CPinOutput;	// so it can access quality sink the easy way

public:

	HRESULT Inactive(void);

	CPinInputColor(CBaseMultiIOFilter* pFilter,
		CCritSec* pLock,
		HRESULT* phr,
		LPCWSTR pName) :
			CBaseMultiIOInputPin(pFilter, pLock, phr, pName, 1),
			m_pFilter((CTrackerMkII*) pFilter){	}

	~CPinInputColor()	{}

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
	
	CTrackerMkII* m_pFilter;
};

#endif