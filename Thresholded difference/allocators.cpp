#ifndef ALLOCATORS_H
	#include "allocators.h"
#endif


STDMETHODIMP CIPPAllocator_8u_C1 :: SetProperties(ALLOCATOR_PROPERTIES* pRequest,
												  ALLOCATOR_PROPERTIES* pActual)	{

	CAutoLock lock(m_pLock);
	
	CheckPointer(pRequest, E_POINTER);
    CheckPointer(pActual, E_POINTER);
    ValidateReadWritePtr(pActual, sizeof(ALLOCATOR_PROPERTIES));
	
	ZeroMemory(pActual, sizeof(ALLOCATOR_PROPERTIES));
    ASSERT(pRequest->cbBuffer > 0);

	 if (m_bCommitted) {
        return VFW_E_ALREADY_COMMITTED;
    }

	// no outstanding buffers
    if (m_lAllocated != m_lFree.GetCount()) {
        return VFW_E_BUFFERS_OUTSTANDING;
    }
	
	if(pRequest->cBuffers>0)	{
		m_lCount=pRequest->cBuffers;
	}
	else	{
		m_lCount=1; // well it would be nice to have at least one
	}

	// everything as we requested in DecideBufferSize
	pActual->cbBuffer = pRequest->cbBuffer;
	pActual->cBuffers = pRequest->cBuffers;
	pActual->cbAlign = pRequest->cbAlign;
    pActual->cbPrefix = pRequest->cbPrefix;

    m_bChanged = TRUE;

	return NOERROR;
}

HRESULT CIPPAllocator_8u_C1::GetBuffer(
    IMediaSample **ppBuffer,
    REFERENCE_TIME *pStartTime, 
    REFERENCE_TIME *pEndTime,
    DWORD dwFlags)	{

    CAutoLock cObjectLock(m_pLock);

    return CMemAllocator::GetBuffer(ppBuffer, pStartTime, pEndTime, dwFlags);    
}

HRESULT	CIPPAllocator_8u_C1 :: Alloc( void )	{

	CAutoLock lock(m_pLock);

	// set the pVih in the derived class
	ASSERT(pVih != NULL);

	//we call base class confirmation to proceed with allocation process
	HRESULT hr = CBaseAllocator::Alloc();
	if (FAILED(hr)) {
		DbgLog((LOG_TRACE,0,TEXT("_C1, CMEMAllocator::Alloc() hr=0x%x"), hr));
		return hr;
	}

	// if buffer isn't free by accident, free it
	if (m_pBuffer) {
		ReallyFree();
	}
	
	DbgLog((LOG_TRACE,0,TEXT("CIPPAllocatorOut_8u_C1::Alloc; allocation.....working")));
	DbgLog((LOG_TRACE,0,TEXT("%d, %d, %d"), size.width, size.height, stepInBytes));
	DbgLog((LOG_TRACE,0,TEXT("%d"), m_lSize));

	// prefix is disregarded...

	ASSERT(m_lAllocated == 0);
	// allocate each buffer individually by Intel IPP alloc methods
	CMediaSample *pSample;
	for (; m_lAllocated < m_lCount; m_lAllocated++) {

		m_pBuffer = ippiMalloc_8u_C1(size.width, size.height, &stepInBytes);

		DbgLog((LOG_TRACE,0,TEXT("Alloc. pointer = %d"), m_pBuffer));

		if (m_pBuffer == NULL) {
			return E_OUTOFMEMORY;
		}

		pSample = new CMediaSample(
			NAME("Default memory media sample"),
			this,
			&hr,
			m_pBuffer,      
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
	
	DbgLog((LOG_TRACE,0,TEXT("CIPPAllocatorOut_8u_C1::Alloc; allocation.....dunn %d"), m_lSize));

	m_bChanged = FALSE;


	hr = AllocAdditional();
	if (FAILED(hr)) {
		DbgLog((LOG_TRACE,0,TEXT("_C1, CMEMAllocator::AllocAdditional() hr=0x%x"), hr));
		return hr;
	}
	
	return NOERROR;
}



void CIPPAllocator_8u_C1 :: ReallyFree() {
	CAutoLock lock(m_pLock);

	DbgLog((LOG_TRACE,0,TEXT("really free")));
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

	// Extra memory deallocation
	HRESULT hr = FreeAdditional();
	ASSERT(SUCCEEDED(hr));

	m_lAllocated = 0;
}


HRESULT	CIPPAllocatorOut_8u_C1 :: AllocAdditional( void )	{

	CAutoLock lock(m_pLock);

	CThreshDiff* m_pFilter = (CThreshDiff*) this->m_pFilter;
	DbgLog((LOG_TRACE,0,TEXT("blaaa ")));

	m_pFilter->pAbsDiff_8u_C1 = ippiMalloc_8u_C1(m_pFilter->frameSize.width,
			m_pFilter->frameSize.width, &m_pFilter->step_8u_C1);
	
	return NOERROR;
}

HRESULT	CIPPAllocatorOut_8u_C1 :: FreeAdditional( void )	{

	CAutoLock lock(m_pLock);

	CThreshDiff* m_pFilt = (CThreshDiff*) m_pFilter;
	ippiFree(m_pFilt->pAbsDiff_8u_C1);

	return NOERROR;
}

HRESULT CIPPAllocatorOut_8u_C1 :: SetVIHFromPin()	{

	CAutoLock lock(m_pLock);

	pVih = (VIDEOINFOHEADER*) m_pPin->CurrentMediaType().Format();

	return NOERROR;
}


/*
HRESULT	CIPPAllocatorIn_8u_C1 :: AllocAdditional( void )	{

	return NOERROR;
}

HRESULT	CIPPAllocatorIn_8u_C1 :: FreeAdditional( void )	{

	return NOERROR;
}
*/