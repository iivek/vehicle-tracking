#include "extracty.h"

class CIPPAllocator_8u_C1 : public CMemAllocator	{

	friend class CPinOutput;

public:
	CIPPAllocator_8u_C1(CTransformFilter* pExtractY,TCHAR * pName, LPUNKNOWN pUk, HRESULT * pHr)
		: CMemAllocator(pName,pUk,pHr), m_pExtractY(pExtractY)	{
	}
	
	~CIPPAllocator_8u_C1()
	{
		if(m_pBuffer) ReallyFree();
	}
	
	STDMETHODIMP SetProperties(ALLOCATOR_PROPERTIES* pRequest,ALLOCATOR_PROPERTIES* pActual);
	
protected:
	void ReallyFree(void);

private:
	HRESULT Alloc(void);

	CCritSec lock;
	// filter pointer, for accessing stuff from CIPPAllocator_8u_C1 methods
	CTransformFilter* m_pExtractY; 
};



class CIPPAllocator_8u_AC4 : public CMemAllocator	{

	friend class CPinInput;

public:
	CIPPAllocator_8u_AC4(CTransformFilter* pExtractY,TCHAR * pName, LPUNKNOWN pUk, HRESULT* pHr) :
		CMemAllocator(pName,pUk,pHr),
		m_pExtractY(pExtractY)	{}
	
	~CIPPAllocator_8u_AC4()	{
		if(m_pBuffer) ReallyFree();
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