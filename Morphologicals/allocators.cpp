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

	DbgLog((LOG_TRACE,0,TEXT("SetProperties, input alloc: %d %d %d %d"),
		pActual->cbBuffer,pActual->cBuffers,pActual->cbAlign,pActual->cbPrefix ));	

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
	 //If the requirements haven't changed then don't reallocate
    if (hr == S_FALSE) {
        ASSERT(m_pBuffer);
        return NOERROR;
    }
    ASSERT(hr == S_OK); // we use this fact in the loop below

	// free the old resources
	if (m_pBuffer) {
		ReallyFree();
	}
	
	// memory allocation by Intel IPP allocation methods
	// we want a single memory block m_pBuffer that will hold all needed buffers. the start of every buffer
	// has to be aligned at 32 bytes so IPP can work optimally. because DIB is aligned at 4, depending on its
	// resolution it might not be aligned at 32, causing IPP to perform suboptimally

	int DIBsize = m_pFilter->step_8u_C1 * m_pFilter->frameSize.height;
	int remainder = DIBsize % m_pFilter->IPPalignment;
	int padding;
	if (remainder != 0)
		padding = m_pFilter->IPPalignment - remainder;	// padding after one DIB's memory to aling next DIB's start at 32
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



void CIPPAllocator_8u_C1 :: ReallyFree() {

	CAutoLock lock(m_pLock);

	DbgLog((LOG_TRACE,0,TEXT("ReallyFree")));

	// Should never be unallocating unless all buffers are freed 
	ASSERT(m_lAllocated == m_lFree.GetCount());

	DbgLog((LOG_TRACE,0,TEXT("ReallyFree 2")));

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

	DbgLog((LOG_TRACE,0,TEXT("ReallyFree 3")));

	//deallocation with IPP
	if (m_pBuffer) {
		ippsFree(m_pBuffer);
		m_pBuffer = NULL;
	}

	DbgLog((LOG_TRACE,0,TEXT("ReallyFree 4")));

	// Extra memory deallocation
	HRESULT hr = FreeAdditional();
	ASSERT(SUCCEEDED(hr));

	DbgLog((LOG_TRACE,0,TEXT("ReallyFree 5")));

	m_lAllocated = 0;
}


HRESULT	CIPPAllocatorOut_8u_C1 :: AllocAdditional( void )	{

	CAutoLock lock(m_pLock);

	CMorphologicalsFilter* pFilt = (CMorphologicalsFilter*) m_pFilter;
	
	// IPL headers that will get attached to our buffers so they can be used by OpenCV
	if(!pFilt->pInputCV_C1)
	{
		pFilt->pInputCV_C1 = cvCreateImageHeader( pFilt->frameSizeCV, IPL_DEPTH_8U, 1 );
		pFilt->pInputCV_C1->origin = 1;
	}
	if(!pFilt->pOutputCV_C1)
	{
		pFilt->pOutputCV_C1 = cvCreateImageHeader( pFilt->frameSizeCV, IPL_DEPTH_8U, 1 );
		pFilt->pOutputCV_C1->origin = 1;
	}

	if(!pFilt->pHelperCV_C1)
	{
		pFilt->pHelperCV_C1 = cvCreateImageHeader( pFilt->frameSizeCV, IPL_DEPTH_8U, 1 );
		pFilt->pHelperCV_C1->origin = 1;
	}
	if(!pFilt->pHelper2CV_C1)
	{
		pFilt->pHelper2CV_C1 = cvCreateImageHeader( pFilt->frameSizeCV, IPL_DEPTH_8U, 1 );
		pFilt->pHelper2CV_C1->origin = 1;
	}

	if(!pFilt->pHelper_8u_C1)
	{
		pFilt->pHelper_8u_C1 = ippsMalloc_8u(m_pFilter->step_8u_C1 * pFilt->frameSize.height);
		cvSetData( pFilt->pHelperCV_C1, pFilt->pHelper_8u_C1, m_pFilter->step_8u_C1 );
	}
	if(!pFilt->pHelper2_8u_C1)
	{
		pFilt->pHelper_8u_C1 = ippsMalloc_8u(m_pFilter->step_8u_C1*pFilt->frameSize.height);
		cvSetData( pFilt->pHelper2CV_C1, pFilt->pHelper2_8u_C1, m_pFilter->step_8u_C1 );
	}
	//pFilt->pHelperCV_C1 = cvCreateImage(pFilt->frameSizeCV, IPL_DEPTH_8U, 1);
	//pFilt->pHelper_8u_C1 = (BYTE*)pFilt->pHelperCV_C1->imageData;

	DbgLog((LOG_TRACE,0,TEXT("%d %d"), pFilt->frameSizeCV.height, pFilt->frameSizeCV.width ));
	if(!pFilt->kernelCV)
	{
		pFilt->kernelCV = cvCreateStructuringElementEx( 3, 3, 1, 1, CV_SHAPE_RECT, NULL );
	}

	return NOERROR;
}

HRESULT	CIPPAllocatorOut_8u_C1 :: FreeAdditional( void )	{

	CAutoLock lock(m_pLock);

	CMorphologicalsFilter* pFilt = (CMorphologicalsFilter*) m_pFilter;
	if(pFilt->pInputCV_C1)
	{
		cvReleaseImageHeader( &pFilt->pInputCV_C1 );
		pFilt->pInputCV_C1 = NULL;
	}
	if(pFilt->pOutputCV_C1)
	{
		cvReleaseImageHeader( &pFilt->pOutputCV_C1 );
		pFilt->pOutputCV_C1 = NULL;
	}
	if(pFilt->pHelperCV_C1)
	{
		cvReleaseImageHeader( &pFilt->pHelperCV_C1 );
		pFilt->pHelperCV_C1 = NULL;
	}
	if(pFilt->pHelper2CV_C1)
	{
		cvReleaseImageHeader( &pFilt->pHelper2CV_C1 );
		pFilt->pHelper2CV_C1 = NULL;
	}
	if(pFilt->kernelCV)
	{
		cvReleaseStructuringElement( &pFilt->kernelCV );
		pFilt->kernelCV = NULL;
	}

	if(pFilt->pHelper_8u_C1)
	{
		ippsFree(pFilt->pHelper_8u_C1);
		pFilt->pHelper_8u_C1 = NULL;
	}
	if(pFilt->pHelper2_8u_C1)
	{
		ippsFree(pFilt->pHelper2_8u_C1);
		pFilt->pHelper2_8u_C1 = NULL;
	}

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