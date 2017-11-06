#ifndef ALLOCATOR_H
	#include "allocator.h"
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
	CDupY* pFilt = (CDupY*) m_pFilter;
	pActual->cbBuffer = pRequest->cbBuffer;
	pActual->cBuffers = pRequest->cBuffers;
	pActual->cbAlign = pRequest->cbAlign;
    pActual->cbPrefix = pRequest->cbPrefix;

	DbgLog((LOG_TRACE,0,TEXT("%d %d %d %d"),m_lSize,m_lCount,m_lAlignment,m_lPrefix ));	

    m_bChanged = TRUE;

	return NOERROR;
}


HRESULT	CIPPAllocator_8u_C1 :: Alloc( void )	{

	CAutoLock cAutolock(&lock);
	CDupY* pFilt = (CDupY*) m_pFilter;

	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) pFilt->m_pInput->CurrentMediaType().Format();

	//we call base class confirmation to proceed with allocation process

	HRESULT hr = CBaseAllocator::Alloc();
	if (FAILED(hr)) {
		DbgLog((LOG_TRACE,0,TEXT("_C1, CMEMAllocator::Alloc() hr=0x%x"), hr));
		return hr;
	}
	if (hr == S_FALSE) {
        return NOERROR;
    }

	// if buffer isn't free by accident, free it
	if (m_pBuffer) {
		ReallyFree();
	}

	// memory allocation by Intel IPP allocation methods
	// we want a single memory block m_pBuffer that will hold all needed buffers. the start of every buffer
	// has to be aligned at 32 bytes so IPP can work optimally. because DIB is aligned at 4, depending on its
	// resolution it might not be aligned at 32, causing IPP to perform suboptimally

	int DIBsize = pFilt->step_8u_C1 * pVih->bmiHeader.biHeight;
	int remainder = DIBsize % pFilt->IPPalignment;
	int padding;
	if (remainder != 0)
		padding = pFilt->IPPalignment - remainder;	// padding after one DIB's memory to aling next DIB's start at 32
	else
		padding = 0;

	m_pBuffer = ippsMalloc_8u(DIBsize*m_lCount + padding*(m_lCount-1));

	// prefix is disregarded...

	ASSERT(m_lAllocated == 0);
	// allocate each buffer individually by Intel IPP alloc methods
	CMediaSample *pSample;
	// sample creation with m_lSize including prefix
	// notice that when we call GetPointer we get size of m_lSize without m_lPrefix
	for (LPBYTE pCurrentInBuffer = m_pBuffer;
		m_lAllocated < m_lCount;
		m_lAllocated++, pCurrentInBuffer += DIBsize + padding) {

		pSample = new CMediaSample(
			NAME("Default memory media sample"),
			this,
			&hr,
			pCurrentInBuffer,
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
	DbgLog((LOG_TRACE,1,TEXT("ReallyFree CIPPAllocator_8u_C1 called")));

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
		ippsFree(m_pBuffer);
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
	
	if(pRequest->cBuffers>0)	{
		m_lCount=pRequest->cBuffers;
	}
	else	{
		m_lCount=1; // well it would be nice
	}

	CDupY* pFilt = (CDupY*) m_pFilter;

	// everything as we requested in DecideBufferSize, we can do it
	pActual->cbBuffer = pRequest->cbBuffer;
	pActual->cBuffers = pRequest->cBuffers;
	pActual->cbAlign = pRequest->cbAlign;
    pActual->cbPrefix = pRequest->cbPrefix;

	DbgLog((LOG_TRACE,0,TEXT("%d %d %d %d"),m_lSize,m_lCount,m_lAlignment,m_lPrefix ));	

    m_bChanged = TRUE;

	return NOERROR;
}


HRESULT	CIPPAllocator_8u_C3 :: Alloc( void )	{

	DbgLog((LOG_TRACE,0,TEXT("CIPPAllocator_8u_C3, alloc")));
	DbgLog((LOG_TRACE,0,TEXT("%d %d %d %d"),m_lSize,m_lCount,m_lAlignment,m_lPrefix ));	

	CAutoLock cAutolock(&lock);
	CDupY* pFilt = (CDupY*) m_pFilter;

	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) pFilt->m_pInput->CurrentMediaType().Format();

	//we call base class confirmation to proceed with allocation process
	HRESULT hr = CBaseAllocator::Alloc();
	if (FAILED(hr)) {
		DbgLog((LOG_TRACE,0,TEXT("_C1, CMEMAllocator::Alloc() hr=0x%x"), hr));
		return hr;
	}
	if (hr == S_FALSE) {
        return NOERROR;
    }

	// if buffer isn't free by accident, free it
	if (m_pBuffer) {
		ReallyFree();
	}
		
	// memory allocation by Intel IPP allocation methods
	// we want a single memory block m_pBuffer that will hold all needed buffers. the start of every buffer
	// has to be aligned at 32 bytes so IPP can work optimally. because DIB is aligned at 4, depending on its
	// resolution it might not be aligned at 32, causing IPP to perform suboptimally

	int DIBsize = pFilt->step_8u_C3 * pVih->bmiHeader.biHeight;
	int remainder = DIBsize % pFilt->IPPalignment;
	int padding;
	if (remainder != 0)
		padding = pFilt->IPPalignment - remainder;	// padding after one DIB's memory to aling next DIB's start at 32
	else
		padding = 0;

	m_pBuffer = ippsMalloc_8u(DIBsize*m_lCount + padding*(m_lCount-1));

	// prefix is disregarded...

	ASSERT(m_lAllocated == 0);
	// allocate each buffer individually by Intel IPP alloc methods
	CMediaSample *pSample;
	// sample creation with m_lSize including prefix
	// notice that when we call GetPointer we get size of m_lSize without m_lPrefix
	for (LPBYTE pCurrentInBuffer = m_pBuffer;
		m_lAllocated < m_lCount;
		m_lAllocated++, pCurrentInBuffer += DIBsize + padding) {

		pSample = new CMediaSample(
			NAME("Default memory media sample"),
			this,
			&hr,
			pCurrentInBuffer,
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
	DbgLog((LOG_TRACE,1,TEXT("ReallyFree CIPPAllocator_8u_C3 called")));

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
		ippsFree(m_pBuffer);
		m_pBuffer = NULL;
	}

	m_lAllocated = 0;
}

