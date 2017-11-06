#ifndef MOVINGMEDIAN_H

#ifndef SET_H
	#include <set>
	#define SET_H
#endif

#ifndef LIST_H
	#include <list>
	#define LIST_H
#endif

//multiset is used to keep the filter input data sorted by values. In addition,
//a list is used to keep the filter input data sorted by time moments they appeared in

typedef std::multiset < unsigned char >::iterator	multiset_iter;

class CMedianFilter
{
private:
	//filter lengths
	int Length, CurrentLength;
	// states of the automaton that relocates median
	bool _MoveMedian_B;		// to Beginning
	bool _MoveMedian_E;		// to End

	void SlideFilter( unsigned char value);
	void InitQueue( unsigned char Value );
	void Queue( unsigned char Value );
	unsigned char Dequeue();

	//pointer to appropriate function for filter update
	void ( CMedianFilter::*fpFeedFilter )( unsigned char );

	//function LUTs
	void ( CMedianFilter::*fpLUT_SlideFilter[8] )();
	void ( CMedianFilter::*fpLUT_Queue[6] )();
	void ( CMedianFilter::*fpLUT_Dequeue[12] )();

	//functions from LUTs
	void MedianStays_SlideFilter();
	void MedianToLeft_SlideFilter();
	void MedianToRight_SlideFilter();

	void MedianStays0_Queue();
	void MedianStays1_Queue();
	void MedianStays2_Queue();
	void MedianToLeft3_Queue();
	void MedianToRight4_Queue();
	void MedianStays5_Queue();

	void MoveMedian0_Dequeue();
	void MoveMedian1_Dequeue();
	void MoveMedian2_Dequeue();
	void MoveMedian3_Dequeue();
	void MoveMedian4_Dequeue();
	void MoveMedian5_Dequeue();
	void MoveMedian6_Dequeue();
	void MoveMedian7_Dequeue();
	void MoveMedian8_Dequeue();
	void MoveMedian9_Dequeue();
	void MoveMedian10_Dequeue();
	void MoveMedian11_Dequeue();

public:
	//value link
	std::multiset < unsigned char > Filter_value;
	//time link
	std::list < multiset_iter > Filter_time;
	//iterator for accessing the median
	multiset_iter iMedian;

	//recommended constructor
	CMedianFilter( int length );
	CMedianFilter();
	~CMedianFilter();

	//returns filter order
	int GetFilterLength();
	//returns current filter length
	int GetCurrentLength();
	//sets new filter order. Returns error code -1 if order is 0 or less
	int SetFilterLength( int length );
	//moves filter window to include the next input value
	void FeedFilter( unsigned char value);
};

#endif