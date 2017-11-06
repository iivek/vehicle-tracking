#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#ifndef FAME2D_H
	#include "fame2D.h"
#endif

#ifndef PINS_H
	#include "pins.h"
#endif

// parent class for our allocators
class CIPPAllocator_8u_C1 : public CMemAllocator	{

public:
	CIPPAllocator_8u_C1(
		CFame2D* pFilter,
		CCritSec* pLock,
		TCHAR* pName,
		LPUNKNOWN pUk,
		HRESULT* pHr, 
		IppiSize sizePix,
		int stepInBytes_8u_C1) :
			CMemAllocator(pName,pUk,pHr),
			m_pFilter(pFilter),
			m_pLock(pLock),
			size(sizePix),
			stepInBytes(stepInBytes_8u_C1)
	{
			ASSERT(!FAILED(&pHr));

	}
	
	~CIPPAllocator_8u_C1()	{	ReallyFree();	}
	
	virtual HRESULT SetVIHFromPin() PURE;
	STDMETHODIMP SetProperties(ALLOCATOR_PROPERTIES* pRequest,ALLOCATOR_PROPERTIES* pActual);

protected:

	void ReallyFree(void);

	VIDEOINFOHEADER* pVih;
	IppiSize size;
	int stepInBytes;
	// filter pointer, for accessing stuff from CIPPAllocator_8u_C1 methods
	CFame2D* m_pFilter;
	CCritSec* m_pLock;

private:
	HRESULT Alloc(void);
	// override to allocate additional memory
	virtual HRESULT	AllocAdditional( void )	{return NOERROR;}
	virtual HRESULT	FreeAdditional( void )	{return NOERROR;}
};


// output allocator; in Alloc() we allocate extra memory needed in the worker thread
class CIPPAllocatorIn_8u_C1 : public CIPPAllocator_8u_C1 	{

	friend class CPinInput;

public:
	CIPPAllocatorIn_8u_C1(
		CFame2D* pFilter,
		CCritSec* pLock,
		CPinInput* pPin,
		TCHAR* pName,
		LPUNKNOWN pUk,
		HRESULT* pHr, 
		IppiSize sizePix,
		int stepInBytes_8u_C1) :
	CIPPAllocator_8u_C1(pFilter, pLock, pName, pUk, pHr, sizePix, stepInBytes_8u_C1),
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


// output allocator; in Alloc() we allocate extra memory needed in the worker thread
class CIPPAllocatorOut_8u_C1 : public CIPPAllocator_8u_C1 	{

	friend class CPinOutput;

public:
	CIPPAllocatorOut_8u_C1(
		CFame2D* pFilter,
		CCritSec* pLock,
		CPinOutput* pPin,
		TCHAR* pName,
		LPUNKNOWN pUk,
		HRESULT* pHr, 
		IppiSize sizePix,
		int stepInBytes_8u_C1,
		int stepInBytes_32f_C1) :
			CIPPAllocator_8u_C1(pFilter, pLock, pName, pUk, pHr, sizePix, stepInBytes_8u_C1),
			stepInBytes_32f_C1(stepInBytes_32f_C1),
			m_pPin(pPin)
	{	ASSERT(!FAILED(&pHr));	}
		
	~CIPPAllocatorOut_8u_C1()	{}

	HRESULT SetVIHFromPin();
	
	int stepInBytes_32f_C1;
protected:
	CPinOutput* m_pPin;

private:
	HRESULT	AllocAdditional( void );
	HRESULT	FreeAdditional( void );
};


#endif