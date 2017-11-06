/*#ifndef ALLOCATORS_H
#define ALLOCATORS_H

#ifndef TEST_H
	#include "test.h"
#endif


class CIPPAllocator_8u_C1 : public CMemAllocator	{

	friend class CPinOutput;

public:
	CIPPAllocator_8u_C1(CTransformFilter* pExtractY,TCHAR * pName, LPUNKNOWN pUk, HRESULT * pHr)
		: CMemAllocator(pName,pUk,pHr), m_pExtractY(pExtractY)	{

			DbgLog((LOG_TRACE,0,TEXT("OOOOOOOOOOOOOOfasdfasd! _C1")));
			if(FAILED(&pHr))
				DbgLog((LOG_TRACE,0,TEXT("OOOOOOOOOOOOOOOOOOOOOOOO NE!_C1")));
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
	CTransformFilter* m_pExtractY; 
};



class CIPPAllocator_8u_C3 : public CMemAllocator	{

	friend class CPinInput;

public:
	CIPPAllocator_8u_C3(CTransformFilter* pExtractY,TCHAR * pName, LPUNKNOWN pUk, HRESULT* pHr)
		: CMemAllocator(pName,pUk,pHr), m_pExtractY(pExtractY)	{
			DbgLog((LOG_TRACE,0,TEXT("CIPPAllocator_8u_C3 - constructeur")));

			DbgLog((LOG_TRACE,0,TEXT("OOOOOOOOOOOOOOfasdfasd! _C3")));
			if(FAILED(&pHr))
				DbgLog((LOG_TRACE,0,TEXT("OOOOOOOOOOOOOOOOOOOOOOOO NE! _C3")));
}
	
	~CIPPAllocator_8u_C3()	{
		ReallyFree();
	}
	
	STDMETHODIMP SetProperties(ALLOCATOR_PROPERTIES* pRequest,ALLOCATOR_PROPERTIES* pActual);
	
protected:
	//called when decommiting, with ippiFree
	void ReallyFree(void);

private:
	HRESULT Alloc(void);

	CCritSec lock;
	
	CTransformFilter* m_pExtractY;
};

#endif
*/