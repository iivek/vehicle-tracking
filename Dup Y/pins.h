#ifndef PINS_H
#define PINS_H

#ifndef ALLOCATOR_H
	#include "allocator.h"
	#define ALLOCATOR_H
#endif

class CPinOutput: public CTransformOutputPin	{

public:
	CPinOutput( CTransformFilter* pFilter, HRESULT * pHr )	:
		CTransformOutputPin( TEXT("OutputPin"), pFilter, pHr, L"Output" ), m_pFilter(pFilter),
		m_pIPPAllocator(NULL)	{	}

	~CPinOutput()
	{
		if( m_pIPPAllocator )
		{
			//we have been fully released, no need to delete
			m_pIPPAllocator = NULL;
		}
	}

	HRESULT DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc);
	HRESULT InitializeAllocatorRequirements();

private:
	// we override this method to instance our IPP allocator:
	HRESULT InitAllocator(IMemAllocator **ppAlloc);
	
	CIPPAllocator_8u_C3* m_pIPPAllocator;	// our pin allocator object
	CTransformFilter* m_pFilter;

	static CCritSec lock;
};


class CPinInput : public CTransformInputPin	{

public:
	CPinInput( CTransformFilter* pFilter, HRESULT * pHr )	:
			CTransformInputPin( TEXT("InputPin"), pFilter, pHr, L"Input" ),
			m_pIPPAllocator(NULL),
			m_pFilter(pFilter)	{
	}
	~CPinInput()
	{
		if( m_pIPPAllocator )
		{
			//we have been fully released, no need to delete
			m_pIPPAllocator = NULL;
		}
	}
	
	STDMETHODIMP GetAllocator (IMemAllocator** ppAllocator);
	STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps);

private:
	CIPPAllocator_8u_C1 *m_pIPPAllocator;	// our pin allocator object
	CTransformFilter* m_pFilter;

	CCritSec lock;
};

// Actually there's no need to make a custom input pin because we insist on format that
// is aligned just fine for IPP

#endif