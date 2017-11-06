#ifndef ALLOCATORS_H
	#include "allocators.h"
#endif


STDMETHODIMP CIPPAllocator_8u_C3 :: SetProperties(ALLOCATOR_PROPERTIES* pRequest,
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

HRESULT CIPPAllocator_8u_C3::GetBuffer(
    IMediaSample **ppBuffer,
    REFERENCE_TIME *pStartTime, 
    REFERENCE_TIME *pEndTime,
    DWORD dwFlags)	{

    CAutoLock cObjectLock(m_pLock);

    return CMemAllocator::GetBuffer(ppBuffer, pStartTime, pEndTime, dwFlags);    
}

HRESULT	CIPPAllocator_8u_C3 :: Alloc( void )	{

	CAutoLock lock(m_pLock);

	// set the pVih in the derived class
	ASSERT(pVih != NULL);
	
	// Check he has called SetProperties
    HRESULT hr = CBaseAllocator::Alloc();
    if (FAILED(hr)) {
        return hr;
    }
	DbgLog((LOG_TRACE,0,TEXT("%b"), m_bChanged));
    //If the requirements haven't changed then don't reallocate
    if (hr == S_FALSE) {
        ASSERT(m_pBuffer);
        return NOERROR;
    }
    ASSERT(hr == S_OK); // we use this fact in the loop below

    // Free the old resources
    if (m_pBuffer) {
        ReallyFree();
    }

	// memory allocation by Intel IPP allocation methods
	// we want a single memory block m_pBuffer that will hold all needed buffers. the start of every buffer
	// has to be aligned at 32 bytes so IPP can work optimally. because DIB is aligned at 4, depending on its
	// resolution it might not be aligned at 32, causing IPP to perform suboptimally
	COverlay* pFilt = (COverlay*) m_pFilter;

	int DIBsize = pFilt->step_8u_C3 * pFilt->frameSize.height;
	int remainder = DIBsize % pFilt->DIBalignment;
	int padding;
	if (remainder != 0)
		padding = pFilt->DIBalignment - remainder;	// padding after one DIB's memory to aling next DIB's start at 32
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

	hr = AllocAdditional();
	if (FAILED(hr)) {
		return hr;
	}
	
	return NOERROR;
}



void CIPPAllocator_8u_C3 :: ReallyFree() {
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
		ippsFree(m_pBuffer);
		m_pBuffer = NULL;
	}

	// Extra memory deallocation
	HRESULT hr = FreeAdditional();
	ASSERT(SUCCEEDED(hr));

	DbgLog((LOG_TRACE,0,TEXT("really free out")));
	m_lAllocated = 0;
}


HRESULT	CIPPAllocatorOut_8u_C3 :: AllocAdditional( void )
{
	CAutoLock lock(m_pLock);

	DbgLog((LOG_TRACE,0,TEXT("allocadditional %d")));

	COverlay* m_pFilter = (COverlay*) this->m_pFilter;

	if(!m_pFilter->pOutputCV_C3)
	{
		m_pFilter->pOutputCV_C3 = cvCreateImageHeader( m_pFilter->frameSizeCV, IPL_DEPTH_8U, 3 );
		m_pFilter->pOutputCV_C3->origin = 1;
	}
	
	return NOERROR;
}

HRESULT	CIPPAllocatorOut_8u_C3 :: FreeAdditional( void )
{
	CAutoLock lock(m_pLock);

	DbgLog((LOG_TRACE,0,TEXT("freeadditional")));

	COverlay* m_pFilt = (COverlay*) m_pFilter;
	if(m_pFilt->pOutputCV_C3)
	{
		cvReleaseImageHeader( &m_pFilt->pOutputCV_C3 );
		m_pFilt->pOutputCV_C3 = NULL;
	}

	return NOERROR;
}

HRESULT CIPPAllocatorOut_8u_C3 :: SetVIHFromPin()	{

	CAutoLock lock(m_pLock);

	pVih = (VIDEOINFOHEADER*) m_pPin->CurrentMediaType().Format();

	return NOERROR;
}