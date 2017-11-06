#ifndef PINS_H
#define PINS_H

#ifndef ALLOCATOR_H
	#include "allocator.h"
	#define ALLOCATOR_H
#endif

#ifndef STREAMS_H
	#include <Streams.h>    // DirectShow (includes windows.h)
	#define STREAMS_H
#endif

#include "../BaseMultiIOFilter/BaseMultiIOFilter.h"


// forward declarations
class CIPPAllocatorBackground_8u_C1;
class CIPPAllocatorMovement_8u_C1;
class CMovDetectorFilter;

class CPinOutputBackground : public CBaseMultiIOOutputPin	{

	friend class CBaseMultiIOInputPin;

public:
	CPinOutputBackground(CBaseMultiIOFilter* pFilter,
		CCritSec* pLock,
		HRESULT* phr,
		LPCWSTR pName);
	~CPinOutputBackground();

	HRESULT DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc);
	STDMETHODIMP Notify(IBaseFilter * pSender, Quality q);
	// so we can commit/decommit our allocator
	HRESULT Active();
	HRESULT Inactive();	

	CIPPAllocatorBackground_8u_C1* m_pIPPAllocator;	// our pin allocator object
	
private:
	HRESULT InitializeAllocatorRequirements();

	// we override this method to instance our IPP allocator
	HRESULT InitAllocator(IMemAllocator **ppAlloc);

	CMovDetectorFilter* m_pFilter;
};

// seeking functionality will be available only if this pin is connected
class CPinOutputMovement : public CBaseMultiIOOutputPin	{

	friend class CBaseMultiIOInputPin;

public:
	CPinOutputMovement(CBaseMultiIOFilter* pFilter,
		CCritSec* pLock,
		HRESULT* phr,
		LPCWSTR pName);

	~CPinOutputMovement();

	HRESULT DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc);
	STDMETHODIMP Notify(IBaseFilter * pSender, Quality q);
	// so we can commit/decommit our allocator
	HRESULT Active();
	HRESULT Inactive();

	CIPPAllocatorMovement_8u_C1* m_pIPPAllocator;	// our pin allocator object

		// seeking stuff
	// implement IMediaPosition by passing upstream
    IUnknown * m_pPosition;
	// overridden to expose seeking related interfaces
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);
	
private:
	HRESULT InitializeAllocatorRequirements();

	// we override this method to instance our IPP allocator
	HRESULT InitAllocator(IMemAllocator **ppAlloc);

	CMovDetectorFilter* m_pFilter;
};


// second pin that receives the dynamic scene. It will be responsible for delivering
// notifications to the output pin
class CPinInput : public CBaseMultiIOInputPin	{

	friend class CPinOutputMovement;	// so it can access quality sink the easy way

public:

	HRESULT Inactive(void);

	CPinInput(CBaseMultiIOFilter* pFilter,
		CCritSec* pLock,
		HRESULT* phr,
		LPCWSTR pName) :
			CBaseMultiIOInputPin(pFilter, pLock, phr, pName, 1),
			m_pFilter((CMovDetectorFilter*) pFilter){	}

	~CPinInput()	{}

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
	CMovDetectorFilter* m_pFilter;
};

#endif