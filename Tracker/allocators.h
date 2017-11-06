#ifndef ALLOCATORS_H
#define ALLOCATORS_H

#ifndef TRACKER_H
	#include "tracker.h"
#endif

#ifndef PINS_H
	#include "pins.h"
#endif

#ifndef LIST
	#include <list>
	#define LIST
#endif

// parent class for our allocators
class CIPPAllocator_8u_C1 : public CMemAllocator	{

public:
	CIPPAllocator_8u_C1(
		CTrackerFilter* pFilter,
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
	CTrackerFilter* m_pFilter;
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
		CTrackerFilter* pFilter,
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

/*

// allocator that doesn't allocate DIB buffer, it initializes a list of objects we track
class CListAllocatorOut : public CBaseAllocator	{

friend class CPinOutput;

public:
	CListAllocatorOut(
		CTrackerFilter* pFilter,
		CCritSec* pLock,
		CPinOutput* pPin,
		TCHAR* pName,
		LPUNKNOWN pUk,
		HRESULT* pHr)	:
			CBaseAllocator(pName, pUk, pHr, TRUE, TRUE),
			m_pFilter(pFilter),
			m_pLock(pLock),
			m_ppOutputArray(NULL)
	{
			ASSERT(!FAILED(&pHr));
	}
	
	~CListAllocatorOut()	{	Decommit();	}
	
	// This goes in the factory template table to create new instances
    static CUnknown *CreateInstance(LPUNKNOWN, HRESULT *);
	STDMETHODIMP SetProperties(ALLOCATOR_PROPERTIES* pRequest,ALLOCATOR_PROPERTIES* pActual);

	// helper method
	HRESULT SetVIHFromPin();

protected:

	void Free(void);
	HRESULT Alloc(void);

	VIDEOINFOHEADER* pVih;
	// filter pointer, for accessing stuff from CIPPAllocator_8u_C1 methods
	CTrackerFilter* m_pFilter;
	CCritSec* m_pLock;
	CPinOutput* m_pPin;
	CMovingObject** m_ppOutputArray;	// substitutes m_pBuffer in a way

private:
	HRESULT	AllocAdditional( void );
	HRESULT	FreeAdditional( void );
};

*/


// parent class for our allocators
class CListAllocatorOut : public CMemAllocator	{
	friend class CPinOutput;

public:
	CListAllocatorOut(
		CTrackerFilter* pFilter,
		CCritSec* pLock,
		CPinOutput* pPin,
		TCHAR* pName,
		LPUNKNOWN pUk,
		HRESULT* pHr)	:
			CMemAllocator(pName, pUk, pHr),
			m_pFilter(pFilter),
			m_pLock(pLock),
			m_pOutputArray(NULL)
	{
			ASSERT(!FAILED(&pHr));
			AllocAdditional();
	}
	
	~CListAllocatorOut()	{	ReallyFree();	}
	
	STDMETHODIMP SetProperties(ALLOCATOR_PROPERTIES* pRequest,ALLOCATOR_PROPERTIES* pActual);

protected:

	void ReallyFree(void);

	VIDEOINFOHEADER* pVih;
	// filter pointer, for accessing stuff from CIPPAllocator_8u_C1 methods
	CTrackerFilter* m_pFilter;
	CCritSec* m_pLock;

	CMovingObject* m_pOutputArray;	// substitutes m_pBuffer in a way
private:
	HRESULT Alloc(void);
	// override to allocate additional memory
	HRESULT	AllocAdditional( void );
	HRESULT	FreeAdditional( void );
};


#endif