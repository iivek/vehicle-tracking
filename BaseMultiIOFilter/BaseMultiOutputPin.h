#ifndef BASEMULTIOUTPUTPIN_H
#define BASEMULTIOUTPUTPIN_H

#ifndef STREAMS_H
	#include <streams.h>
	#define STREAMS_H
#endif


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

// doesn't expose seeking related interfaces - override NonDelegatingQueryInterface
// in your derived class if you need seeking functionality
class CBaseMultiIOOutputPin : public CBaseOutputPin	{

	friend class CBaseMultiIOFilter;

public:
	// helper
	CMediaType& CurrentMediaType() { return m_mt; }

	CBaseMultiIOOutputPin(
		CBaseMultiIOFilter* pFilter, CCritSec* pLock, HRESULT* phr, LPCWSTR pName, int nIndex);
	virtual ~CBaseMultiIOOutputPin();

	// connection stuff, we will switch almost everything over to the CBaseMultiIOFilter class
	virtual HRESULT CheckMediaType(const CMediaType* pmt);
	virtual HRESULT GetMediaType(int iPosition, CMediaType* pmt);
	virtual HRESULT CompleteConnect(IPin *pReceivePin);
	// override to release any allocator we are holding
	virtual HRESULT BreakConnect();
	virtual HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProps);

	// IQualityControl override
	// override it in your derived class because, the way it is here, it's slow for filters with many input pins
	virtual STDMETHODIMP Notify(IBaseFilter * pSender, Quality q);

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

	CMyOutputQueue *m_pOutputQueue;  // Streams data to the peer pin

	// Override since the life time of pins and filters are not the same
	STDMETHODIMP_(ULONG) NonDelegatingAddRef();
	STDMETHODIMP_(ULONG) NonDelegatingRelease();

	// our reference counter
	LONG m_cOurRef;

protected:
	// pointer to the corresponding filter instance
	CBaseMultiIOFilter* m_pMultiIO;

	int m_pinIndex;
	// we could use pointers to pins
};

// to provide custom allocators override (DecideAllocator--public--) and InitAllocator--private--

#endif