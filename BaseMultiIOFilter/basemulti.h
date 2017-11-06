#ifndef BASEMULTI_H


class CBaseMultiIOOutputPin : public CBaseOutputPin	{

	friend class CBaseMultiIOFilter;

public:

	IUnknown			*position;			// PosPassThru interface
	CMuxInputPins		inputPins;
	CMuxInputPins		outputPins;


	// connection stuff, we will switch almost everything over to the CBaseMultiIOFilter class
	virtual HRESULT CheckMediaType(const CMediaType *pmt);
	virtual HRESULT SetMediaType(const CMediaType *pmt);
	virtual HRESULT CompleteConnect(IPin *pReceivePin);
	virtual HRESULT BreakConnect();

	virtual HRESULT GetMediaType(int i, CMediaType *pmt);
	virtual HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProps);


protected:
	// pointer to the corresponding filter instance
	CBaseMultiIOFilter* m_pMultiIO;
};


class CBaseMultiIOInputPin : public CBaseInputPin	{

	friend class CBaseMultiIOFilter;

public:
	CBaseMultiIOInputPin(
		TCHAR *pObjectName, CBaseMultiIOFilter *pFilter, HRESULT * phr, LPCWSTR pName);
	virtual ~CBaseMultiIOInputPin();
	
	// IPin interface
	STDMETHODIMP BeginFlush();
    STDMETHODIMP EndFlush();
	STDMETHODIMP EndOfStream();
	// IMemInputPin interface
	STDMETHODIMP Receive(IMediaSample *pMediaSample);

	// connection stuff
	virtual HRESULT CheckStreaming();
	virtual HRESULT CheckMediaType(const CMediaType *pmt);
	virtual HRESULT SetMediaType(const CMediaType *pmt);
	virtual HRESULT CompleteConnect(IPin *pReceivePin);
	virtual HRESULT BreakConnect();

protected:
	CBaseMultiIOFilter* m_pMultiIO;
};

//typedef std::vector<CBaseMuxInputPin *>		CMuxInputPins;


class CBaseMultiIOFilter : public CBaseFilter
{
public:

	CCritSec m_csMultiIO;

	// override this to check if your filter has an output connection to stream to
	virtual HRESULT CheckOutputConnection{ return S_OK };
	// override this to check if your filter has an input connection that is OK
	virtual HRESULT CheckInputConnection{ return S_OK };

	// output I/O
	CComPtr<IStream>	outstream;				// downstream IStream object



}

#endif