#ifndef BASEMULTIINPUTPIN_H
#define BASEMULTIINPUTPIN_H

#ifndef STREAMS_H
	#include <streams.h>
	#define STREAMS_H
#endif


class CBaseMultiIOInputPin : public CBaseInputPin	{

	friend class CBaseMultiIOFilter;
	friend class CBaseMultiIOOutputPin;	// so we can access quality sink from an output pin

public:

	CBaseMultiIOInputPin(
		CBaseMultiIOFilter* pFilter, CCritSec* pLock, HRESULT* phr, LPCWSTR pName, int nIndex);
	virtual ~CBaseMultiIOInputPin();

	// helper
	CMediaType& CurrentMediaType() { return m_mt; };
	
	// IPin interface
	STDMETHODIMP BeginFlush();
    STDMETHODIMP EndFlush();
	STDMETHODIMP EndOfStream();
	STDMETHODIMP NewSegment(
		REFERENCE_TIME tStart,
		REFERENCE_TIME tStop,
		double dRate);

	// IMemInputPin interface	
	// we pass pinIndex because we want to know which pin called us
	virtual STDMETHODIMP Receive(IMediaSample *pMediaSample);	// do something useful


	// connection stuff
	//virtual HRESULT CheckStreaming();

	// override it to check input media type.
	virtual HRESULT CheckMediaType(const CMediaType *pmt);
	virtual HRESULT SetMediaType(const CMediaType *pmt);
	virtual HRESULT CompleteConnect(IPin *pReceivePin);
	virtual HRESULT BreakConnect();

	// quality control stuff
	HRESULT PassNotify(Quality q);

	// overriden since the life times of pins and filters are not the same
	STDMETHODIMP_(ULONG) NonDelegatingAddRef();
	STDMETHODIMP_(ULONG) NonDelegatingRelease();


	// our reference counter
	LONG m_cOurRef;

protected:
	int m_pinIndex;
	CBaseMultiIOFilter* m_pMultiIO;


	// alternatively we could use pointers to pins
};

// to provide custom allocator override GetAllocator--public-- and
// GetAllocatorRequirements--public--

#endif