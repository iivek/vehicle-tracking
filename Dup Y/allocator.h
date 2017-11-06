#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "dupy.h"

class CIPPAllocator_8u_C1 : public CMemAllocator	{

	friend class CPinInput;

public:
	CIPPAllocator_8u_C1(CTransformFilter* pFilter,TCHAR* pName, LPUNKNOWN pUk, HRESULT* pHr)
		: CMemAllocator(pName,pUk,pHr), m_pFilter(pFilter)	{	}
	
	~CIPPAllocator_8u_C1()	{	ReallyFree();	}
	
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



class CIPPAllocator_8u_C3 : public CMemAllocator	{

	// for TEST also
	friend class CPinOutput;

public:
	CIPPAllocator_8u_C3::CIPPAllocator_8u_C3(CTransformFilter* pFilter,TCHAR * pName, LPUNKNOWN pUk, HRESULT * pHr)
		: CMemAllocator(pName,pUk,pHr), m_pFilter(pFilter)	{
	}
	
	~CIPPAllocator_8u_C3()
	{
		if(m_pBuffer) ReallyFree();
	}
	
	STDMETHODIMP SetProperties(ALLOCATOR_PROPERTIES* pRequest,ALLOCATOR_PROPERTIES* pActual);
	
protected:
	//called when decommiting, with ippiFree
	void ReallyFree(void);

private:
	HRESULT Alloc(void);

	CCritSec lock;
	// filter pointer, for accessing stuff from CIPPAllocator_8u_C3 methods
	CTransformFilter* m_pFilter;
};

#endif