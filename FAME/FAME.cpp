/*#ifndef FAME_H
	#include "FAME.h"
#endif

#ifndef MATH_H
	#include <math.h>
	#define MATH_H
#endif

CFameFilter::CFameFilter(float minStep)	{
	
	this->minStep = minStep;
	fpFeedFilter = FameInitialization;

}

void CFameFilter::FameInitialization(unsigned char value)	{

	estimate = value;

	step = (float)( value/2 );
	if(  step < minStep )
		estimate = minStep;
	else
		estimate = step;

	// set function pointer to the iterative part of the algorithm
	fpFeedFilter = FameIteration;
}


void CFameFilter::FameIteration(unsigned char value)	{

	if( estimate > value )
		estimate -= step;
	else
		if( estimate < value ) 
			estimate += step;
	if( fabs( value - estimate ) < step )
		step /= 2;
	// else step remains unchanged

	// give the algorithm a windowing flavor
	step *= float( 1 + epsilon );

}


void CFameFilter::FeedFilter(unsigned char value)	{

	( *this.*fpFeedFilter )( value );
}

*/