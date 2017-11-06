#ifndef PINS_H
	#define PINS_H

#include "Allocators.h"

class CPinOutput: public CTransformOutputPin	{

	friend class CDelayFilter;

public:
	CPinOutput( CTransformFilter* pFilter, HRESULT * pHr )
		: CTransformOutputPin( TEXT("OutputPin"), pFilter, pHr, L"Output" ),
		m_pDelayFilter(pFilter),
		m_pIPPAllocator(NULL)
	{
	}

	~CPinOutput()
	{
		if(m_pIPPAllocator)
		{
			//we have been fully released, no need to delete
			m_pIPPAllocator = NULL;
		}
	}

	HRESULT DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc);
	HRESULT InitializeAllocatorRequirements();
	//HRESULT BreakConnect();

private:
	// we override this method to instance our IPP allocator:
	HRESULT InitAllocator(IMemAllocator **ppAlloc);
	CIPPAllocator_8u_AC4* m_pIPPAllocator;	// our pin allocator object

	CTransformFilter* m_pDelayFilter;
};


class CPinInput : public CTransformInputPin	{

public:
	CPinInput( CTransformFilter* pFilter, HRESULT * pHr )
		: CTransformInputPin( TEXT("InputPin"), pFilter, pHr, L"Input" ),
		m_pDelayFilter(pFilter)
		//,m_pIPPAllocator(NULL)
	{
	}
	~CPinInput()
	{
/*		if(m_pIPPAllocator){
			//we have been fully released, no need to delete
			m_pIPPAllocator = NULL;
		}
*/
	}
	
	//STDMETHODIMP GetAllocator (IMemAllocator** ppAllocator);
	STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps);

private:
	//CIPPAllocator_8u_AC4 *m_pIPPAllocator;	// our pin allocator object
	CTransformFilter* m_pDelayFilter;
};

// Actually there's no need to make a custom input pin because we insist on format that
// is aligned just fine for IPP
#endif