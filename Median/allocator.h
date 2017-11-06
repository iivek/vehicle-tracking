#ifndef ALLOCATOR_H

#ifndef MEDIAN2D_H
	#include "median2D.h"
	#define MEDIAN2D_H
#endif

class CIPPAllocator_8u_C1 : public CMemAllocator	{

	friend class CPinInput;
	friend class CPinOutput;

public:
	CIPPAllocator_8u_C1(CTransformFilter* pFilter,TCHAR* pName, LPUNKNOWN pUk, HRESULT* pHr)
		: CMemAllocator(pName,pUk,pHr), m_pFilter(pFilter)	{
	if(FAILED(&pHr))
				DbgLog((LOG_TRACE,0,TEXT("OOOOOOOOOOOOOOOOOOOOOOOO NE! _C1")));
		}
	
	~CIPPAllocator_8u_C1()
	{
		ReallyFree();
	}
	
	STDMETHODIMP SetProperties(ALLOCATOR_PROPERTIES* pRequest,ALLOCATOR_PROPERTIES* pActual);
	
protected:
	//called when decommiting, with ippiFree
	void ReallyFree(void);

private:
	HRESULT Alloc(void);

	CCritSec lock;
	// filter pointer, for accessing stuff from CIPPAllocator_8u_C1 methods
	CTransformFilter* m_pFilter; 
};

#endif