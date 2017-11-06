#include "cxcore.h"
#include "highgui.h"


#ifndef TRACKER_H
	#include "tracker.h"
#endif

#ifndef PINS_H
	#include "pins.h"
#endif

#ifndef MATH_H
	#include <math.h>
	#define MATH_H
#endif


HRESULT CTrackerFilter :: InitializeProcessParams(VIDEOINFOHEADER* pVih)	{
	// these will be returned by ippimalloc, but we cheat and want them here...

	// our internal monochrome datatype is aligned to 32 bits, so we have to calculate strides and everything
	bytesPerPixel_8u_C1 = 1;
	int remainder = (pVih->bmiHeader.biWidth*bytesPerPixel_8u_C1)%DIBalignment;
	if (remainder != 0)
		stride_8u_C1 = DIBalignment - remainder;
	else
		stride_8u_C1 = 0;
	step_8u_C1 = pVih->bmiHeader.biWidth*bytesPerPixel_8u_C1+stride_8u_C1;

	frameSize.height = pVih->bmiHeader.biHeight;
	frameSize.width = pVih->bmiHeader.biWidth;
	frameSizeCV = cvSize(pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight);	//it would perhaps be better if we used pointers to the same integers here

	// Kalman initial estimates, one Kalman for each cordinate
	//	
	// states:			{	x,		position
	//						v,		velocity
	//						a	}	acceleration
	//
	// measurements:	{	x	}	position
	
	float _TransitionMatrixD[4]	=	{	1,		1,
										0,		1
	};
	float _MeasurementMatrixD[2]	=	{	1,	0
	};
	float _ProcessNoise_covD[4]	=	{	100,	0,
										0,		100
	};
	float _ErrorPost_covD[4]		=	{	10000,	0,
											0,		10000
	};
	float _MeasurementNoise_covD[1]	=	{	2500
	};
	float _State_PostD[2]	=	{	0,		// location
									0		// velocity
	};

	m_params.initialKalman.TransitionMatrix		= cvCreateMatHeader( 2, 2, CV_32FC1 );
	m_params.initialKalman.MeasurementMatrix	= cvCreateMatHeader( 1, 2, CV_32FC1 );
	m_params.initialKalman.ProcessNoise_cov		= cvCreateMatHeader( 2, 2, CV_32FC1 );
	m_params.initialKalman.ErrorPost_cov		= cvCreateMatHeader( 2, 2, CV_32FC1 );
	m_params.initialKalman.MeasurementNoise_cov	= cvCreateMatHeader( 1, 1, CV_32FC1 );
	m_params.initialKalman.State_post			= cvCreateMatHeader( 2, 1, CV_32FC1 );

	memcpy( m_params.initialKalman.TransitionMatrixD,
		_TransitionMatrixD,		4*sizeof( float ) );
	memcpy( m_params.initialKalman.MeasurementMatrixD,
		_MeasurementMatrixD,	2*sizeof( float ) );
	memcpy( m_params.initialKalman.ProcessNoise_covD,
		_ProcessNoise_covD,		4*sizeof( float ) );
	memcpy( m_params.initialKalman.ErrorPost_covD,
		_ErrorPost_covD,		4*sizeof( float ) );
	memcpy( m_params.initialKalman.MeasurementNoise_covD,
		_MeasurementNoise_covD,	1*sizeof( float ) );
	memcpy( m_params.initialKalman.State_postD,
		_State_PostD,	2*sizeof( float ) );

	cvSetData( m_params.initialKalman.TransitionMatrix,
		m_params.initialKalman.TransitionMatrixD, 2*sizeof(float) );
	cvSetData( m_params.initialKalman.MeasurementMatrix,
		m_params.initialKalman.MeasurementMatrixD, 2*sizeof(float) );
	cvSetData( m_params.initialKalman.ProcessNoise_cov,
		m_params.initialKalman.ProcessNoise_covD, 2*sizeof(float) );
	cvSetData( m_params.initialKalman.ErrorPost_cov,
		m_params.initialKalman.ErrorPost_covD, 2*sizeof(float) );
	cvSetData( m_params.initialKalman.MeasurementNoise_cov,
		m_params.initialKalman.MeasurementNoise_covD, 1*sizeof(float) );
	cvSetData( m_params.initialKalman.State_post,
		m_params.initialKalman.State_postD, 1*sizeof(float) );

	// other parameters, to be set via property pages
	m_params.timeToLive = 0;
	m_params.minimalSize.height = 15;
	m_params.minimalSize.width = 15;
	m_params.bufferSize = 50;

	return NOERROR;
}

HRESULT CTrackerFilter :: CheckInputType(const CMediaType *mtIn)
{	
	CheckPointer(mtIn,E_POINTER);

	DbgLog((LOG_TRACE,0,TEXT("CheckInputType")));

	if ((mtIn->majortype != MEDIATYPE_Video) ||
		(mtIn->subtype != MEDIASUBTYPE_8BitMonochrome_internal) ||
		(mtIn->formattype != FORMAT_VideoInfo) ||		// is this really necessary?
		(mtIn->cbFormat < sizeof(VIDEOINFOHEADER)))		// do all CMediaTypes have a VIH format?
	{
		DbgLog((LOG_TRACE,0,TEXT("Probljem1")));
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	// reinterpret cast is safe because we already checked formattype and cbFormat
	VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(mtIn->pbFormat);

	InitializeProcessParams(pVih);

	DbgLog((LOG_TRACE,0,TEXT("PROSHLO!")));
	return S_OK;
}


HRESULT CTrackerFilter :: GetMediaType(int iPosition, CMediaType *pOutputMediaType)	{

	DbgLog((LOG_TRACE,0,TEXT("GetMediaType")));

    // This should never happen
    if (iPosition < 0) {
        return E_INVALIDARG;
    }

    // Do we have more items to offer
    if (iPosition > 0) {
		DbgLog((LOG_TRACE,0,TEXT("No more items")));
        return VFW_S_NO_MORE_ITEMS;
    }

	HRESULT hr = m_pInput->ConnectionMediaType(pOutputMediaType);
	if (FAILED(hr))		{
		return hr;
	}
    CheckPointer(pOutputMediaType,E_POINTER);
	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) pOutputMediaType->Format();

	// with same DIB width and height but only one (monochrome) channel
	pVih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pVih->bmiHeader.biPlanes = 1;
	pVih->bmiHeader.biBitCount = 8;
	pVih->bmiHeader.biCompression = BI_RGB;	// ?? no alternative, we treat it as uncompressed RGB
	pVih->bmiHeader.biSizeImage = DIBSIZE(pVih->bmiHeader);	
	pVih->bmiHeader.biClrImportant = 0;
	//SetRectEmpty(&(pVih->rcSource));
	//SetRectEmpty(&(pVih->rcTarget));

	// preferred output media subtype is MEDIASUBTYPE_CMovingObjects...
	// Our output MediaType is defined as follows: (irrelevant)
	pOutputMediaType->SetType(outputMediaType->Type());
	pOutputMediaType->SetSubtype(outputMediaType->Subtype());
	pOutputMediaType->SetFormatType(&outputMediaType->formattype);
	pOutputMediaType->bFixedSizeSamples = outputMediaType->bFixedSizeSamples;
	pOutputMediaType->SetTemporalCompression(outputMediaType->bTemporalCompression);
	pOutputMediaType->lSampleSize = DIBSIZE(pVih->bmiHeader);
	pOutputMediaType->cbFormat = sizeof(VIDEOINFOHEADER);

	// a good time to fill the rest of outputMediaType
	outputMediaType->SetSampleSize( DIBSIZE(pVih->bmiHeader) );

	DbgLog((LOG_TRACE,0,TEXT("yes more items")));

    return NOERROR;
}


CBasePin* CTrackerFilter :: GetPin( int n )	{

/*	HRESULT hr;

	//DbgLog((LOG_TRACE,0,TEXT("GetPin")));

	switch ( n )	{
		case 0:
			// Create an input pin if necessary
			if ( m_pInput == NULL )	{
				m_pInput = new CPinInput( this, &hr );      
				if ( m_pInput == NULL )	{
					return NULL;
				}
				DbgLog((LOG_TRACE,0,TEXT("InputPin created")));
				return m_pInput;
			}
			else	{
				return m_pInput;
			}
			break;
		case 1:
			// Create an output pin if necessary
			if ( m_pOutput == NULL )	{
				m_pOutput = new CPinOutput( this, &hr );      
				if ( m_pOutput == NULL )	{
					return NULL;
				}
				DbgLog((LOG_TRACE,0,TEXT("OutputPin created")));
				return m_pOutput;
			}
			else	{
				return m_pOutput;
			}
			break;
		default:
			return NULL;
      }
	  */
		// we need to have input pin and output pin at the same time, so this upper commented stuff
	// might not work. here's the way it's done in CTransformFilter::GetPin(int)
	HRESULT hr = S_OK;

    // Create an input pin if necessary

    if (m_pInput == NULL) {

        m_pInput = new CPinInput( this, &hr );     

        //  Can't fail
        ASSERT(SUCCEEDED(hr));
        if (m_pInput == NULL) {
            return NULL;
        }
		m_pOutput = new CPinOutput( this, &hr );

        // Can't fail
        ASSERT(SUCCEEDED(hr));
        if (m_pOutput == NULL) {
            delete m_pInput;
            m_pInput = NULL;
        }
		
    }

    // Return the appropriate pin

    if (n == 0) {
        return m_pInput;
    } else
    if (n == 1) {
        return m_pOutput;
    } else {
        return NULL;
    }
}

// output ALLOCATOR_PROPERTIES will also be set here, depending on input DIB format and 8 bit
// monochrome submediaformat we want to force as output
HRESULT CTrackerFilter :: CheckTransform( const CMediaType *mtIn, const CMediaType *mtOut )	{

	// our terms of connection: input has to be a valid and DIBs have to be of same dimensions

    CheckPointer(mtIn,E_POINTER);
    CheckPointer(mtOut,E_POINTER);

	DbgLog((LOG_TRACE,0,TEXT("CheckTransform")));

	VIDEOINFOHEADER* pVihIn = (VIDEOINFOHEADER*) mtIn->Format();
	VIDEOINFOHEADER* pVihOut = (VIDEOINFOHEADER*) mtOut->Format();
	if(pVihIn == NULL)	{
		DbgLog((LOG_TRACE,0,TEXT("Probljem2")));
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	if(mtIn->subtype != MEDIASUBTYPE_8BitMonochrome_internal)	{
		DbgLog((LOG_TRACE,0,TEXT("Probljem3")));
		return VFW_E_TYPE_NOT_ACCEPTED;
	}
	if(mtOut->subtype != MEDIASUBTYPE_CMovingObjects)	{
		DbgLog((LOG_TRACE,0,TEXT("Probljem4")));
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	DbgLog((LOG_TRACE,0,TEXT("CheckTransform OK")));

	return S_OK;
}


// we will accept the downstream filters proposals regarding the buffer size and number of buffeers,
// unless they are nonsense.
// we will force our allocator, don't care about the proposed one
HRESULT CTrackerFilter :: DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProp)	{

	DbgLog((LOG_TRACE,0,TEXT("DecideBufferSize")));

	CheckPointer(pAlloc,E_POINTER);
	CheckPointer(pProp,E_POINTER);

	if (!m_pInput->IsConnected())	{
		return E_UNEXPECTED;
	}

	CheckPointer(pAlloc,E_POINTER);
    CheckPointer(pProp,E_POINTER);
    HRESULT hr = S_OK;

	DbgLog((LOG_TRACE,0,TEXT("requirement: %d, at worst: %d"), pProp->cbBuffer, m_params.bufferSize));

	// we set cbBuffer to the number of CMovingObjects in the output buffer/array
	if (pProp->cbBuffer <= 0)
		pProp->cbBuffer = outputAllocProps.cbBuffer = m_params.bufferSize;
	// else don't change anything
	if (pProp->cBuffers <= 0)
		pProp->cBuffers = outputAllocProps.cBuffers = 5;	// 5 should also be a parameter -> a TODO :)
	// pProp->cbAlign = outputAllocProps.cbAlign;			// irrelevant
	// pProp->cbPrefix = outputAllocProps.cbPrefix;		// irrelevant
    
	ASSERT(pProp->cbBuffer);

	ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProp,&Actual);
    if (FAILED(hr)) {
        return hr;
    }
	
	DbgLog((LOG_TRACE,0,TEXT("DecideBufferSize alles OK")));

    return NOERROR;
}

//		...oooOOO Oldskul konzervativni treking OOOooo...
/*
// in pSource we expect to have a binarized image. What we want to do here is represent each connected component
// with a center of its bounding box and track those points by Kalman filters
HRESULT CTrackerFilter :: Transform( IMediaSample* pSource, IMediaSample* pDest )	{

	CAutoLock lock(&m_csReceive);

	DbgLog((LOG_TRACE,0,TEXT("")));
	DbgLog((LOG_TRACE,0,TEXT("New Iteration")));

	Ipp8u* pSourceBuffer;
	LPBYTE pDestBufferRaw;
	// pointer to the buffers
	pSource->GetPointer( &pSourceBuffer );
	pDest->GetPointer( &pDestBufferRaw );
	CMovingObject* pDestBuffer = (CMovingObject*) pDestBufferRaw;

	cvSetData( pInputCV_C1, pSourceBuffer, step_8u_C1 );

	DbgLog((LOG_TRACE,0,TEXT("At the beginning: ")));
	for( iterOld = objectsOld.begin(); iterOld != objectsOld.end(); ++iterOld )
	{
		DbgLog((LOG_TRACE,0,TEXT("	%d"), iterOld->get()->ID));
	}

	CvSeq* contour = FindMovingObjects();

	TrackObjects();

			// copy the CMovingObjects' data to the output buffer (output array has already been initialized)

	if(objectsOld.size() >= m_params.bufferSize)
	{
		// copy only bufferSize CMovingObjects to output to prevent writing on unallocated mem.
		unsigned int i;
		for( iterOld = objectsOld.begin(), i = 0; i<m_params.bufferSize ; ++iterOld, ++i )
		{
			pDestBuffer[i].ID = iterOld->get()->ID;
			pDestBuffer[i].rect = iterOld->get()->rect;
			pDestBuffer[i].TTL = iterOld->get()->TTL;
			// object center
			pDestBuffer[i].center.x = iterOld->get()->center.x;
			pDestBuffer[i].center.y = iterOld->get()->center.y;
		}

	}
	else
	{
		unsigned int i;
		for( iterOld = objectsOld.begin(), i = 0; iterOld != objectsOld.end(); ++iterOld, ++i )
		{
			pDestBuffer[i].ID = iterOld->get()->ID;
			pDestBuffer[i].rect = iterOld->get()->rect;
			pDestBuffer[i].TTL = iterOld->get()->TTL;
			// object center
			pDestBuffer[i].center.x = iterOld->get()->center.x;
			pDestBuffer[i].center.y = iterOld->get()->center.y;
		}
		
		// set the rect values of the object that is next in the output buffer to 0. this signals that
		// the objects that follow in the buffer are not valid
		pDestBuffer[i].rect.height = 0;
		pDestBuffer[i].rect.width = 0;
	}


	DbgLog((LOG_TRACE,0,TEXT("At the endining: ")));
	for( iterOld = objectsOld.begin(); iterOld != objectsOld.end(); ++iterOld )
	{
		DbgLog((LOG_TRACE,0,TEXT("	%d"), iterOld->get()->ID));
	}

	// clear after this iteration
	if( contour != NULL )	{		
		cvClearSeq( contour );
	}
	cvClearMemStorage( contourStorage );

	return NOERROR;
}



// we make an abstraction of each connected component as a moving object, and sort it in an appropriate list
//	- if it is considered to be an object from the last frame in objectsWithFit
//	- if it is considered to be an entirely new object in objectsNew
CvSeq* CTrackerFilter :: FindMovingObjects()
{
	// find contours

	CvSeq* contour;
	int numContours = cvFindContours(	pInputCV_C1, contourStorage,
										&contour, sizeof( CvContour ), CV_RETR_EXTERNAL,
										CV_CHAIN_APPROX_NONE );
			
	CvSeq* contourIter = contour;		// so we don't lose the original pointer
	for( ; contourIter; contourIter = contourIter->h_next )
	{
		// contour must satisfy a minimum dimensions condition
		CvRect contourRect = cvBoundingRect( contourIter, 0);
		if( ( contourRect.width <= m_params.minimalSize.width ) &&
			( contourRect.width <= m_params.minimalSize.height ) )	{	continue;	}

		// create a CMovingObject out of each contour
		SPMovingObject	spNewObject(new CMovingObject(0,								// we will assign ID later
										contourRect,
										m_params.timeToLive)
						);
		
		// now we check if center of our newObject falls into region where kalman filter
		// expects our old object to be. we check that for all old objects
		double distance_x, distance_y;

		for( iterOld = objectsOld.begin(); iterOld != objectsOld.end(); ++iterOld )
		{
			// check the a priori prediction variance to see where to look for an CMovingObject that fits
			double rooted_sigma_x = sqrt( iterOld->get()->kalman->error_cov_pre->data.fl[0] );
			double rooted_sigma_y = sqrt( iterOld->get()->kalman->error_cov_pre->data.fl[5] );

			//DbgLog((LOG_TRACE,0,TEXT("rooted_sigma_x = %g; rooted_sigma_y = %g"), rooted_sigma_x, rooted_sigma_y));

			distance_x = fabs( iterOld->get()->predictedX - (float)spNewObject->center.x );
			distance_y = fabs( iterOld->get()->predictedY - (float)spNewObject->center.y );

			if( ( distance_x <= rooted_sigma_x )&&( distance_y <= rooted_sigma_y ) )
			{
				SPFitsTo spFitsTo (new CFitsTo(iterOld->get()->ID, distance_x + distance_y));
				spNewObject->fitsTo.push_back( spFitsTo );

				DbgLog((LOG_TRACE,0,TEXT("				fitsTo grows, %d"), spFitsTo->ID));
			}
		}

		// if possible fit hasn't been found for this object, it is considered to have appeared
		// for the first time and put to the appropriate list
		if( spNewObject->fitsTo.empty() )	{
			objectsNew.push_front( spNewObject );
		}
		else	{
			objectsWithFit.push_front( spNewObject );

			// lets check the fitsTo
			std::list <SPFitsTo>::iterator i;
			DbgLog((LOG_TRACE,0,TEXT("		fictu")));
			for( i = spNewObject->fitsTo.begin(); i != spNewObject->fitsTo.end(); ++i )
			{
				DbgLog((LOG_TRACE,0,TEXT("	%d"), i->get()->ID));
			}
		}
	}

	return contour;
}

// we want to associate the objectOld with the element of objectsWithFit which is closest
// to objectOld's Kalman prediction of location. Unassociated elements of objectNew will
// be stored in objectsNew, and are considered appearing for the first time.
// We do ti in a greedy way - in each iteration we take the closest object. Alternatively,
// it is posssible to minimize some global measure, e.g. we could assign new objects to old ones
// in a way that minimizes the sum of distances between assigned pairs.
void CTrackerFilter :: TrackObjects()	{
	
	for( iterNew = objectsWithFit.begin(); iterNew != objectsWithFit.end(); ++iterNew )	{
		// sort them by distances from predicted location.
		iterNew->get()->fitsTo.sort();
	}

	double distance;

	for( ; !objectsWithFit.empty(); )
	{
		std::list <SPMovingObject>::iterator iterCount;
		std::list <SPFitsTo>::iterator iterCount2;	// iterator that passes through CFitsTo list of each new Object

		distance = frameSize.height + frameSize.width + 2;	// so that every other distance is smaller
		iterNew = NULL;

		for( iterCount = objectsWithFit.begin(); iterCount != objectsWithFit.end(); ++iterCount )
		{
			if( iterCount->get()->fitsTo.begin()->get()->distance < distance )
			{
				iterNew = iterCount;
				distance = iterCount->get()->fitsTo.begin()->get()->distance;
			}
		}

		// we move &iterNew to objectsSuccessors
		objectsSuccessors.splice( objectsSuccessors.end(), objectsWithFit, iterNew );

		// remove all fitsTo that have iterNew's ID. This means that we have found a fit with an old Object
		// that has this ID and we exclude it from further search.
		for( iterCount = objectsWithFit.begin(); iterCount != objectsWithFit.end(); ++iterCount )
		{		
			for(	iterCount2 = iterCount->get()->fitsTo.begin();
					iterCount2 != iterCount->get()->fitsTo.end();
					++iterCount2 )
			{
				if( iterCount2->get()->ID == iterNew->get()->fitsTo.begin()->get()->ID )
				{
					std::list <SPFitsTo>::iterator iterTmp = iterCount2--;
					iterCount->get()->fitsTo.erase( iterTmp );
					break;
				}
			}

			// check if we have objects with empty fitsTo lists. if so, move them to objectsNew because
			// they really don't fit to any of the old objects; a better fit has been found
			if( iterCount->get()->fitsTo.empty() )
			{	
				std::list <SPMovingObject>::iterator iterTmp = iterCount--;
				objectsNew.splice( objectsNew.begin(), objectsWithFit, iterTmp );
			}

		}
	}
	// this all is not the fastest way in the world to do this but we don't expect a large number of objects
	// (short lists) so it should work OK

	// Checkpoint - at this moment we have:
	// in objectsOld		- old objects, fitted and unfitted
	// in objectsNew		- only objects that appeared for the first time
	// in objectsWithFit	- new objects that have a fit

	// for each element of objectsSuccessors, we do an update (location, kalman) and copy them into objectsOld.
	// we locate objectOld by ID. (storing iterators would be somewhat faster for a list. or using hashmaps?)
	for( iterNew = objectsSuccessors.begin(); iterNew != objectsSuccessors.end(); ++iterNew )
	{
		for( iterOld = objectsOld.begin(); iterOld != objectsOld.end(); ++iterOld)
		{
			if( iterNew->get()->fitsTo.begin()->get()->ID == iterOld->get()->ID )
			{
				// Kalman state measuring & update:
				// we combine prediction and measurement to get Kalman gain and the final estimate
				iterNew->get()->fitsTo.clear();
				iterNew->get()->kalman = iterOld->get()->kalman;
				iterOld->get()->kalman = NULL;
				iterNew->get()->ID = iterOld->get()->ID;

				objectsOld.erase( iterOld );			// erase old object... 

				DbgLog((LOG_TRACE,0,TEXT("Updating ID %d"),iterNew->get()->ID));
			
				// make a measurement. location only.
				measurementD[0] = ( float )( iterNew->get()->center.x );
				measurementD[1] = ( float )( iterNew->get()->center.y );

				cvKalmanCorrect( iterNew->get()->kalman, measurement );

				// predict next state
				const CvMat* prediction = cvKalmanPredict( iterNew->get()->kalman, NULL );
				iterNew->get()->predictedX = prediction->data.fl[0];
				iterNew->get()->predictedY = prediction->data.fl[1];

					DbgLog((LOG_TRACE,0,TEXT("	center: ( %d, %d ), prediction: ( %f, %f )"),
					iterNew->get()->center.x, iterNew->get()->center.y, iterNew->get()->predictedX, iterNew->get()->predictedY ));

				break;
			}
		}
	}

	// Checkpoint - now each of the objectsOld which had an assigned successor has had its Kalman filter's
	// state updated and lives on in ObjectsSuccessors. objectsOld now contains only old objects which do not
	// have a successor. What to do with them?

	// decrease TTL from each remaining object of objectsOld. If its TTL expires, we kill them
	for( iterOld = objectsOld.begin(); iterOld != objectsOld.end(); )
	{
		if( (iterOld->get()->TTL)-- <= 0 )
		{
			DbgLog((LOG_TRACE,0,TEXT("Killing: %d"), iterOld->get()->ID));
			objectsOld.erase( iterOld++ );
		}
		/*else
		{
			// feed Kalman filter with its last a posteriori state estimation as measurements
			// we cheat KF, so we should increase variance at least
			measurementD[0] = ( float )( iterOld->get()->kalman->state_post->data.fl[0] +
											iterOld->get()->kalman->state_post->data.fl[2] );
			measurementD[1] = ( float )( iterOld->get()->kalman->state_post->data.fl[1] +
											iterOld->get()->kalman->state_post->data.fl[3] );
			cvKalmanCorrect( iterOld->get()->kalman, Measurement );

			// predict next state
			const CvMat* prediction = cvKalmanPredict( iterOld->get()->kalman, NULL );
			iterOld->get()->predictedX = prediction->data.fl[0];
			iterOld->get()->predictedY = prediction->data.fl[1];

			// if object (its center has gotten out of the frame), delete object
			if( ( iterOld->get()->predictedX<0 ) || ( iterOld->get()->predictedX>=FrameSize.width ) )
			{
				objectsOld.erase( iterOld++ );
			}
			else
			{
				if( ( iterOld->get()->predictedY<0 ) || ( iterOld->get()->predictedY>=FrameSize.height ) )
				{
					objectsOld.erase( iterOld++ );
				}
				else
				{
					++iterOld;
				}
			}
		}*/
/*		++iterOld;		// comment this row and uncomment above section to feed kalman with prediction instead of freezing it
	}

	// move objects from objectsSuccessors to oldObjects
	objectsOld.splice( objectsOld.end(), objectsSuccessors );
	
	// finally, we assign ID to every object from objectsNew and move it to oldObjects.
	for( ; !objectsNew.empty(); )
	{
		iterNew = objectsNew.begin();
		// Kalman initialization and update for new objects
		iterNew->get()->InitializeKalman(m_params.initialKalman);

		// we have to initialize in some way
		const CvMat* prediction = cvKalmanPredict( iterNew->get()->kalman, NULL );
		iterNew->get()->predictedX = prediction->data.fl[0];
		iterNew->get()->predictedY = prediction->data.fl[1];

		iterNew->get()->ID = ++(uniqueID);
	
		objectsOld.splice( objectsOld.end(), objectsNew, iterNew );	// move that new object to old objects;
	}
}
*/

// this is a modification with smart pointers
//
// in pSource we expect to have a binarized image. What we want to do here is represent each connected component
// with a center of its bounding box and track those points by Kalman filters
HRESULT CTrackerFilter :: Transform( IMediaSample* pSource, IMediaSample* pDest )	{

	CAutoLock lock(&m_csReceive);

	DbgLog((LOG_TRACE,0,TEXT("")));
	DbgLog((LOG_TRACE,0,TEXT("New Iteration")));

	Ipp8u* pSourceBuffer;
	LPBYTE pDestBufferRaw;
	// pointer to the buffers
	pSource->GetPointer( &pSourceBuffer );
	pDest->GetPointer( &pDestBufferRaw );
	CMovingObject* pDestBuffer = (CMovingObject*) pDestBufferRaw;

	cvSetData( pInputCV_C1, pSourceBuffer, step_8u_C1 );
	cvSetData( pHelperCV_C1, pHelper_8u_C1, step_8u_C1 );
	
	// we copy the pInputCV_C1 and work with pHelperCV_C1 so we don't mess it up... we need it to preserve
	// (and display) the image from the upstream filter
	cvCopy (pInputCV_C1, pHelperCV_C1);

	CvSeq* contour = FindMovingObjects(pHelperCV_C1);

	TrackObjects();

		// copy the CMovingObjects' data to the output buffer (output array has already been initialized)

	if(objectsOld.size() >= m_params.bufferSize)
	{
		// copy only bufferSize CMovingObjects to output to prevent writing on unallocated mem.
		unsigned int i;
		for( iterOld = objectsOld.begin(), i = 0; i<m_params.bufferSize ; ++iterOld, ++i )
		{
			pDestBuffer[i].ID = iterOld->get()->ID;
			pDestBuffer[i].rect = iterOld->get()->rect;
			// object center
			pDestBuffer[i].center.x = iterOld->get()->center.x;
			pDestBuffer[i].center.y = iterOld->get()->center.y;
			// predictions
			pDestBuffer[i].predictedX = iterOld->get()->predictedX;
			pDestBuffer[i].predictedY = iterOld->get()->predictedY;
			// velocities
			pDestBuffer[i].velocityX = iterOld->get()->velocityX;
			pDestBuffer[i].velocityY = iterOld->get()->velocityY;

		}

	}
	else
	{
		unsigned int i;
		for( iterOld = objectsOld.begin(), i = 0; iterOld != objectsOld.end(); ++iterOld, ++i )
		{
			pDestBuffer[i].ID = iterOld->get()->ID;
			pDestBuffer[i].rect = iterOld->get()->rect;
			// object center
			pDestBuffer[i].center.x = iterOld->get()->center.x;
			pDestBuffer[i].center.y = iterOld->get()->center.y;
			// predictions
			pDestBuffer[i].predictedX = iterOld->get()->predictedX;
			pDestBuffer[i].predictedY = iterOld->get()->predictedY;
			// velocities
			pDestBuffer[i].velocityX = iterOld->get()->velocityX;
			pDestBuffer[i].velocityY = iterOld->get()->velocityY;
			
		}
		
		// set the rect values of the object that is next in the output buffer to 0. this signals that
		// the objects that follow in the buffer are not valid
		pDestBuffer[i].rect.height = 0;
		pDestBuffer[i].rect.width = 0;
	}
	
	// clear after this iteration
	if( contour != NULL )	{		
		cvClearSeq( contour );
	}
	cvClearMemStorage( contourStorage );


	return NOERROR;
}


// we make an abstraction of each connected component as a moving object, and sort it in an appropriate list
//	- if it is considered to be an object from the last frame in objectsWithFit
//	- if it is considered to be an entirely new object in objectsNew
CvSeq* CTrackerFilter :: FindMovingObjects(IplImage* pInputCV_C1)
{
	// find contours

	CvSeq* contour;
	int numContours = cvFindContours(	pInputCV_C1, contourStorage,
										&contour, sizeof( CvContour ), CV_RETR_EXTERNAL,
										CV_CHAIN_APPROX_NONE );
			
	CvSeq* contourIter = contour;		// so we don't lose the original pointer
	for( ; contourIter; contourIter = contourIter->h_next )
	{
		// contour must satisfy a minimum dimensions condition
		CvRect contourRect = cvBoundingRect( contourIter, 0);
		if( ( contourRect.width <= m_params.minimalSize.width ) &&
			( contourRect.width <= m_params.minimalSize.height ) )	{	continue;	}

		// create a CMovingObject out of each contour
		SPMovingObject	spNewObject	(new CMovingObject(0,			// we will assign ID later
										contourRect,
										m_params.timeToLive)
									);

		DbgLog((LOG_TRACE,0,TEXT("contourRect = %d; "), contourRect.x));

		
		// now we check if center of our newObject falls into region where kalman filter
		// expects our old object to be. we check that for all old objects
		double distance_x, distance_y;

		for( iterOld = objectsOld.begin(); iterOld != objectsOld.end(); ++iterOld )
		{
			// check the a priori prediction variance to see where to look for an CMovingObject that fits
			double rooted_sigma_x = sqrt( iterOld->get()->kalmanX->error_cov_pre->data.fl[0] );
			double rooted_sigma_y = sqrt( iterOld->get()->kalmanY->error_cov_pre->data.fl[0] );

			DbgLog((LOG_TRACE,0,TEXT("rooted_sigma_x = %g; rooted_sigma_y = %g"), rooted_sigma_x, rooted_sigma_y));

			distance_x = fabs( iterOld->get()->predictedX - (float)spNewObject->center.x );
			distance_y = fabs( iterOld->get()->predictedY - (float)spNewObject->center.y );

			if( ( distance_x <= rooted_sigma_x )&&( distance_y <= rooted_sigma_y ) )
			{
				//if we have a possible fit
				spNewObject->fitsTo.push_back( CFitsTo(iterOld->get()->ID, distance_x + distance_y) );
			}
		}
 
		// if possible fit hasn't been found for this object, it is considered to have appeared
		// for the first time and put to the appropriate list
		if( spNewObject->fitsTo.empty() )	{
			objectsNew.push_front( spNewObject );
		}
		else	{
			objectsWithFit.push_front( spNewObject );
		}
	}

	return contour;
}

// we want to associate the objectOld with the element of objectsWithFit which is closest
// to objectOld's Kalman prediction of location. Unassociated elements of objectNew will
// be stored in objectsNew, and are considered appearing for the first time.
// We do ti in a greedy way - in each iteration we take the closest object. Alternatively,
// it is posssible to minimize some global measure, e.g. we could assign new objects to old ones
// in a way that minimizes the sum of distances between assigned pairs.
void CTrackerFilter :: TrackObjects()	{
	
	for( iterNew = objectsWithFit.begin(); iterNew != objectsWithFit.end(); ++iterNew )	{
		// sort them by distances from predicted location.
		iterNew->get()->fitsTo.sort();
	}

	double distance;

	for( ; !objectsWithFit.empty(); )
	{
		std::list <SPMovingObject>::iterator iterCount;
		distance = INT_MAX;	// so that every other distance that makes sense is smaller
		iterNew = NULL;

		std::list <CFitsTo>::iterator iterCount2;	// iterator that passes through CFitsTo list of each new Object

		DbgLog((LOG_TRACE,0,TEXT("evo tucam6")));

		for( iterCount = objectsWithFit.begin(); iterCount != objectsWithFit.end(); ++iterCount )
		{
			if( iterCount->get()->fitsTo.begin()->distance <= distance )
			{
				iterNew = iterCount;
				distance = iterCount->get()->fitsTo.begin()->distance;
			}
		}

		DbgLog((LOG_TRACE,0,TEXT("evo tucam 7")));
		// we move &iterNew to objectsSuccessors
		objectsSuccessors.splice( objectsSuccessors.begin(), objectsWithFit, iterNew );

		// remove all fitsTo that have iterNew's ID. This means that we have found a fit with an old Object
		// that has this ID and we exclude it from further search.

		DbgLog((LOG_TRACE,0,TEXT("evo tucam")));

		for( iterCount = objectsWithFit.begin(); iterCount != objectsWithFit.end(); ++iterCount )
		{		
			DbgLog((LOG_TRACE,0,TEXT("evo tucam2")));
			for(	iterCount2 = iterCount->get()->fitsTo.begin();
					iterCount2 != iterCount->get()->fitsTo.end();
					++iterCount2 )
			{
				DbgLog((LOG_TRACE,0,TEXT("evo tucam3")));
				if( iterCount2->ID == iterNew->get()->fitsTo.begin()->ID )
				{
					std::list <CFitsTo>::iterator iterTmp = iterCount2--;
					iterCount->get()->fitsTo.erase( iterTmp );
					break;
				}
			}

			// check if we have objects with empty fitsTo lists. if so, move them to objectsNew because
			// they really don't fit to any of the old objects; a better fit has been found
			DbgLog((LOG_TRACE,0,TEXT("evo tucam4")));
			if( iterCount->get()->fitsTo.empty() )
			{	
				std::list <SPMovingObject>::iterator iterTmp = iterCount--;
				objectsNew.splice( objectsNew.begin(), objectsWithFit, iterTmp );
			}
			DbgLog((LOG_TRACE,0,TEXT("evo tucam5")));

		}
	}
	// this all is not the fastest way in the world to do this but we don't expect a large number of objects
	// (short lists) so it should work OK

	// Checkpoint - at this moment we have:
	// in objectsOld		- old objects, fitted and unfitted
	// in objectsNew		- only objects that appeared for the first time
	// in objectsWithFit	- new objects that have a fit

	// for each element of objectsSuccessors, we do an update (location, kalman) and copy them into objectsOld.
	// we locate objectOld by ID. (storing iterators would be somewhat faster for a list. or using hashmaps?)
	for( iterNew = objectsSuccessors.begin(); iterNew != objectsSuccessors.end(); ++iterNew )
	{
		for( iterOld = objectsOld.begin(); iterOld != objectsOld.end(); ++iterOld)
		{
			if( iterNew->get()->fitsTo.begin()->ID == iterOld->get()->ID )
			{
				// Kalman state measuring & update:
				// we combine prediction and measurement to get Kalman gain and the final estimate
				iterNew->get()->fitsTo.clear();
				iterNew->get()->kalmanX = iterOld->get()->kalmanX;
				iterNew->get()->kalmanY = iterOld->get()->kalmanY;
				iterOld->get()->kalmanX = NULL;				// we don't release the kalman structure - recycle for a greener world
				iterOld->get()->kalmanY = NULL;
				iterNew->get()->ID = iterOld->get()->ID;

				objectsOld.erase( iterOld );			// erase old object... 
			
				// x
				// make a measurement. location only.
				measurementD[0] = ( float )( iterNew->get()->center.x );
				DbgLog((LOG_TRACE,0,TEXT("Measuring x: %d"), iterNew->get()->center.x));
				cvKalmanCorrect( iterNew->get()->kalmanX, measurement );
				// predict next state
				const CvMat* predictionX = cvKalmanPredict( iterNew->get()->kalmanX, NULL );
				iterNew->get()->predictedX = predictionX->data.fl[0];
				DbgLog((LOG_TRACE,0,TEXT("Predicting x: %g"), iterNew->get()->predictedX));
				// take and store velocity component
				iterNew->get()->velocityX = iterNew->get()->kalmanX->state_post->data.fl[1];

				// y
				// make a measurement. location only.
				measurementD[0] = ( float )( iterNew->get()->center.y );
				DbgLog((LOG_TRACE,0,TEXT("Measuring y: %d"), iterNew->get()->center.y));
				cvKalmanCorrect( iterNew->get()->kalmanY, measurement );
				// predict next state
				const CvMat* predictionY = cvKalmanPredict( iterNew->get()->kalmanY, NULL );
				iterNew->get()->predictedY = predictionY->data.fl[0];
				DbgLog((LOG_TRACE,0,TEXT("Predicting y: %g"), iterNew->get()->predictedY));
				// take and store velocity component
				iterNew->get()->velocityY = iterNew->get()->kalmanY->state_post->data.fl[1];

				break;
			}
		}
	}

	// Checkpoint - now each of the objectsOld which had an assigned successor has had its Kalman filter's
	// state updated and lives on in ObjectsSuccessors. objectsOld now contains only old objects which do not
	// have a successor. What to do with them?

	// decrease TTL from each remaining object of objectsOld. If its TTL expires, we kill them
	for( iterOld = objectsOld.begin(); iterOld != objectsOld.end(); )
	{
		if( (iterOld->get()->TTL)-- <= 0 )
		{
			DbgLog((LOG_TRACE,0,TEXT("Killing: %d"), iterOld->get()->ID));
			objectsOld.erase( iterOld++ );
		}
		/*else
		{
			// feed Kalman filter with its last a posteriori state estimation as measurements
			// we cheat KF, so we should increase variance at least
			measurementD[0] = ( float )( iterOld->kalman->state_post->data.fl[0] +
											iterOld->kalman->state_post->data.fl[2] );
			measurementD[1] = ( float )( iterOld->kalman->state_post->data.fl[1] +
											iterOld->kalman->state_post->data.fl[3] );
			cvKalmanCorrect( iterOld->kalman, Measurement );

			// predict next state
			const CvMat* prediction = cvKalmanPredict( iterOld->kalman, NULL );
			iterOld->predictedX = prediction->data.fl[0];
			iterOld->predictedY = prediction->data.fl[1];

			// if object (its center has gotten out of the frame), delete object
			if( ( iterOld->predictedX<0 ) || ( iterOld->predictedX>=FrameSize.width ) )
			{
				objectsOld.erase( iterOld++ );
			}
			else
			{
				if( ( iterOld->predictedY<0 ) || ( iterOld->predictedY>=FrameSize.height ) )
				{
					objectsOld.erase( iterOld++ );
				}
				else
				{
					++iterOld;
				}
			}
		}*/
		++iterOld;		// comment this row and uncomment above section to feed kalman with prediction instead of freezing it
	}

	// move objects from objectsSuccessors to oldObjects
	objectsOld.splice( objectsOld.end(), objectsSuccessors );
	
	// finally, we assign ID to every object from objectsNew and move it to oldObjects.
	for( ; !objectsNew.empty(); )
	{
		iterNew = objectsNew.begin();
		// Kalman initialization and update for new objects
		iterNew->get()->InitializeKalmanLocation(m_params.initialKalman);

		// we have to initialize predictions in some way
		// x

		const CvMat* predictionX = cvKalmanPredict( iterNew->get()->kalmanX, NULL );
		iterNew->get()->predictedX	= predictionX->data.fl[0];
		DbgLog((LOG_TRACE,0,TEXT("Initial location: %d"), iterNew->get()->center.x));
		DbgLog((LOG_TRACE,0,TEXT("Initial prediction: %g, %g, %g"), predictionX->data.fl[0],
			predictionX->data.fl[1], predictionX->data.fl[2]));
						
		iterNew->get()->velocityX = iterNew->get()->kalmanX->state_post->data.fl[1];
		// y
		const CvMat* predictionY = cvKalmanPredict( iterNew->get()->kalmanY, NULL );
		iterNew->get()->predictedY	= predictionY->data.fl[0];
		DbgLog((LOG_TRACE,0,TEXT("Initial location: %d"), iterNew->get()->center.y));
		DbgLog((LOG_TRACE,0,TEXT("Initial prediction: %g, %g, %g"),
			predictionY->data.fl[0], predictionY->data.fl[1], predictionY->data.fl[2]));
		iterNew->get()->velocityY = iterNew->get()->kalmanY->state_post->data.fl[1];

		iterNew->get()->ID = ++(uniqueID);

		objectsOld.splice( objectsOld.end(), objectsNew, iterNew );	// move that new object to old objects;
	}
}



// setup data - allows the self-registration to work.
const AMOVIESETUP_MEDIATYPE sudPinTypes =
{
  &MEDIATYPE_NULL        // clsMajorType
, &MEDIASUBTYPE_NULL     // clsMinorType
};

const AMOVIESETUP_PIN psudPins[] =
{ { L"Input"            // strName
  , FALSE               // bRendered
  , FALSE               // bOutput
  , FALSE               // bZero
  , TRUE		        // bMany
  , &CLSID_NULL         // clsConnectsToFilter
  , L""                 // strConnectsToPin
  , 1                   // nTypes
  , &sudPinTypes        // lpTypes
  }
, { L"Output"           // strName
  , FALSE               // bRendered
  , TRUE                // bOutput
  , FALSE               // bZero
  , TRUE                // bMany
  , &CLSID_NULL         // clsConnectsToFilter
  , L""                 // strConnectsToPin
  , 1                   // nTypes
  , &sudPinTypes        // lpTypes
  }
};

const AMOVIESETUP_FILTER sudExtractY =
{ 
	&CLSID_TRACKER				// clsID
	, FILTER_NAME				// strName
	, MERIT_DO_NOT_USE			// dwMerit
	, 2							// nPins
	, psudPins					// lpPin
};

REGFILTER2 rf2FilterReg = {
    1,                    // Version 1 (no pin mediums or pin category).
    MERIT_DO_NOT_USE,     // Merit.
    2,                    // Number of pins.
    psudPins              // Pointer to pin information.
};

// Class factory template - needed for the CreateInstance mechanism to
// register COM objects in our dynamic library
CFactoryTemplate g_Templates[]=
    {   
		{
			FILTER_NAME,
			&CLSID_TRACKER,
			CTrackerFilter::CreateInstance,
			NULL,
			&sudExtractY
		}
    };

int g_cTemplates = sizeof(g_Templates)/sizeof(g_Templates[0]);


// Provide the way for COM to create a filter object
CUnknown* WINAPI CTrackerFilter :: CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
	CheckPointer(phr,NULL);
	
	CTrackerFilter* spNewObject = new CTrackerFilter( NAME("Tracker"), punk, phr );

	if (spNewObject == NULL)
	{
		*phr = E_OUTOFMEMORY;
	}

	return spNewObject;
} 


// DllEntryPoint
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, 
                      DWORD  dwReason, 
                      LPVOID lpReserved)	{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}


// Exported entry points for registration and unregistration 
// (in this case they only call through to default implementations).

// Self registration
// called  when the user runs the Regsvr32.exe tool
// we'll use FilterMapper2 interface for managing registration
STDAPI DllRegisterServer()
{
	HRESULT hr;
	IFilterMapper2 *pFM2 = NULL;

	hr = AMovieDllRegisterServer2(TRUE);
	if (FAILED(hr))
		return hr;

	hr = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
		IID_IFilterMapper2, (void **)&pFM2);

	if (FAILED(hr))
		return hr;

	// we will create our category, we'll call it "Somehow"
	pFM2->CreateCategory( CLSID_MotionTrackingCategory, MERIT_NORMAL, L"Motion Tracking");

	hr = pFM2->RegisterFilter(
		CLSID_TRACKER,							// Filter CLSID. 
		FILTER_NAME,							// Filter name.
		NULL,								    // Device moniker. 
		&CLSID_MotionTrackingCategory,			// category.
		FILTER_NAME,							// Instance data.
		&rf2FilterReg							// Pointer to filter information.
		);
	pFM2->Release();
	return hr;

}

 //Self unregistration
 //called  when the user runs the Regsvr32.exe tool with -u command.
STDAPI DllUnregisterServer()
{	
	HRESULT hr;
	IFilterMapper2 *pFM2 = NULL;

	hr = AMovieDllRegisterServer2(FALSE);
	if (FAILED(hr))
		return hr;

	hr = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
		IID_IFilterMapper2, (void **)&pFM2);

	if (FAILED(hr))
		return hr;

	hr = pFM2->UnregisterFilter(&CLSID_MotionTrackingCategory, 
		FILTER_NAME, CLSID_TRACKER);

	pFM2->Release();
	return hr;
}