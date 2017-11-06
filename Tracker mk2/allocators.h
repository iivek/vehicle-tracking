#ifndef ALLOCATORS_H
#define ALLOCATORS_H

#ifndef IPP_H
	#include <ipp.h>
	#define IPP_H
#endif

#ifndef TRACKER_MKII_H
	#include "tracker mkII.h"
#endif

#ifndef BASEMULTIIOFILTER_H
	#include "..\BaseMultiIOFilter\BaseMultiIOFilter.h"
#endif

#ifndef STREAMS_H
	#include <Streams.h>    // DirectShow (includes windows.h)
	#define STREAMS_H
#endif

#ifndef PINS_H
	#include "pins.h"
#endif

#ifndef LIST
	#include <list>
	#define LIST
#endif


// forward declarations
class CTrackerMkII;
class CMovingObject;

class CListAllocatorOut : public CMemAllocator	{
	friend class CPinOutput;

public:
	CListAllocatorOut(
		CTrackerMkII* pFilter,
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
	CTrackerMkII* m_pFilter;
	CCritSec* m_pLock;

	CMovingObject* m_pOutputArray;	// substitutes m_pBuffer in a way
private:
	HRESULT Alloc(void);
	// override to allocate additional memory
	HRESULT	AllocAdditional( void );
	HRESULT	FreeAdditional( void );
};

class CIPPAllocator_8u_C3 : public CMemAllocator	{

public:
	CIPPAllocator_8u_C3(
		CBaseMultiIOFilter* pFilter,
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
	
	~CIPPAllocator_8u_C3()	{	ReallyFree();	}
	
	virtual HRESULT SetVIHFromPin() PURE;
	STDMETHODIMP SetProperties(ALLOCATOR_PROPERTIES* pRequest,ALLOCATOR_PROPERTIES* pActual);
	STDMETHODIMP GetBuffer(	IMediaSample **ppBuffer,
						REFERENCE_TIME *pStartTime, 
						REFERENCE_TIME *pEndTime,
						DWORD dwFlags	);

protected:

	void ReallyFree(void);

	VIDEOINFOHEADER* pVih;
	IppiSize size;
	int stepInBytes;
	// filter pointer, for accessing stuff from CIPPAllocator_8u_C1 methods
	CBaseMultiIOFilter* m_pFilter;
	CCritSec* m_pLock;

private:
	HRESULT Alloc(void);
	// override to allocate additional memory
	virtual HRESULT	AllocAdditional( void )	{return NOERROR;}
	virtual HRESULT	FreeAdditional( void )	{return NOERROR;}
};


// output allocator; in Alloc() we allocate extra memory needed in the worker thread
class CIPPAllocatorOut_8u_C3 : public CIPPAllocator_8u_C3 	{

	friend class CPinOutput;

public:
	CIPPAllocatorOut_8u_C3(
		CBaseMultiIOFilter* pFilter,
		CCritSec* pLock,
		CPinOutput* pPin,
		TCHAR* pName,
		LPUNKNOWN pUk,
		HRESULT* pHr	) :
	CIPPAllocator_8u_C3(pFilter, pLock, pName, pUk, pHr),
	m_pPin(pPin)
	{	ASSERT(!FAILED(&pHr));	}
		
	~CIPPAllocatorOut_8u_C3()	{}

	HRESULT SetVIHFromPin();
	
protected:
	CPinOutput* m_pPin;

private:
	HRESULT	AllocAdditional( void );
	HRESULT	FreeAdditional( void );
};

#endif