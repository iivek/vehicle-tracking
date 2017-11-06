#ifndef ALLOCATORS_H
	#include "allocators.h"
#endif


STDMETHODIMP CListAllocatorOut :: SetProperties(ALLOCATOR_PROPERTIES* pRequest,
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

	DbgLog((LOG_TRACE,0,TEXT("SetProperties, output ListAlloc: %d %d %d %d"),
		pActual->cbBuffer,pActual->cBuffers,pActual->cbAlign,pActual->cbPrefix ));	

    m_bChanged = TRUE;

	return NOERROR;
}


HRESULT	CListAllocatorOut :: Alloc( void )	{

	CAutoLock lock(m_pLock);

	DbgLog((LOG_TRACE,0,TEXT("CListAllocatorOut :: Alloc( void )")));

	// set the pVih in the derived class
	ASSERT(pVih != NULL);

	HRESULT hr;

	// we call base class confirmation to proceed with allocation process
	hr = CBaseAllocator::Alloc();
	if (FAILED(hr)) {
		DbgLog((LOG_TRACE,0,TEXT("CListAllocatorOut, CMEMAllocator::Alloc() hr=0x%x"), hr));
		return hr;
	}
	 if (hr == S_FALSE) {
        return NOERROR;
    }

	// if buffer isn't free by accident, free it
	if (m_pBuffer) {
		ReallyFree();
	}
	ASSERT(m_lAllocated == 0);

	m_pOutputArray = new CMovingObject[ m_lSize*m_lCount ];
	if (m_pOutputArray == NULL)	return E_OUTOFMEMORY;
	m_pBuffer = (LPBYTE) m_pOutputArray;	// just to assign it something


	DbgLog((LOG_TRACE,0,TEXT("ALLOCATING FOR REAL!")));
	CMediaSample *pSample;
	for (CMovingObject* pCurrentInBuffer = m_pOutputArray;
		m_lAllocated < m_lCount;
		m_lAllocated++, pCurrentInBuffer += m_lSize) {

		pSample = new CMediaSample(
			NAME("Default memory media sample"),
			this,
			&hr,
			(LPBYTE) pCurrentInBuffer,
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
	
	if (FAILED(hr)) {
		return hr;
	}

	DbgLog((LOG_TRACE,0,TEXT("CListAllocatorOut :: Alloc( void )")));

	return NOERROR;
}

void CListAllocatorOut :: ReallyFree() {
	CAutoLock lock(m_pLock);

	DbgLog((LOG_TRACE,0,TEXT("free")));

	// Should never be deallocating unless all buffers are freed 
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

	// delete the stuff
	delete []m_pOutputArray;
	m_pOutputArray = NULL;
	m_pBuffer = NULL;

	m_lAllocated = 0;

	// Extra memory deallocation
	HRESULT hr = FreeAdditional();
	ASSERT(SUCCEEDED(hr));

}


HRESULT	CListAllocatorOut :: AllocAdditional( void )	{

	CAutoLock lock(m_pLock);

	CTrackerMkII* pFilt = (CTrackerMkII*) m_pFilter;

	DbgLog((LOG_TRACE,0,TEXT("allocadditional %d")));

	// reset ID and reinitialize all object lists. perhaps better done in flushing. 
	pFilt->uniqueID = 0;
	pFilt->objectsOld.clear();
	pFilt->objectsNew.clear();
	pFilt->objectsWithFit.clear();
	pFilt->objectsSuccessors.clear();

	if(!pFilt->pHelper_8u_C1)
	pFilt->pHelper_8u_C1 = ippsMalloc_8u(pFilt->frameSize.height*pFilt->step_8u_C1);

	if(!pFilt->pContours_8u_C1)
	pFilt->pContours_8u_C1 = ippsMalloc_8u(pFilt->frameSize.height*pFilt->step_8u_C1);

	if(!pFilt->pMask_8u_C1)
	pFilt->pMask_8u_C1 = ippsMalloc_8u(pFilt->frameSize.height*pFilt->step_8u_C1);

	if(!pFilt->pTemporaryBuffer_8u_C1)
	pFilt->pTemporaryBuffer_8u_C1 = ippsMalloc_8u(pFilt->frameSize.height*pFilt->step_8u_C1);

	// IPL headers that will get attached to our buffers so they can be used by OpenCV
	if(!pFilt->pLumaCV_C1)
	{
		pFilt->pLumaCV_C1 = cvCreateImageHeader( pFilt->frameSizeCV, IPL_DEPTH_8U, 1 );
		pFilt->pLumaCV_C1->origin = 1;
	}
	if(!pFilt->pColorCV_AC4)
	{
		pFilt->pColorCV_AC4 = cvCreateImageHeader( pFilt->frameSizeCV, IPL_DEPTH_8U, 1 );
		pFilt->pColorCV_AC4->origin = 1;
	}
	if(!pFilt->pHelperCV_C1)
	{
		pFilt->pHelperCV_C1 = cvCreateImageHeader( pFilt->frameSizeCV, IPL_DEPTH_8U, 1 );
		pFilt->pHelperCV_C1->origin = 1;
	}
	if(!pFilt->pContoursCV_C1)
	{
		pFilt->pContoursCV_C1 = cvCreateImageHeader( pFilt->frameSizeCV, IPL_DEPTH_8U, 1 );
		pFilt->pContoursCV_C1->origin = 1;
	}
	if(!pFilt->pMaskCV_C1)
	{
		pFilt->pMaskCV_C1 = cvCreateImageHeader( pFilt->frameSizeCV, IPL_DEPTH_8U, 1 );
		pFilt->pMaskCV_C1->origin = 1;
	}
	if(!pFilt->pTemporaryBufferCV_C1)
	{
		pFilt->pTemporaryBufferCV_C1 = cvCreateImageHeader( pFilt->frameSizeCV, IPL_DEPTH_8U, 1 );
		pFilt->pTemporaryBufferCV_C1->origin = 1;
	}

	// set headers
	cvSetData( pFilt->pHelperCV_C1, pFilt->pHelper_8u_C1, pFilt->step_8u_C1 );
	cvSetData( pFilt->pContoursCV_C1, pFilt->pContours_8u_C1, pFilt->step_8u_C1 );
	cvSetData( pFilt->pTemporaryBufferCV_C1, pFilt->pTemporaryBuffer_8u_C1, pFilt->step_8u_C1 );
	cvSetData( pFilt->pMaskCV_C1, pFilt->pMask_8u_C1, pFilt->step_8u_C1 );

	if(!pFilt->contourStorageNew)
		pFilt->contourStorageNew = cvCreateMemStorage(0);
	if(!pFilt->contourStorageOld)
		pFilt->contourStorageOld = cvCreateMemStorage(0);

	// more CV stuff - data type for Kalman filter measurements
	if(!pFilt->measurement)
	{
		pFilt->measurement = cvCreateMatHeader( 1, 1, CV_32FC1 );
		cvSetData( pFilt->measurement, pFilt->measurementD, 1*sizeof(float) );
	}

	return NOERROR;
}

HRESULT	CListAllocatorOut :: FreeAdditional( void )	{

	CAutoLock lock(m_pLock);

	DbgLog((LOG_TRACE,0,TEXT("freeadditional")));

	CTrackerMkII* pFilt = (CTrackerMkII*) m_pFilter;
	if(pFilt->measurement)
	{
		cvReleaseMatHeader( &pFilt->measurement );
		pFilt->measurement = NULL;
	}
	if(pFilt->pLumaCV_C1)
	{
		cvReleaseImageHeader( &pFilt->pLumaCV_C1 );
		pFilt->pLumaCV_C1 = NULL;
	}
	if(pFilt->pHelperCV_C1)
	{
		cvReleaseImageHeader( &pFilt->pHelperCV_C1 );
		pFilt->pHelperCV_C1 = NULL;
	}
	if(pFilt->pContoursCV_C1)
	{
		cvReleaseImageHeader( &pFilt->pContoursCV_C1 );
		pFilt->pContoursCV_C1 = NULL;
	}
	if(pFilt->pMaskCV_C1)
	{
		cvReleaseImageHeader( &pFilt->pMaskCV_C1 );
		pFilt->pMaskCV_C1 = NULL;
	}
	if(pFilt->pTemporaryBufferCV_C1)
	{
		cvReleaseImageHeader( &pFilt->pTemporaryBufferCV_C1 );
		pFilt->pTemporaryBufferCV_C1 = NULL;
	}

	if(pFilt->contourStorageNew)
	{
		cvClearMemStorage( pFilt->contourStorageNew );
		pFilt->contourStorageNew = NULL;
	}
	if(pFilt->contourStorageOld)
	{
		cvClearMemStorage( pFilt->contourStorageOld );
		pFilt->contourStorageOld = NULL;
	}

	if(pFilt->pHelper_8u_C1)
	{
		ippsFree(pFilt->pHelper_8u_C1);
		pFilt->pHelper_8u_C1 = NULL;
	}
	if(!pFilt->pContours_8u_C1)
	{
		ippsFree(pFilt->pHelper_8u_C1);
		pFilt->pHelper_8u_C1 = NULL;
	}
	if(!pFilt->pMask_8u_C1)
	{
		ippsFree(pFilt->pMask_8u_C1);
		pFilt->pMask_8u_C1 = NULL;
	}
	if(!pFilt->pTemporaryBuffer_8u_C1)
	{
		ippsFree(pFilt->pTemporaryBuffer_8u_C1);
		pFilt->pTemporaryBuffer_8u_C1 = NULL;
	}
	

	return NOERROR;
}

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
	CTrackerMkII* pFilt = (CTrackerMkII*) m_pFilter;

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

	CTrackerMkII* pFilt = (CTrackerMkII*) m_pFilter;

	DbgLog((LOG_TRACE,0,TEXT("allocadditional %d")));

	// reset ID and reinitialize all object lists. perhaps better done in flushing. 
	pFilt->uniqueID = 0;
	pFilt->objectsOld.clear();
	pFilt->objectsNew.clear();
	pFilt->objectsWithFit.clear();
	pFilt->objectsSuccessors.clear();

	if(!pFilt->pHelper_8u_C1)
	pFilt->pHelper_8u_C1 = ippsMalloc_8u(pFilt->frameSize.height*pFilt->step_8u_C1);

	if(!pFilt->pContours_8u_C1)
	pFilt->pContours_8u_C1 = ippsMalloc_8u(pFilt->frameSize.height*pFilt->step_8u_C1);

	if(!pFilt->pTemporaryBuffer_8u_C1)
	pFilt->pTemporaryBuffer_8u_C1 = ippsMalloc_8u(pFilt->frameSize.height*pFilt->step_8u_C1);

	if(!pFilt->pMask_8u_C1)
	pFilt->pMask_8u_C1 = ippsMalloc_8u(pFilt->frameSize.height*pFilt->step_8u_C1);

	// IPL headers that will get attached to our buffers so they can be used by OpenCV
	if(!pFilt->pLumaCV_C1)
	{
		pFilt->pLumaCV_C1 = cvCreateImageHeader( pFilt->frameSizeCV, IPL_DEPTH_8U, 1 );
		pFilt->pLumaCV_C1->origin = 1;
	}
	if(!pFilt->pColorCV_AC4)
	{
		pFilt->pColorCV_AC4 = cvCreateImageHeader( pFilt->frameSizeCV, IPL_DEPTH_8U, 1 );
		pFilt->pColorCV_AC4->origin = 1;
	}
	if(!pFilt->pHelperCV_C1)
	{
		pFilt->pHelperCV_C1 = cvCreateImageHeader( pFilt->frameSizeCV, IPL_DEPTH_8U, 1 );
		pFilt->pHelperCV_C1->origin = 1;
	}
	if(!pFilt->pContoursCV_C1)
	{
		pFilt->pContoursCV_C1 = cvCreateImageHeader( pFilt->frameSizeCV, IPL_DEPTH_8U, 1 );
		pFilt->pContoursCV_C1->origin = 1;
	}
	if(!pFilt->pMaskCV_C1)
	{
		pFilt->pMaskCV_C1 = cvCreateImageHeader( pFilt->frameSizeCV, IPL_DEPTH_8U, 1 );
		pFilt->pMaskCV_C1->origin = 1;
	}
	if(!pFilt->pTemporaryBufferCV_C1)
	{
		pFilt->pTemporaryBufferCV_C1 = cvCreateImageHeader( pFilt->frameSizeCV, IPL_DEPTH_8U, 1 );
		pFilt->pTemporaryBufferCV_C1->origin = 1;
	}

	// set headers
	cvSetData( pFilt->pHelperCV_C1, pFilt->pHelper_8u_C1, pFilt->step_8u_C1 );
	cvSetData( pFilt->pContoursCV_C1, pFilt->pContours_8u_C1, pFilt->step_8u_C1 );
	cvSetData( pFilt->pMaskCV_C1, pFilt->pMask_8u_C1, pFilt->step_8u_C1 );
	cvSetData( pFilt->pTemporaryBufferCV_C1, pFilt->pTemporaryBuffer_8u_C1, pFilt->step_8u_C1 );
	
	if(!pFilt->contourStorageNew)
		pFilt->contourStorageNew = cvCreateMemStorage(0);
	if(!pFilt->contourStorageOld)
		pFilt->contourStorageOld = cvCreateMemStorage(0);

	// more CV stuff - data type for Kalman filter measurements
	if(!pFilt->measurement)
	{
		pFilt->measurement = cvCreateMatHeader( 1, 1, CV_32FC1 );
		cvSetData( pFilt->measurement, pFilt->measurementD, 1*sizeof(float) );
	}

	return NOERROR;
}

HRESULT	CIPPAllocatorOut_8u_C3 :: FreeAdditional( void )
{
	CAutoLock lock(m_pLock);

	DbgLog((LOG_TRACE,0,TEXT("freeadditional")));

	CTrackerMkII* pFilt = (CTrackerMkII*) m_pFilter;
	if(pFilt->measurement)
	{
		cvReleaseMatHeader( &pFilt->measurement );
		pFilt->measurement = NULL;
	}
	if(pFilt->pLumaCV_C1)
	{
		cvReleaseImageHeader( &pFilt->pLumaCV_C1 );
		pFilt->pLumaCV_C1 = NULL;
	}
	if(pFilt->pHelperCV_C1)
	{
		cvReleaseImageHeader( &pFilt->pHelperCV_C1 );
		pFilt->pHelperCV_C1 = NULL;
	}
	if(pFilt->pContoursCV_C1)
	{
		cvReleaseImageHeader( &pFilt->pContoursCV_C1 );
		pFilt->pContoursCV_C1 = NULL;
	}
	if(pFilt->pMaskCV_C1)
	{
		cvReleaseImageHeader( &pFilt->pMaskCV_C1 );
		pFilt->pMaskCV_C1 = NULL;
	}
	if(pFilt->pTemporaryBufferCV_C1)
	{
		cvReleaseImageHeader( &pFilt->pTemporaryBufferCV_C1 );
		pFilt->pTemporaryBufferCV_C1 = NULL;
	}

	if(pFilt->contourStorageNew)
	{
		cvClearMemStorage( pFilt->contourStorageNew );
		pFilt->contourStorageNew = NULL;
	}
	if(pFilt->contourStorageOld)
	{
		cvClearMemStorage( pFilt->contourStorageOld );
		pFilt->contourStorageOld = NULL;
	}

	if(pFilt->pHelper_8u_C1)
	{
		ippsFree(pFilt->pHelper_8u_C1);
		pFilt->pHelper_8u_C1 = NULL;
	}
	if(!pFilt->pContours_8u_C1)
	{
		ippsFree(pFilt->pHelper_8u_C1);
		pFilt->pHelper_8u_C1 = NULL;
	}
	if(!pFilt->pMask_8u_C1)
	{
		ippsFree(pFilt->pMask_8u_C1);
		pFilt->pMask_8u_C1 = NULL;
	}
	if(!pFilt->pTemporaryBuffer_8u_C1)
	{
		ippsFree(pFilt->pTemporaryBuffer_8u_C1);
		pFilt->pTemporaryBuffer_8u_C1 = NULL;
	}
	

	return NOERROR;
}

HRESULT CIPPAllocatorOut_8u_C3 :: SetVIHFromPin()	{

	CAutoLock lock(m_pLock);

	pVih = (VIDEOINFOHEADER*) m_pPin->CurrentMediaType().Format();

	return NOERROR;
}

