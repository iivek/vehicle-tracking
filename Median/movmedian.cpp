#ifndef MOVMEDIAN_H
	#include "movmedian.h"
	#define MOVMEDIAN_H
#endif

CMedianFilter::CMedianFilter( int length )
{
	CurrentLength = 0;
	_MoveMedian_B = false;
	_MoveMedian_E = false;

		//initialize all the LUTs and needed function pointers

	fpLUT_SlideFilter[0] = MedianStays_SlideFilter;
	fpLUT_SlideFilter[1] = MedianToLeft_SlideFilter;
	fpLUT_SlideFilter[2] = MedianToLeft_SlideFilter;
	fpLUT_SlideFilter[3] = MedianToLeft_SlideFilter;
	fpLUT_SlideFilter[4] = MedianToRight_SlideFilter;
	fpLUT_SlideFilter[5] = MedianToRight_SlideFilter;
	fpLUT_SlideFilter[6] = MedianStays_SlideFilter;
	fpLUT_SlideFilter[7] = MedianToRight_SlideFilter;

	fpLUT_Queue[0] = MedianStays0_Queue;
	fpLUT_Queue[1] = MedianStays1_Queue;
	fpLUT_Queue[2] = MedianStays2_Queue;
	fpLUT_Queue[3] = MedianToLeft3_Queue;
	fpLUT_Queue[4] = MedianToRight4_Queue,
	fpLUT_Queue[5] = MedianStays5_Queue;

	fpLUT_Dequeue[0] = MoveMedian0_Dequeue;
	fpLUT_Dequeue[1] = MoveMedian1_Dequeue;
	fpLUT_Dequeue[2] = MoveMedian2_Dequeue;
	fpLUT_Dequeue[3] = MoveMedian3_Dequeue;
	fpLUT_Dequeue[4] = MoveMedian4_Dequeue;
	fpLUT_Dequeue[5] = MoveMedian5_Dequeue;
	fpLUT_Dequeue[6] = MoveMedian6_Dequeue;
	fpLUT_Dequeue[7] = MoveMedian7_Dequeue;
	fpLUT_Dequeue[8] = MoveMedian8_Dequeue;
	fpLUT_Dequeue[9] = MoveMedian9_Dequeue;
	fpLUT_Dequeue[10] = MoveMedian10_Dequeue;
	fpLUT_Dequeue[11] = MoveMedian11_Dequeue;
	

	SetFilterLength( length );
	return;
}

CMedianFilter::CMedianFilter()
{
	CurrentLength = 0;
	_MoveMedian_B = false;
	_MoveMedian_E = false;

		//initialize all the LUTs and needed function pointers

	fpLUT_SlideFilter[0] = MedianStays_SlideFilter;
	fpLUT_SlideFilter[1] = MedianToLeft_SlideFilter;
	fpLUT_SlideFilter[2] = MedianToLeft_SlideFilter;
	fpLUT_SlideFilter[3] = MedianToLeft_SlideFilter;
	fpLUT_SlideFilter[4] = MedianToRight_SlideFilter;
	fpLUT_SlideFilter[5] = MedianToRight_SlideFilter;
	fpLUT_SlideFilter[6] = MedianStays_SlideFilter;
	fpLUT_SlideFilter[7] = MedianToRight_SlideFilter;

	fpLUT_Queue[0] = MedianStays0_Queue;
	fpLUT_Queue[1] = MedianStays1_Queue;
	fpLUT_Queue[2] = MedianStays2_Queue;
	fpLUT_Queue[3] = MedianToLeft3_Queue;
	fpLUT_Queue[4] = MedianToRight4_Queue,
	fpLUT_Queue[5] = MedianStays5_Queue;

	fpLUT_Dequeue[0] = MoveMedian0_Dequeue;
	fpLUT_Dequeue[1] = MoveMedian1_Dequeue;
	fpLUT_Dequeue[2] = MoveMedian2_Dequeue;
	fpLUT_Dequeue[3] = MoveMedian3_Dequeue;
	fpLUT_Dequeue[4] = MoveMedian4_Dequeue;
	fpLUT_Dequeue[5] = MoveMedian5_Dequeue;
	fpLUT_Dequeue[6] = MoveMedian6_Dequeue;
	fpLUT_Dequeue[7] = MoveMedian7_Dequeue;
	fpLUT_Dequeue[8] = MoveMedian8_Dequeue;
	fpLUT_Dequeue[9] = MoveMedian9_Dequeue;
	fpLUT_Dequeue[10] = MoveMedian10_Dequeue;
	fpLUT_Dequeue[11] = MoveMedian11_Dequeue;

	return;
}

CMedianFilter::~CMedianFilter()
{
}

int CMedianFilter::SetFilterLength( int length )
{
	if( length <= 0 )
		return -1;	//error

	Length = length;

	// in order to change the filter order, we will queue (to increase it) or dequeue (to decrease it)
	// input values that follow
	if( Length == CurrentLength)
	{
		// set the fpointer in FeedFilter() to SlideFilter()
		fpFeedFilter = SlideFilter;
	}
	if( Length < CurrentLength )
	{
		int iterations = CurrentLength - Length;
		for( int i=0; i<iterations; ++i )
		{
			Dequeue();
		}

		// set the fpointer in FeedFilter() to SlideFilter()
		fpFeedFilter = SlideFilter;
	}
	if( CurrentLength == 0 )
	{
		fpFeedFilter = InitQueue;
	}
	else
		if( Length > CurrentLength )
		{
			// set the fpointer in FeedFilter() to Queue()
			// ( until filter length reaches filter order. after that fpointer is set to SlideFilter )
			fpFeedFilter = Queue;
		}

	return 1;
}

int CMedianFilter::GetFilterLength()
{
	return Length;
}

int CMedianFilter::GetCurrentLength()
{
	return CurrentLength;
}

void CMedianFilter::FeedFilter( unsigned char value)
{
	//call the appropriate method, Queue() or SlideFilter(), when new data arrive
	( *this.*fpFeedFilter )( value );

	return;
}

		//Automaton that relocates median, in SlideFilter() method:
//we first erase value that has to go if it's not the median, then relocate the median as in the table
//---------------------- -----------  --------------
// LRG_E_In	  LRG_Out	| MedianOut ||	Median		|
//---------- ----------- -----------  --------------				
//	0		|	0		|	0		||	stays		|
//	0		|	0		|	1		||				|	// this can never be
//	0		|	1		|	0		||	BEGIN...<-	|					
//	0		|	1		|	1		||	BEGIN...<-	|
//	1		|	0		|	0		||	->...END	|				
//	1		|	0		|	1		||				|	// this can never be
//	1		|	1		|	0		||	stays		|
//	1		|	1		|	1		||	->...END	|
//---------- ----------- -----------  --------------
//
//LRG_E_In:		equals 1 if input value is strictly larger than median,
//				otherwise equals 0
//LESS_E_Out:	equals 1 if value that gets erased is lesser than or equal to median,
//				otherwise equals 0
//
//MedianOut:	0 marks regular sliding
//				1 marks that median gets dequeued
//
//because keys in multimap are sorted by less<int>,
//for input values that enter filter we use >= to determine whether they're left or right from median and
//for input values that exit filter we use > to determine whether they're left or right from median
//example: filter order is 5, filtering list of values: 1 3 7 3 9 3 8
//				  median
//					|
//					V
//			1	3	3	7	9
//
//			1	3	3	3	7
//				 \		\__new value, equal to median, and gets on median's right side
//				  \__in the next step this value gets erased. it's equal to median and in its left side
//			1	3	3	7	8
//
//					
//here we don't care about _MoveMedian_B and _MoveMedian_E, or change their values
//
//we use LUTs in the implementation


void CMedianFilter::SlideFilter( unsigned char ValueIn )
{
	unsigned char ValueOut;
	multiset_iter iInsertionPlace, iRemovalPlace;

	// queue
	iInsertionPlace = Filter_value.insert( ValueIn);	//value-link
	Filter_time.push_back( iInsertionPlace );			//time-link
	// dequeue preparation
	iRemovalPlace = *Filter_time.begin();
	ValueOut = *iRemovalPlace;

	bool _LargerEqual_IN	= ( ValueIn >= *iMedian );
	bool _Larger_OUT		= ( ValueOut > *iMedian );
	bool _MedianOut			= ( iMedian == iRemovalPlace );

	// dequeue if not median, so we don't declare a value that will be erased as median
	if( !_MedianOut )
	{
		//dequeue
		Filter_value.erase( iRemovalPlace );
		Filter_time.pop_front( );
	}

		//relocate median
		//
	//bijective function that maps 3 bools to an  int
	int result = 0;
	if( _LargerEqual_IN )
		result += 4;
	if( _Larger_OUT )
		result += 2;
	if( _MedianOut )
		result += 1;

	//we use an array of function pointers that acts as a look-up table. very fast.
	( *this.*fpLUT_SlideFilter[result] )();

	if( _MedianOut )		// we could also avoid call of one if:P
	{
		//dequeue
		Filter_value.erase( iRemovalPlace );
		Filter_time.pop_front( );
	}
}


							//Automaton that relocates median, in Queue() method:
//---------------------------------- ---------------  ---------------------------------- ---------------
//	MoveMedian_B	MoveMedian_E	| Value>=Median	||	MoveMedian_B	MoveMedian_E	|	Median		|
//--------------- ------------------ ---------------  ---------------- ----------------- ---------------	
//		0				0			|		0		||		1				0			|	stays		|
//		0				0			|		1		||		0				1			|	stays		|
//		0				1			|		0		||		0				0			|	stays		|
//		0				1			|		1		||		0				0			|	->...END	|
//		1				0			|		0		||		0				0			|	BEGIN...<-	|
//		1				0			|		1		||		0				0			|	stays		|
//		1				1			|		0		||			can't be				|				|
//		1				1			|		1		||			can't be				|				|
//---------------------------------- ---------------  ---------------------------------- ---------------
//
//initial values are _MoveMedian_B = _MoveMedian_E = false, when filter length is 1.
//this means that:	-when current filter length is odd _MoveMedian_B = _MoveMedian_E = false
//						and we have real median
//					-when current filter length is even, exactly one of _MoveMedian_B
//						and _MoveMedian_E will be true. median is ambiguous
//							-if _MoveMedian_B == true, there is N+1 filter values smaller than median
//								and N filter valses larger than median
//							-if _MoveMedian_E == true, there is N filter values smaller than median
//								and N+1 filter values larger than median
//so, when we queue a value onto a filter whose current length is odd (and get a filter with
//even length), median stays unchanged
//
//so how exactly this works:
//		we start from a filter of odd length with properly marked median
//												   
//												------------------------			---------------------------------------> filter of ODD length,
//											   | _MoveMedian_B == false |													 with iMedian on the 
//											   | _MoveMedian_E == false |													 median
//												------------ -----------
//															|
//															|	queue(value). two cases can occur:
//															|
//							    ---------------------------- -------------------------
//							   |													  |
//							   |	value < *iMedian								  |	value >= *iMedian
//							   |													  |
//				   ------------ -----------								  ------------ -----------			---------------> filter of EVEN length,
//				  | _MoveMedian_B == true  |							 | _MoveMedian_B == false |							 with iMedian on one 
//				  | _MoveMedian_E == false | don't move iMedian			 | _MoveMedian_E == true  | don't move iMedian		 of the two middle
//			       ------------ -----------								  ------------ -----------							 values
//							   |													  |
//							   |	queue(value). two cases can occur:				  |		queue(value). two cases can occur:
//							   |													  |
//				  ------------- -------------							  ------------ --------------
//				 |							 |							 |							 |
//				 |	value < *iMedian		 |	value >= *iMedian		 |	value < *iMedian		 |	value >= *iMedian
//				 |							 |							 |							 |
//	 ------------ -----------	 ------------ -----------	 ------------ -----------	 ------------ -----------		---> filter of ODD length,
//	| _MoveMedian_B == false |	| _MoveMedian_B == false |	| _MoveMedian_B == false |	| _MoveMedian_B == false |			 with iMedian on the
//	| _MoveMedian_E == false |	| _MoveMedian_E == false |	| _MoveMedian_E == false |	| _MoveMedian_E == false |			 median
//	 ------------------------	 ------------------------	 ------------------------	 ------------------------
//		move iMedian				don't move iMedian			don't move iMedian			move iMedian
//		towards BEGIN																		towards END
//		(to next smaller value)																(to next larger value)
//
//										... and we came back to the case we began from


void CMedianFilter::Queue( unsigned char Value )
{
	multiset_iter iInsertionPlace;

	iInsertionPlace = Filter_value.insert( Value);	//value-link
	Filter_time.push_back( iInsertionPlace );		//time-link
	++CurrentLength;

	//relocate median
	bool _LargerEqual = ( Value >= *iMedian );	//translate Value from upper table into bool
	
		//bijective function
	int result = 0;
	if( _LargerEqual )
		result += 1;
	if( _MoveMedian_E )
		result += 2;
	if( _MoveMedian_B )
		result += 4;

	//call method from LUT
	( *this.*fpLUT_Queue[result] )();

	if( CurrentLength == Length )
	{
		// filter length reached the filter order so we end this transitional period
		fpFeedFilter = SlideFilter;
	}

	return;
}

// called when our multiset is empty; first filter value gets queued and iMedian gets initialized
void CMedianFilter::InitQueue( unsigned char Value )
{
	multiset_iter iInsertionPlace;

	iInsertionPlace = Filter_value.insert( Value);	//value-link
	Filter_time.push_back( iInsertionPlace );		//time-link
	iMedian = ( iInsertionPlace );
	++CurrentLength;

	if( CurrentLength == Length )
	{
		// filter length reached the filter order so we end this transitional period
		fpFeedFilter = SlideFilter;
	}
	else
	{
		// now when iMedian is defined, continue with regular queue
		fpFeedFilter = Queue;
	}

	return;
}


					//Automaton that relocates median, in Dequeue() method:
//
//a bit more complicated than in Queue(), because we have to check if the filter input that
//gets removed is new median.
//for example:
//													
//		* * * * M * * * *	-> value = Dequeue()				(	* ... filter input value
//																	M ... median
//																	left ... smallest, right ... largest	)
//		
//			1A) value < median,	median doesn't get dequeued:
//					* * * M * * * *, median stays unchanged
//
//			1B)	value < median,	median gets dequeued:
//					* * * M * * * *, median moves to LEFT
//										( so both 1A) and 1B) have
//										  N elements on their left and N+1 on their right )
//
//			2A) value >= median, median doesn't get dequeued:
//					* * * * M * * *, median stays unchanged
//
//			2B) value >= median, median gets dequeued:
//					* * * * M * * *, median moves to RIGHT
//										( so both 2A) and 2B) have
//										  N+1 elements on their left and N on their right )
//
//
//the full table looks like this:
//---------------------------------- ------------------- -----------  ---------------------------------- ---------------
//	MoveMedian_B	MoveMedian_E	|	Value>Median	| MDequeued ||	MoveMedian_B	MoveMedian_E	|	Median		|
//--------------- ------------------ ------------------- -----------  ---------------- ----------------- ---------------
//		0				0			|		0			|	0		||		0				1			|	stays		|
//									|					|	1		||									|	BEGIN...<-	|
//		0				0			|		1			|	0		||		1				0			|	stays		|
//									|					|	1		||									|	->...END	|
//		0				1			|		0			|	0		||		0				0			|	->...END	|
//									|					|	1		||									|	->...END	|
//		0				1			|		1			|	0		||		0				0			|	stays		|
//									|					|	1		||									|	->...END	|
//		1				0			|		0			|	0		||		0				0			|	stays		|
//									|					|	1		||									|	BEGIN...<-	|
//		1				0			|		1			|	0		||		0				0			|	BEGIN...<-	|
//									|					|	1		||									|	BEGIN...<-	|
//		1				1			|		0			|	0		||			can't happen			|				|
//									|					|	1		||									|				|
//		1				1			|		1			|	0		||			can't happen			|				|
//									|					|	1		||									|				|
//---------------------------------- ------------------- -----------  ---------------------------------- ---------------
//
//Value is the oldest filter input value that gets dequeued. we check if it's strictly larger than or
//or lesser than or equal to median. why? see example with SlideFilter()
//MDequeued:	0 marks no problems with dequeuing
//				1 marks that input value that should have become median gets dequeued
//
//values _MoveMedian_B and _MoveMedian_E are the same ones used in Queue(), i.e. we don't
//reinitialize them but use their current state instead
//
//similar to Queue():	-when current filter length is odd, _MoveMedian_B = _MoveMedian_E = false
//							and we have real median
//						-when current filter length is even, exactly one of _MoveMedian_B
//							and _MoveMedian_E will be true. median is ambiguous.
//							-if _MoveMedian_B == true, there is N+1 filter values smaller than median
//								and N filter valuse larger than median
//							-if _MoveMedian_E == true, there is N filter values smaller than median
//								and N+1 filter vales larger than median
//
//what's important is that in the implementation median should first be relocated and
//input value erased after that. otherwise, we could lose track of the median.
//

unsigned char CMedianFilter::Dequeue()
{
	multiset_iter iRemovalPlace;
	unsigned char Value;

	iRemovalPlace = *Filter_time.begin();
	Value = *iRemovalPlace;
	
	//relocate median

	bool _Larger = ( Value > *iMedian );
	bool _MDequeued = ( iRemovalPlace == iMedian );

	//bijective function
	int result = 0;
	if( _MDequeued )
		result += 1;
	if( _Larger )
		result += 2;
	if( _MoveMedian_E )
		result += 4;
	if( _MoveMedian_B )
		result += 8;

	//call method from LUT
	( *this.*fpLUT_Dequeue[result] )();

	Filter_value.erase( iRemovalPlace );
	Filter_time.pop_front( );
	--CurrentLength;

	return Value;
}


void CMedianFilter::MedianStays_SlideFilter()
{
	// do nothing
	//std::cout << "Stays \n";
}

void CMedianFilter::MedianToLeft_SlideFilter()
{
	--iMedian;
	//std::cout << "To left \n";
}

void CMedianFilter::MedianToRight_SlideFilter()
{
	++iMedian;
	//std::cout << "To right \n";
}


void CMedianFilter::MedianStays0_Queue()
{
	_MoveMedian_B = true;
	// iMedian stays
}

void CMedianFilter::MedianStays1_Queue()
{
	_MoveMedian_E = true;
	// iMedian stays
}

void CMedianFilter::MedianStays2_Queue()
{
	_MoveMedian_E = false;
	//iMedian stays
}

void CMedianFilter::MedianToLeft3_Queue()
{
	_MoveMedian_E = false;
	++iMedian;
}

void CMedianFilter::MedianToRight4_Queue()
{
	_MoveMedian_B = false;
	--iMedian;
}

void CMedianFilter::MedianStays5_Queue()
{
	_MoveMedian_B = false;
	//iMedian stays
}


void CMedianFilter::MoveMedian0_Dequeue()
{
	_MoveMedian_E = true;
	//iMedian stays
}

void CMedianFilter::MoveMedian1_Dequeue()
{
	_MoveMedian_E = true;
	--iMedian;
}

void CMedianFilter::MoveMedian2_Dequeue()
{
	_MoveMedian_B = true;
	//iMedian stays
}

void CMedianFilter::MoveMedian3_Dequeue()
{
	_MoveMedian_B = true;
	++iMedian;
}

void CMedianFilter::MoveMedian4_Dequeue()
{
	_MoveMedian_E = false;
	++iMedian;
}

void CMedianFilter::MoveMedian5_Dequeue()
{
	_MoveMedian_E = false;
	++iMedian;
}

void CMedianFilter::MoveMedian6_Dequeue()
{
	_MoveMedian_E = false;
	//iMedian;
}

void CMedianFilter::MoveMedian7_Dequeue()
{
	_MoveMedian_E = false;
	++iMedian;
}

void CMedianFilter::MoveMedian8_Dequeue()
{
	_MoveMedian_B = false;
	//++iMedian;
}

void CMedianFilter::MoveMedian9_Dequeue()
{
	_MoveMedian_B = false;
	--iMedian;
}

void CMedianFilter::MoveMedian10_Dequeue()
{
	_MoveMedian_B = false;
	--iMedian;
}

void CMedianFilter::MoveMedian11_Dequeue()
{
	_MoveMedian_B = false;
	--iMedian;
}
