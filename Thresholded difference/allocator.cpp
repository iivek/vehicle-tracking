#ifndef ALLOCATOR_H
	#include "allocator.h"
	#define ALLOCATOR_H
#endif

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
	
	if(pRequest->cBuffers>0)	{
		m_lCount=pRequest->cBuffers;
	}
	else	{
		m_lCount=1; // well it would be nice to have at least one
	}

	// everything as we requested in DecideBufferSize
	CThreshDiff* pFilt = (CThreshDiff*) m_pFilter;
	pActual->cbBuffer = pRequest->cbBuffer;
	pActual->cBuffers = pRequest->cBuffers;
	pActual->cbAlign = pRequest->cbAlign;
    pActual->cbPrefix = pRequest->cbPrefix;

	DbgLog((LOG_TRACE,0,TEXT("SetProperties, input alloc: %d %d %d %d"),m_lSize,m_lCount,m_lAlignment,m_lPrefix ));	
	DbgLog((LOG_TRACE,0,TEXT("SetProperties, input alloc: %d %d %d %d"),
		pActual->cbBuffer,pActual->cBuffers,pActual->cbAlign,pActual->cbPrefix ));	

    m_bChanged = TRUE;

	return NOERROR;
}


HRESULT	CIPPAllocator_8u_C1 :: Alloc( void )	{

	CAutoLock cAutolock(&lock);
	CThreshDiff* pFilt = (CThreshDiff*) m_pFilter;
	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) pFilt->m_pInput->CurrentMediaType().Format();

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
	
	DbgLog((LOG_TRACE,0,TEXT("CIPPAllocator_8u_C1::Alloc; allocation.....working")));
	
	// memory allocation by Intel IPP allocation methods
	int step = pVih->bmiHeader.biWidth*pFilt->bytesperpixel_8u_C1 + pFilt->stride_8u_C1;
	m_pBuffer = ippiMalloc_8u_C1(pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, &step);
	if (m_pBuffer == NULL) {
		return E_OUTOFMEMORY;
	}

	DbgLog((LOG_TRACE,0,TEXT("CIPPAllocator_8u_C1::Alloc; allocation.....dunn")));

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


HRESULT	CIPPAllocatorOut_8u_C1 :: Alloc( void )	{

	CAutoLock cAutolock(&lock);
	CThreshDiff* pFilt = (CThreshDiff*) m_pFilter;
	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) pFilt->m_pInput->CurrentMediaType().Format();

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
	
	// memory allocation by Intel IPP allocation methods
	int step = pVih->bmiHeader.biWidth*pFilt->bytesperpixel_8u_C1 + pFilt->stride_8u_C1;
	m_pBuffer = ippiMalloc_8u_C1(pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, &step);
	if (m_pBuffer == NULL) {
		return E_OUTOFMEMORY;
	}

	DbgLog((LOG_TRACE,0,TEXT("CIPPAllocatorOut_8u_C1::Alloc; allocation.....dunn")));

	// additional memory allocation - FAME
	pFilt->pBackground_8u_C1 = ippiMalloc_8u_C1(pVih->bmiHeader.biWidth,
			pVih->bmiHeader.biHeight, &pFilt->step_8u_C1);
	pFilt->pBackground_32f_C1 = ippiMalloc_32f_C1(pVih->bmiHeader.biWidth,
							pVih->bmiHeader.biHeight, &pFilt->step_32f_C1);
	pFilt->pStep =
		( float* )malloc( sizeof( float )*pVih->bmiHeader.biWidth*pVih->bmiHeader.biHeight );

	DbgLog((LOG_TRACE,0,TEXT("CIPPAllocatorOut_8u_C1::Alloc; Adicijonalna alokacija")));


	CMediaSample *pSample;
	//number of allocated buffers
	ASSERT(m_lAllocated == 0);

	// FP initialization. CThreshDiff::StopStreaming is also appropriate for this task
	CThreshDiff::fpDecideFAME = CThreshDiff::FAME_initialization;

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


void CIPPAllocatorOut_8u_C1 :: ReallyFree() 
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

	// FAME
	CThreshDiff* pFilt = (CThreshDiff*) m_pFilter;
	ippiFree(pFilt->pBackground_8u_C1);
	ippiFree(pFilt->pBackground_32f_C1);
	free( pFilt->pStep );

	m_lAllocated = 0;
}