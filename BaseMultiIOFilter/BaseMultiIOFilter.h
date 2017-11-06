//******************************************************************************************
//	this will be a class suitable to derive from in the future, BUT IT ISN'T QUITE FINISHED
//


// take care with the streaming locks- each input pin has its own lock!



#ifndef BASEMULTIIOFILTER_H
#define BASEMULTIIOFILTER_H

#ifndef STREAMS_H
	#include <streams.h>
	#define STREAMS_H
#endif

//STL
#ifndef MAP
	#include <map>
	#define MAP
#endif

#ifndef BASEMULTIINPUTPIN_H
	#include "BaseMultiInputPin.h"
#endif

#ifndef BASEMULTIOUTPUTPIN_H
	#include "BaseMultiOutputPin.h"
#endif

using namespace std;

typedef map<int, CBaseMultiIOInputPin*> INPUT_PINS;
typedef map<int, CBaseMultiIOOutputPin*> OUTPUT_PINS;
//typedef map<int, CMediaType> MEDIA_TYPE_LIST;


class CBaseMultiIOFilter : public CBaseFilter
{
	friend class CBaseMultiIOInputPin;
	friend class CBaseMultiIOOutputPin;

public:

	CBaseMultiIOFilter(TCHAR *pObjectName, LPUNKNOWN lpUnk, CLSID clsid,
		int initialInput, int finalInput, int initialOutput, int finalOutput);
	~CBaseMultiIOFilter(void);

	// variable initial number of pins
	HRESULT InitializePins();

	// we work with multiple input and output pins
	int GetPinCount();
	CBasePin* GetPin(int n);

	// Output pin methods
	virtual HRESULT DecideBufferSize(
		IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pRequestProperties, CBaseMultiIOOutputPin* caller)PURE;
	virtual HRESULT GetMediaType(int iPosition, CMediaType* pmt, CBaseMultiIOOutputPin* caller)PURE;
	virtual HRESULT CheckOutputType(const CMediaType* pMediaType);
	

	// called when an input pin has a media sample
	//virtual HRESULT Receive(IMediaSample *pSample, int nIndex )PURE;
	virtual HRESULT SetInputType(CBaseMultiIOInputPin* caller, const CMediaType *pmt);

	// override to have access at the time point when a pin gets connected
	// we create and destroy pins here, as with muxes and inftees, ( beyond InitializePins() )
	HRESULT OnConnect(CBaseMultiIOInputPin* caller);
	HRESULT OnConnect(CBaseMultiIOOutputPin* caller);
	HRESULT OnDisconnect(CBaseMultiIOInputPin* caller);
	HRESULT OnDisconnect(CBaseMultiIOOutputPin* caller);

	// Sets minimum and maximum of number of pins
	void SetNrPins(int initialInput, int finalInput, int initialOutput, int finalOutput);
	// returns number of connected input/output pins
	int AllInputConnected();
	int AllOutputConnected();

	// Streaming calls delegated from input pins. typically it sends the message downstream to
	// all output pins, so you might need to override it
	virtual STDMETHODIMP EndOfStream(CBaseMultiIOInputPin* caller);
	virtual STDMETHODIMP BeginFlush(CBaseMultiIOInputPin* caller);
	virtual STDMETHODIMP EndFlush(CBaseMultiIOInputPin* caller);
	virtual STDMETHODIMP NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate, CBaseMultiIOInputPin* caller);

	virtual HRESULT CheckInputType(const CMediaType *mtIn);	// anything is supported by default

	// Each input pin will have input queues to optimize performance and facilitate synchronization

	// Used to create output queues
	HRESULT Active();
    HRESULT Inactive();

	// Overriden to pass data to the output queues
    HRESULT Deliver(IMediaSample *pMediaSample);
    HRESULT DeliverEndOfStream();
    HRESULT DeliverBeginFlush();
    HRESULT DeliverEndFlush();
    HRESULT DeliverNewSegment(
                    REFERENCE_TIME tStart,
                    REFERENCE_TIME tStop,
                    double dRate);
	

protected:

	// create an input/output pin with parameter as name. override them to provide custom pins
	HRESULT AddInputPin(LPCWSTR name);	
	HRESULT AddOutputPin(LPCWSTR name);

	// List of input pins of the filter
	INPUT_PINS m_inputPins;
	// List of output pins of the filter
	OUTPUT_PINS m_outputPins;
	// Media types of the connected input pins

	unsigned int m_nrInitialIn, m_nrInitialOut;
	unsigned int m_nrFinalIn, m_nrFinalOut;

	CCritSec m_csMultiIO;
	CCritSec m_csReceive;		//receive lock

private:

	// used to dynamically delete pins, called from NonDelegatingRelease()
	void DeleteInputPin(CBaseMultiIOInputPin *pPin);
	void DeleteOutputPin(CBaseMultiIOOutputPin *pPin);

	// used to give a specific name to newly created filters
	int m_pinCountOut;
	int m_pinCountIn;
};

#endif