#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#ifndef PINS_H
	#include "pins.h"
#endif

#ifndef MOVDETECTOR_H
	#include "movdetector.h"
#endif

// parent class for our allocators
class CIPPAllocator_8u_C1 : public CMemAllocator	{

public:
	CIPPAllocator_8u_C1(
		CMovDetectorFilter* pFilter,
		CCritSec* pLock,
		TCHAR* pName,
		LPUNKNOWN pUk,
		HRESULT* pHr) :
			CMemAllocator(pName,pUk,pHr),
			m_pFilter(pFilter),
			m_pLock(pLock)
	{
			ASSERT(!FAILED(&pHr));

	}
	
	~CIPPAllocator_8u_C1()
	{
		ReallyFree();
	}
	
	virtual HRESULT SetVIHFromPin() PURE;
	STDMETHODIMP SetProperties(ALLOCATOR_PROPERTIES* pRequest,ALLOCATOR_PROPERTIES* pActual);

protected:

	void ReallyFree(void);

	VIDEOINFOHEADER* pVih;
	IppiSize size;
	// filter pointer, for accessing stuff from CIPPAllocator_8u_C1 methods
	CMovDetectorFilter* m_pFilter;
	CCritSec* m_pLock;

private:
	HRESULT Alloc(void);
	// override to allocate additional memory
	virtual HRESULT	AllocAdditional( void )	{return NOERROR;}
	virtual HRESULT	FreeAdditional( void )	{return NOERROR;}
};


class CIPPAllocatorIn_8u_C1 : public CIPPAllocator_8u_C1 	{

	friend class CPinInput;

public:
	CIPPAllocatorIn_8u_C1(
		CMovDetectorFilter* pFilter,
		CCritSec* pLock,
		CPinInput* pPin,
		TCHAR* pName,
		LPUNKNOWN pUk,
		HRESULT* pHr) :
	CIPPAllocator_8u_C1(pFilter, pLock, pName, pUk, pHr),
	m_pPin(pPin)
	{	ASSERT(!FAILED(&pHr));	}
		
	~CIPPAllocatorIn_8u_C1()	{}

	HRESULT SetVIHFromPin();

protected:
	CPinInput* m_pPin;

private:
	HRESULT	AllocAdditional( void );
	HRESULT	FreeAdditional( void );
};

class CIPPAllocatorMovement_8u_C1 : public CIPPAllocator_8u_C1
{

	friend class CPinOutputMovement; 
	CIPPAllocatorMovement_8u_C1(
		CMovDetectorFilter* pFilter,
		CCritSec* pLock,
		CPinOutputMovement* pPin,
		TCHAR* pName,
		LPUNKNOWN pUk,
		HRESULT* pHr) :
	CIPPAllocator_8u_C1(pFilter, pLock, pName, pUk, pHr),
	m_pPin(pPin)
	{	ASSERT(!FAILED(&pHr));	}

	~CIPPAllocatorMovement_8u_C1()	{}

	HRESULT SetVIHFromPin();

protected:
	CPinOutputMovement* m_pPin;

private:
	HRESULT AllocAdditional();
	HRESULT FreeAdditional();

};

class CIPPAllocatorBackground_8u_C1 : public CIPPAllocator_8u_C1
{
	friend class CPinOutputBackground;
	CIPPAllocatorBackground_8u_C1(
		CMovDetectorFilter* pFilter,
		CCritSec* pLock,
		CPinOutputBackground* pPin,
		TCHAR* pName,
		LPUNKNOWN pUk,
		HRESULT* pHr) :
	CIPPAllocator_8u_C1(pFilter, pLock, pName, pUk, pHr),
	m_pPin(pPin)
	{	ASSERT(!FAILED(&pHr));	}

	~CIPPAllocatorBackground_8u_C1()	{}

	HRESULT SetVIHFromPin();

protected:
	CPinOutputBackground* m_pPin;

private:
	HRESULT AllocAdditional();
	HRESULT FreeAdditional();

};
#endif