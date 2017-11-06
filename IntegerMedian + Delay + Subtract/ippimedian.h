#ifndef IPPIMOVDETECTOR_H
	#define IPPIMOVDETECTOR_H

#ifndef IPP_H
	#include <ipp.h>
	#define IPP_H
#endif


/*	a circular buffer of Ipp8us
	bufferLength and currentIndex will be shared by all circular buffers in the 2D matrix of c.buffers
	and set only once, rather than being set by every buffer in the array
*/
class CCircularIppiBuffer
{
public:

	unsigned int maxIndex;	// bufferLength = maxIndex + 1
	IppiSize size;

	CCircularIppiBuffer();
	CCircularIppiBuffer(IppiSize syz, int stepInBytes, unsigned int bufferLength);
	~CCircularIppiBuffer();

	// returns buffer length
	unsigned int GetSize();
	// caller is responsible for providing proper input and output
	void PushBuffer(Ipp8u* input, Ipp8u* output);

protected:
	Ipp8u** ppBuffer;				// a buffer of 2d ippi arrays
	unsigned int currentIndex;
	int stepInBytes;

};



/*	lightweight 2D array of medians on IPP_8u images
	class CIppiMedian_8u_C1 inherits from CCircularIppiBuffer that takes care of the 2D buffer.
*/
class CIppiMedian_8u_C1 : public CCircularIppiBuffer
{

public:

	CIppiMedian_8u_C1();
	CIppiMedian_8u_C1(IppiSize syz, int step, unsigned int bufferLength);
	~CIppiMedian_8u_C1();

	// writes median in the given location provided the new input that goes into the FIFO queue and the
	// output, the value that FIFO pushes out
	void SimulateMedian(	unsigned int* pHistogram,
							const Ipp8u &input,
							const Ipp8u &output,
							Ipp8u* median,
							unsigned int* accumulator);
	// returns the address where the median is stored
	Ipp8u* Push(Ipp8u* pInput);
	// returns the address where the median is stored, modifies pMiddle to point to the half of median window
	// rounded towards the ceiling
	Ipp8u* Push(Ipp8u* pInput, Ipp8u** pMiddle);

protected:
	unsigned int halfBufferLength;
	unsigned int** ppHistogram;
	Ipp8u* pOutput;
	int stride;
	// to avoid cumulative summations at every median updating (pushing in the buffer) we use
	// a faster algorithm that needs to know about the median and the cumulative sum from the last step
	Ipp8u* pMedian;
	unsigned int* pAccumulator;

private:
	Ipp8u** ppMarker;	// we store the beginnings and endings of the rows. rather than using indices when iterating
						// through arrays we will use pointers directly, and markers will tell us when to stop
						// and add IPP stride/padding. as a reference we will use pOutput, because
						// we allocated it and know where it is and never move it anywhere
	unsigned int nrMarkers;
	unsigned int bufferMiddleOffset;

};

#endif