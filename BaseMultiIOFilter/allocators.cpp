/*#ifndef ALLOCATORS_H
	#include "Allocators.h"
#endif

// can be called to change the buffering
STDMETHODIMP CIPPAllocator_8u_C1 :: SetProperties(ALLOCATOR_PROPERTIES* pRequest,
											ALLOCATOR_PROPERTIES* pActual)	{

	CheckPointer(pRequest, E_POINTER);
    CheckPointer(pActual, E_POINTER);
    ValidateReadWritePtr(pActual, sizeof(ALLOCATOR_PROPERTIES));

	CAutoLock cAutolock(&lock);

	ZeroMemory(pActual, sizeof(ALLOCATOR_PROPERTIES));
    ASSERT(pRequest->cbBuffer > 0);

	 if (m_bCommitted) {
        return VFW_E_ALREADY_COMMITTED;
    }

	// no outstanding buffers
    if (m_lAllocated != m_lFree.GetCount()) {
        return VFW_E_BUFFERS_OUTSTANDING;
    }
	
	// everything as we requested in DecideBufferSize
	CExtractY* pFilt = (CExtractY*) m_pExtractY;
	pActual->cbBuffer = pRequest->cbBuffer;
	pActual->cBuffers = pRequest->cBuffers;
	pActual->cbAlign = pRequest->cbAlign;
    pActual->cbPrefix = pRequest->cbPrefix;

    m_bChanged = TRUE;

	DbgLog((LOG_TRACE,0,TEXT("%d %d %d %d"),m_lSize,m_lCount,m_lAlignment,m_lPrefix ));

	return NOERROR;
}


HRESULT	CIPPAllocator_8u_C1 :: Alloc( void )	{

	CAutoLock cAutolock(&lock);

	CExtractY* pFilt = (CExtractY*) m_pExtractY;
	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) pFilt->m_pInput->CurrentMediaType().Format();

	DbgLog((LOG_TRACE,0,TEXT("CIPPAllocator_8u_C1Out::Alloc")));

	// we call base class confirmation to proceed with allocation process
	DbgLog((LOG_TRACE,0,TEXT("_C1, a call to CMemAllocator::Alloc()")));
	HRESULT hr = CMemAllocator::Alloc();

	if (FAILED(hr)) {
		DbgLog((LOG_TRACE,0,TEXT("_C1, CMEMAllocator::Alloc() hr=0x%x"), hr));
		return hr;
	}
	
	if (hr == S_FALSE) {
		ASSERT(m_pBuffer);
		return NOERROR;
	}
	ASSERT(hr == S_OK);

	// if buffer isn't free accidentally, free it
	if (m_pBuffer) {
		ReallyFree();
	}
	
	DbgLog((LOG_TRACE,0,TEXT("CIPPAllocator_8u_C1Out::Alloc; allocation.....working")));
	
	// memory allocation by Intel IPP allocation methods
	int step = pVih->bmiHeader.biWidth + pFilt->stride_8u_C1;
	m_pBuffer = ippiMalloc_8u_C1(pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, &step);
	if (m_pBuffer == NULL) {
		return E_OUTOFMEMORY;
	}

	DbgLog((LOG_TRACE,0,TEXT("CIPPAllocator_8u_C1Out::Alloc; allocation.....DONE :)")));

	CMediaSample *pSample;

	//number of allocated buffers
	ASSERT(m_lAllocated == 0);

	// sample creation with m_lSize including prefix
	// notice that when we call GetPointer we get size of m_lSize without m_lPrefix
	LPBYTE pNext = m_pBuffer;
	for (; m_lAllocated < m_lCount; m_lAllocated++, pNext += m_lSize) {
		pSample = new CMediaSample(
			NAME("Default memory media sample"),
			this,
			&hr,
			pNext + m_lPrefix,      
			m_lSize
			);               

		ASSERT(SUCCEEDED(hr));
		if (pSample == NULL) {
			return E_OUTOFMEMORY;
		}

		//we put sample in the list
		m_lFree.Add(pSample);
		NotifySample();
	}

	m_bChanged = FALSE;
	
	return NOERROR;
}

void CIPPAllocator_8u_C1 :: ReallyFree() 
{
	CAutoLock cAutolock(&lock);
	//DbgLog((LOG_TRACE,1,TEXT("ReallyFree called")));

	// Should never be unallocating unless all buffers are freed 
	ASSERT(m_lAllocated == m_lFree.GetCount());

	// Free up all the CMediaSamples 
	CMediaSample *pSample;
	for (;;)
	{
		pSample = m_lFree.RemoveHead();
		if (pSample != NULL)
			delete pSample;
		else
			break;
	}

	//deallocation with IPP
	if (m_pBuffer) {
		ippiFree(m_pBuffer);
		m_pBuffer = NULL;
	}

	m_lAllocated = 0;
}

STDMETHODIMP CIPPAllocator_8u_C3 :: SetProperties(ALLOCATOR_PROPERTIES* pRequest,
											ALLOCATOR_PROPERTIES* pActual)	{

	DbgLog((LOG_TRACE,0,TEXT("CIPPAllocator_8u_C3, setprops")));

	CheckPointer(pRequest, E_POINTER);
    CheckPointer(pActual, E_POINTER);
    ValidateReadWritePtr(pActual, sizeof(ALLOCATOR_PROPERTIES));
	CAutoLock cAutolock(&lock);
	
	ZeroMemory(pActual, sizeof(ALLOCATOR_PROPERTIES));

    ASSERT(pRequest->cbBuffer > 0);

	 if (m_bCommitted) {
        return VFW_E_ALREADY_COMMITTED;
    }

	// no outstanding buffers
    if (m_lAllocated != m_lFree.GetCount()) {
        return VFW_E_BUFFERS_OUTSTANDING;
    }
	
	// everything as we requested in DecideBufferSize, we can do it
	pActual->cbBuffer = pRequest->cbBuffer;
	pActual->cBuffers = pRequest->cBuffers;
	pActual->cbAlign = pRequest->cbAlign;
    pActual->cbPrefix = pRequest->cbPrefix;

    m_bChanged = TRUE;

	DbgLog((LOG_TRACE,0,TEXT("%d %d %d %d"),m_lSize,m_lCount,m_lAlignment,m_lPrefix ));

	return NOERROR;
}


HRESULT	CIPPAllocator_8u_C3 :: Alloc( void )	{

	DbgLog((LOG_TRACE,0,TEXT("CIPPAllocator_8u_C3, alloc")));

	CAutoLock cAutolock(&lock);
	CExtractY* pFilt = (CExtractY*) m_pExtractY;

	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) pFilt->m_pInput->CurrentMediaType().Format();

	//we call base class confirmation to proceed with allocation process
	DbgLog((LOG_TRACE,0,TEXT("_C3, a call to CBaseAllocator::Alloc()")));
	HRESULT hr = CBaseAllocator::Alloc();
	if (FAILED(hr)) {

		DbgLog((LOG_TRACE,0,TEXT("_C3, CBaseAllocator::Alloc() falide")));
		return hr;
	}
	
	/*if (hr == S_FALSE) {
		ASSERT(m_pBuffer);
		return NOERROR;
	}
	ASSERT(hr == S_OK);
	*/

	// if buffer isn't free, by accident, free it
/*	if (m_pBuffer) {
		ReallyFree();
	}
	
	DbgLog((LOG_TRACE,0,TEXT("CIPPAllocator_8u_C3::Alloc; allocation.....working")));
	
	// memory allocation by Intel IPP allocation methods
	int step = pVih->bmiHeader.biWidth + pFilt->stride_8u_C3;
	m_pBuffer = ippiMalloc_8u_C3(pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, &step);
	if (m_pBuffer == NULL) {
		return E_OUTOFMEMORY;
	}

	DbgLog((LOG_TRACE,0,TEXT("CIPPAllocator_8u_C3::Alloc; allocation.....DONE :)")));

	CMediaSample *pSample;

	//number of allocated buffers
	ASSERT(m_lAllocated == 0);

	// sample creation with m_lSize including prefix
	// notice that when we call GetPointer we get size of m_lSize without m_lPrefix
	LPBYTE pNext = m_pBuffer;
	for (; m_lAllocated < m_lCount; m_lAllocated++, pNext += m_lSize) {
		pSample = new CMediaSample(
			NAME("Default memory media sample"),
			this,
			&hr,
			pNext + m_lPrefix,      
			m_lSize
			);               

		ASSERT(SUCCEEDED(hr));
		if (pSample == NULL) {
			return E_OUTOFMEMORY;
		}

		//we put sample in the list
		m_lFree.Add(pSample);
		NotifySample();
	}

	m_bChanged = FALSE;
	
	return NOERROR;
}


void CIPPAllocator_8u_C3 :: ReallyFree() 
{
	CAutoLock cAutolock(&lock);
	//DbgLog((LOG_TRACE,1,TEXT("ReallyFree called")));

	// Should never be unallocating unless all buffers are freed 
	ASSERT(m_lAllocated == m_lFree.GetCount());

	// Free up all the CMediaSamples 
	CMediaSample *pSample;
	for (;;)	{
		pSample = m_lFree.RemoveHead();
		if (pSample != NULL)
			delete pSample;
		else
			break;
	}

	//deallocation with IPP
	if (m_pBuffer) {
		ippiFree(m_pBuffer);
		m_pBuffer = NULL;
	}

	m_lAllocated = 0;
}

*/