#ifndef ALLOCATOR_H
	#include "allocator.h"
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
	
	// allocation of chunk of memory for all the buffers by Intel IPP allocation methods
	m_pBuffer = ippiMalloc_8u_C1(size.width, size.height*m_lCount, &stepInBytes);

	if (m_pBuffer == NULL) {
		return E_OUTOFMEMORY;
	}

	DbgLog((LOG_TRACE,0,TEXT("CIPPAllocatorOut_8u_C1::Alloc; allocation.....dunn")));

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

	CFame2D* pFilt = (CFame2D*) this->m_pFilter;
	
	// FP initialization. CFame2D::StopStreaming is also appropriate for this task.
	// Or Inactive or sth.
	pFilt->fpDecideFAME = &CFame2D::FAME_initialization;

	DbgLog((LOG_TRACE,0,TEXT("additional allocs: %d %d %d"),
		pFilt->frameSize.width,pFilt->frameSize.height,pFilt->step_8u_C1 ));	

	// additional memory allocation - FAME
	pFilt->pBackground_8u_C1 = ippiMalloc_8u_C1(pFilt->frameSize.width,
			pFilt->frameSize.height, &pFilt->step_8u_C1);
	pFilt->pBackground_32f_C1 = ippiMalloc_32f_C1(pFilt->frameSize.width,
							pFilt->frameSize.height, &pFilt->step_32f_C1);
	pFilt->pStep =
		( float* )malloc( sizeof( float )*pFilt->frameSize.width*pFilt->frameSize.height );


	return NOERROR;
}

HRESULT	CIPPAllocatorOut_8u_C1 :: FreeAdditional( void )	{

	CAutoLock lock(m_pLock);

	CFame2D* pFilt = (CFame2D*) m_pFilter;
	ippiFree(pFilt->pBackground_8u_C1);
	ippiFree(pFilt->pBackground_32f_C1);
	free( pFilt->pStep );

	return NOERROR;
}

HRESULT CIPPAllocatorOut_8u_C1 :: SetVIHFromPin()	{

	CAutoLock lock(m_pLock);

	pVih = (VIDEOINFOHEADER*) m_pPin->CurrentMediaType().Format();

	return NOERROR;
}

HRESULT	CIPPAllocatorIn_8u_C1 :: AllocAdditional( void )	{

	return NOERROR;
}

HRESULT	CIPPAllocatorIn_8u_C1 :: FreeAdditional( void )	{

	return NOERROR;
}

HRESULT CIPPAllocatorIn_8u_C1 :: SetVIHFromPin()	{

	CAutoLock lock(m_pLock);

	pVih = (VIDEOINFOHEADER*) m_pPin->CurrentMediaType().Format();

	return NOERROR;
}

