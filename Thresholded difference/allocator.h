#ifndef ALLOCATOR_H

#ifndef FAME2D_H
	#include "fame2D.h"
	#define FAME2D_H
#endif

class CIPPAllocator_8u_C1 : public CMemAllocator	{

	friend class CPinInput;

public:
	CIPPAllocator_8u_C1(CTransformFilter* pFilter,TCHAR* pName, LPUNKNOWN pUk, HRESULT* pHr)
		: CMemAllocator(pName,pUk,pHr), m_pFilter(pFilter)	{
		ASSERT(!FAILED(&pHr));
		}
	
	~CIPPAllocator_8u_C1()	{	ReallyFree();
	}
	
	STDMETHODIMP SetProperties(ALLOCATOR_PROPERTIES* pRequest,ALLOCATOR_PROPERTIES* pActual);
	
protected:
	//called when decommiting, with ippiFree
	void ReallyFree(void);
	// filter pointer, for accessing stuff from CIPPAllocator_8u_C1 methods
	CTransformFilter* m_pFilter; 

	CCritSec lock;

private:
	HRESULT Alloc(void);
};

// output allocator; the difference is that in Alloc() we allocate extra memory needed in
// CThreshDiff::Transform()
class CIPPAllocatorOut_8u_C1 : public CIPPAllocator_8u_C1 	{

	friend class CPinOutput;

public:
	CIPPAllocatorOut_8u_C1(CTransformFilter* pFilter,TCHAR* pName, LPUNKNOWN pUk, HRESULT* pHr) :
	CIPPAllocator_8u_C1(pFilter, pName, pUk, pHr)	{
		ASSERT(!FAILED(&pHr));
	}
		
	~CIPPAllocatorOut_8u_C1()	{	ReallyFree();	}
	
protected:
	//called when decommiting, with ippiFree
	void ReallyFree(void);

private:
	HRESULT Alloc(void);

};

#endif