#ifndef ALLOCATORS_H
	#define ALLOCATORS_H

#include "delay.h"

class CIPPAllocator_8u_AC4 : public CMemAllocator	{

	friend class CPinInput;
	friend class CPinOutput;

public:
	CIPPAllocator_8u_AC4(CTransformFilter* pFilter,TCHAR * pName, LPUNKNOWN pUk, HRESULT* pHr) :
		CMemAllocator(pName,pUk,pHr),
		m_pDelayFilter(pFilter)	{}
	
	~CIPPAllocator_8u_AC4()	{
		DbgLog((LOG_TRACE,0,TEXT("	~CIPPAllocatorbla")));
		ReallyFree();
	}
	
	STDMETHODIMP SetProperties(ALLOCATOR_PROPERTIES* pRequest,ALLOCATOR_PROPERTIES* pActual);
	
protected:
	//called when decommiting, with ippsFree
	void ReallyFree(void);

private:
	HRESULT Alloc(void);

	CCritSec lock;
	
	CTransformFilter* m_pDelayFilter;
};
#endif