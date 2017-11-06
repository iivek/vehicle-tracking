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


class CPinOutput : public CTransformOutputPin	{

public:
	CPinOutput( CTrackerFilter* pFilter, HRESULT * pHr ) :
		CTransformOutputPin( TEXT("OutputPin"), pFilter, pHr, L"Output" ),
		m_pFilter(pFilter)
		{	}

	~CPinOutput()	{
		if( m_pAllocator )
		{
			// already decommitted here
			m_pAllocator = NULL;
		}
	;}

	HRESULT InitializeAllocatorRequirements();
	HRESULT DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc);

private:
	// we override this method to instance our IPP allocator:
	HRESULT InitAllocator(IMemAllocator **ppAlloc);
	
	CListAllocatorOut* m_pAllocator;	// our pin allocator object
	CTrackerFilter* m_pFilter;
};

/*
// Pin that will send std::list<CMovingObject> to appropriate input pin.
// The input pin won't need the IMemInputPin and we won't use IMemAllocator or use allocated buffers.
// The pins will communicate in a custom internal datatype
class CPinOutputCustom : public CBasePin
{
public:
	HRESULT CheckMediaType(const CMediaType *pmt);
	HRESULT CheckConnect();
	HRESULT BreakConnect(void);
	HRESULT CompleteConnect(void);
	HRESULT Active();
	HRESULT Inactive();
	HRESULT Run();

	// IPin interface
	//STDMETHODIMP BeginFlush();
    //STDMETHODIMP EndFlush();
	//STDMETHODIMP EndOfStream();
	//STDMETHODIMP NewSegment(
	//	REFERENCE_TIME tStart,
	//	REFERENCE_TIME tStop,
	//	double dRate);
	STDMETHODIMP EndOfStream();

	// IQualityControl
	STDMETHODIMP Notify();

};
*/

class CPinInput : public CTransformInputPin	{

public:
	CPinInput( CTrackerFilter* pFilter, HRESULT * pHr )
		: CTransformInputPin( TEXT("InputPin"), pFilter, pHr, L"Input" ), m_pFilter(pFilter)	{
	}
	~CPinInput()	{ }
	
	HRESULT InitializeAllocatorRequirements();
	STDMETHODIMP GetAllocator (IMemAllocator** ppAllocator);
	STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps);
	
private:
	CIPPAllocatorIn_8u_C1 *m_pIPPAllocator;	// our pin allocator object
	CTrackerFilter* m_pFilter;
};

// Actually there's no need to make a custom input pin because we insist on format that
// is aligned just fine for IPP

#endif