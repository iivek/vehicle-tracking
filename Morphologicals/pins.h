#ifndef PINS_H
#define PINS_H

#ifndef ALLOCATORS_H
	#include "allocators.h"
	#define ALLOCATORS_H
#endif

#ifndef STREAMS_H
	#include <Streams.h>    // DirectShow (includes windows.h)
	#define STREAMS_H
#endif

// forward declarations
class CIPPAllocatorOut_8u_C1;
class CIPPAllocatorIn_8u_C1;


// Output queue class modified to have access to m_pIsFlushing
class CMyOutputQueue : public COutputQueue	{

public:
	CMyOutputQueue(
		IPin *pInputPin,
		HRESULT *phr,
		BOOL bAuto = TRUE,
		BOOL bQueue = TRUE,
		LONG lBatchSize = 1,
		BOOL bBatchExact = FALSE,
		LONG lListSize = DEFAULTCACHE,
		DWORD dwPriority = THREAD_PRIORITY_NORMAL) :
    COutputQueue(
		pInputPin,
		phr,
		bAuto,
		bQueue,
		lBatchSize,
		bBatchExact,
		lListSize,
		dwPriority){};

	~CMyOutputQueue(){};

	BOOL IsFlushing()	{return m_bFlushing;}
};

class CPinOutput : public CTransformOutputPin	{

public:
	CPinOutput( CMorphologicalsFilter* pFilter, HRESULT * pHr );
	~CPinOutput();

	HRESULT InitializeAllocatorRequirements();
	HRESULT DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc);

	CMyOutputQueue *m_pOutputQueue;  // Streams data to the peer pin

	// Overridden to create output queue
	HRESULT Active();
	HRESULT Inactive();
	HRESULT Deliver(IMediaSample *pMediaSample);
	HRESULT DeliverEndOfStream();
	HRESULT DeliverBeginFlush();
	HRESULT DeliverEndFlush();
	HRESULT DeliverNewSegment(
		REFERENCE_TIME tStart,
		REFERENCE_TIME tStop,
		double dRate);

private:
	// we override this method to instance our IPP allocator:
	HRESULT InitAllocator(IMemAllocator **ppAlloc);
	
	CIPPAllocatorOut_8u_C1* m_pIPPAllocator;	// our pin allocator object
	CMorphologicalsFilter* m_pFilter;
};


class CPinInput : public CTransformInputPin	{

public:
	CPinInput( CMorphologicalsFilter* pFilter, HRESULT * pHr );
	~CPinInput();
	
	HRESULT InitializeAllocatorRequirements();
	STDMETHODIMP GetAllocator (IMemAllocator** ppAllocator);
	STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps);
	
private:
	CIPPAllocatorIn_8u_C1 *m_pIPPAllocator;	// our pin allocator object
	CMorphologicalsFilter* m_pFilter;
};

// Actually there's no need to make a custom input pin because we insist on format that
// is aligned just fine for IPP

#endif