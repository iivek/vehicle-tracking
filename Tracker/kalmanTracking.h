#ifndef KALMANTRACKING_H
#define KALMANTRACKING_H

#ifndef CV_H
	#include <cv.h>			// OpenCV
	#define CV_H
#endif

#ifndef CXCORE_H
	#include <cxcore.h>		// OpenCV
	#define CXCORE_H
#endif

#ifndef LIST
	#include <list>
	#define LIST
#endif

#ifndef SMART_PTR_HPP
	#include "boost\smart_ptr.hpp"
	#define SMART_PTR_HPP
#endif


// custom output datatype
// {A6012679-3603-4bf5-97B3-7E7F2F690854}
DEFINE_GUID(MEDIASUBTYPE_CMovingObjects, 
0xa6012679, 0x3603, 0x4bf5, 0x97, 0xb3, 0x7e, 0x7f, 0x2f, 0x69, 0x8, 0x54);

struct KalmanParameters
{
	float	TransitionMatrixD[9];
	float	MeasurementMatrixD[3];
	float	ProcessNoise_covD[9];
	float	ErrorPost_covD[9];
	float	MeasurementNoise_covD[1];
	float	State_postD[3];
	CvMat*	TransitionMatrix;
	CvMat*	MeasurementMatrix;
	CvMat*	ProcessNoise_cov;
	CvMat*	ErrorPost_cov;
	CvMat*	MeasurementNoise_cov;
	CvMat*	State_post;
};


class CFitsTo
{
public:
	double distance;	
	unsigned long ID;
	// defining operator < so we can use list::sort()
	bool operator<(	CFitsTo other ){ return( distance < other.distance ); }

	CFitsTo(unsigned long ID, double distance)	{
		this->distance = distance;
		this->ID = ID;
	}

	CFitsTo()	{};
};

typedef boost::shared_ptr<CFitsTo> SPFitsTo;


// TODO: write a factory class for moving object instances. We spawn and kill instances all the time, freeing
// and allocating memory over and over again. Allocating memory is expensive (with respect to execution time),
// so a factory that uses arrays would be nice.
// 
class CMovingObject
{
public:

		unsigned long ID;
	CvRect rect;
	IppiPoint center;				// center of rect. we treat our object as an rectangle	
	float velocityX, velocityY;
	float predictedX, predictedY;
	CvKalman* kalmanX;
	CvKalman* kalmanY;
	int TTL;						// Time to live: to keep track when object stops moving. does not help with
									// occlusions, just to keep track of object even though it's not moving anymore

	// TODO: use an inherently sorted container instead of a list, because we will want CFitsTo instances
	// sorted by distances
	//std::list <SPFitsTo> fitsTo;	// list that holds data for object tracking
									// distance from the old object tagged with ID, predicted location
									// and flag that is true if that distance falls into limits
									// defined by sigmas from the prediction covariance matrix
	std::list <CFitsTo> fitsTo;

	CMovingObject()
	{
		ID = 0;
		rect.x = 0;		rect.y = 0;
		TTL = 0;

		// object center
		center.x = 0;
		center.y = 0;

		kalmanX = NULL;
		kalmanY = NULL;
	}

	CMovingObject(unsigned long ID, CvRect rect, int TTL, KalmanParameters kalmanInitial)
	{
		this->ID = ID;
		this->rect = rect;
		this->TTL = TTL;

		// object center
		this->center.x = rect.x + ( int )( rect.width/2 );
		this->center.y = rect.y + ( int )( rect.height/2 );

		InitializeKalmanLocation(kalmanInitial);
	}

	CMovingObject(unsigned long ID, CvRect rect, int TTL):
		kalmanX( NULL ),
		kalmanY( NULL )
	{
		this->ID = ID;
		this->rect = rect;
		this->TTL = TTL;
		// object center
		this->center.x = rect.x + ( int )( rect.width/2 );
		this->center.y = rect.y + ( int )( rect.height/2 );
	}
/*
	// Semi-copy constructor (doesn't copy the Kalman)
	CMovingObject(const CMovingObject &other):
		kalman( NULL )
	{
		this->ID = other.ID;
		this->rect = other.rect;
		this->TTL = other.TTL;
		// object center
		this->center.x = other.center.x;
		this->center.y = other.center.y;
	}
*/
	// TODO: overload the = operator and define the copy constructor

	~CMovingObject()	{
		if (kalmanX) cvReleaseKalman( &kalmanX );
		if (kalmanY) cvReleaseKalman( &kalmanY );
		if (!fitsTo.empty())
		{
			// empty the list
			fitsTo.clear();
		}
	}

	/*	Each coordinate will have its separate kalman filter	*/

	void InitializeKalmanLocation(KalmanParameters kalmanInitial)	{
		
				this->kalmanX = cvCreateKalman(2, 1, 0);
		DbgLog((LOG_TRACE,0,TEXT("How our kalman's dimensions look like")));
		DbgLog((LOG_TRACE,0,TEXT("transition matrix: %d %d	measurement m.: %d %d process noise: %d %d	error cov post: %d %d	measurement noise cov: %d %d	state_post: %d %d"),this->kalmanX->transition_matrix->height,	this->kalmanX->transition_matrix->width,	this->kalmanX->measurement_matrix->height,	this->kalmanX->measurement_matrix->width,	this->kalmanX->process_noise_cov->height,	this->kalmanX->process_noise_cov->width,	this->kalmanX->error_cov_post->height,	this->kalmanX->error_cov_post->width,	this->kalmanX->measurement_noise_cov->height,	this->kalmanX->measurement_noise_cov->width,	this->kalmanX->state_post->height,	this->kalmanX->state_post->width));
		/*	3 states:		location, velocity, acceleration
			1 measurement:	location	*/
		cvCopy( kalmanInitial.TransitionMatrix,		this->kalmanX->transition_matrix );
		cvCopy( kalmanInitial.MeasurementMatrix,	this->kalmanX->measurement_matrix );
		cvCopy( kalmanInitial.ProcessNoise_cov,		this->kalmanX->process_noise_cov );
		cvCopy( kalmanInitial.ErrorPost_cov,		this->kalmanX->error_cov_post );
		cvCopy( kalmanInitial.MeasurementNoise_cov,	this->kalmanX->measurement_noise_cov );
		// only state_post needs to be modified in respect to individual Object
		this->kalmanX->state_post->data.fl[0] = (float)this->center.x;
		this->kalmanX->state_post->data.fl[1] = kalmanInitial.State_post->data.fl[1];
//		this->kalmanX->state_post->data.fl[2] = kalmanInitial.State_post->data.fl[2];
		DbgLog((LOG_TRACE,0,TEXT("aaaargh %g, %g, %g"), this->kalmanX->state_post->data.fl[0],
		this->kalmanX->state_post->data.fl[1], this->kalmanX->state_post->data.fl[2]));

		this->kalmanY = cvCreateKalman(2, 1, 0);
		/*	3 states:		location, velocity, acceleration
			1 measurement:	location	*/
		cvCopy( kalmanInitial.TransitionMatrix,		this->kalmanY->transition_matrix );
		cvCopy( kalmanInitial.MeasurementMatrix,	this->kalmanY->measurement_matrix );
		cvCopy( kalmanInitial.ProcessNoise_cov,		this->kalmanY->process_noise_cov );
		cvCopy( kalmanInitial.ErrorPost_cov,		this->kalmanY->error_cov_post );
		cvCopy( kalmanInitial.MeasurementNoise_cov,	this->kalmanY->measurement_noise_cov );
		// only state_post needs to be modified in respect to individual Object
		this->kalmanY->state_post->data.fl[0] = (float)this->center.y;
		this->kalmanY->state_post->data.fl[1] = kalmanInitial.State_post->data.fl[1];
//		this->kalmanY->state_post->data.fl[2] = kalmanInitial.State_post->data.fl[2];

	}
		
};

typedef boost::shared_ptr<CMovingObject> SPMovingObject;

/*
// a basic smart pointer implementation
template < typename T > class SmartPointer
{
private:
	T*    pObject;		// we want to wrap this pointer

public:
	SmartPointer(T* pValue) : pObject(pValue)
	{
	}

	~SmartPointer()
	{
		delete pObject;
	}

	T& operator* ()
	{
		return *pObject;
	}

	T* operator-> ()
	{
		return pObject;
	}
};
*/

#endif