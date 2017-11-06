#ifndef IPPIMOVDETECTOR_H
	#include "ippimedian.h"
#endif


CCircularIppiBuffer::CCircularIppiBuffer() : currentIndex(0)	{}

CCircularIppiBuffer::CCircularIppiBuffer(IppiSize syz, int step, unsigned int bufferLength) :
			currentIndex(0),
			maxIndex(bufferLength-1),
			stepInBytes(step)
	{
	size.width = syz.width;		// never trust the unseen copy constructors
	size.height = syz.height;

	ppBuffer = new Ipp8u*[bufferLength];
	Ipp8u init = 0;
	for(unsigned int i = 0; i< bufferLength; ++i)
	{
		ppBuffer[i] = ippsMalloc_8u(size.height*stepInBytes);	// how to check for memory shortage from the constructor?
		// initialize it to init
		ippiSet_8u_C1R(init, ppBuffer[i], stepInBytes, size);
	}
}

CCircularIppiBuffer::~CCircularIppiBuffer()
{
	for(unsigned int i = 0; i<= maxIndex; ++i)
		ippsFree(ppBuffer[i]);
	delete [] ppBuffer;
}
	
unsigned int CCircularIppiBuffer::GetSize()
{
	return maxIndex+1;
}
	
void CCircularIppiBuffer::PushBuffer(Ipp8u* input, Ipp8u* output)
{
	ippiCopy_8u_C1R( ppBuffer[currentIndex], stepInBytes, output, stepInBytes, size );
	ippiCopy_8u_C1R( input, stepInBytes, ppBuffer[currentIndex], stepInBytes, size );
	if(currentIndex <maxIndex)
		++currentIndex;
	else
		currentIndex = 0;
	return;
}

CIppiMedian_8u_C1::CIppiMedian_8u_C1()
{
	// we should initialize buffers and histogram and everything here... take care not to use this cstor!
	// TODO
}

CIppiMedian_8u_C1::CIppiMedian_8u_C1(IppiSize syz, int step, unsigned int bufferLength) :
		CCircularIppiBuffer(syz, step, bufferLength)
{
	halfBufferLength = (unsigned int) bufferLength/2 + 1;
	unsigned int numPixels = size.height*size.width;

	// to store the matrix that pops out of circular buffer
	pOutput = ippsMalloc_8u(size.height*stepInBytes);
	//setting median to zeros
	pMedian = ippsMalloc_8u(size.height*stepInBytes);
	ippiSet_8u_C1R(0, pMedian, stepInBytes, size);

	pAccumulator = new unsigned int[numPixels];
	stride = stepInBytes - size.width*1;
	ppMarker = new Ipp8u*[size.height*2];
	Ipp8u* pOutputTemp = pOutput;
	Ipp8u** ppMarkerTemp = ppMarker;
	for(int rows=0; rows<size.height; ++rows)
	{
		*ppMarkerTemp = pOutputTemp;
		++ppMarkerTemp;
		pOutputTemp += size.width;
		*ppMarkerTemp = pOutputTemp;
	}
	nrMarkers = size.width*2;
		
	// initializing accumulator
	pAccumulator[0] = bufferLength;
	ppHistogram = new unsigned int*[numPixels];
	for(unsigned int i = 0; i<numPixels; ++i)
	{
		pAccumulator[i] = bufferLength;
		ppHistogram[i] = new unsigned int[256];
		ppHistogram[i][0] = bufferLength;
		for(unsigned int j=1; j<256; ++j)
			ppHistogram[i][j] = 0;
	}

	bufferMiddleOffset = halfBufferLength-1;
}

CIppiMedian_8u_C1::~CIppiMedian_8u_C1()
{
	ippsFree(pOutput);
	ippsFree(pMedian);
	delete ppMarker;
	unsigned int nrPixels = size.height*size.width;
	for(unsigned int i = 0; i<nrPixels; ++i)
	{
		delete ppHistogram[i];
	}
	delete ppHistogram;
	delete pAccumulator;
}


// writes *median in the given location provided the new input that goes into the FIFO queue and the
// output, the value that FIFO pushes out
void CIppiMedian_8u_C1::SimulateMedian(	unsigned int* pHistogram,
									const Ipp8u &input,
									const Ipp8u &output,
									Ipp8u* median,
									unsigned int* accumulator)	{
	--pHistogram[output];
	++pHistogram[input];
	// find *median using its probability property as indicator

	// the point is that each push operation can raise or reduce *accumulator by 1 at most. the *median can
	// shift to the nex
	bool inGreaterThanMedian	= input > *median;
	bool outGreaterThanMedian	= output > *median;

	if(input == output)
		return;

		if(!inGreaterThanMedian && !outGreaterThanMedian)
		{
		if(input == *median)
		{
			// *accumulator stays the same, recall that if we're here input != output
			return;
		}
		if(output == *median)
		{
			if(*accumulator - pHistogram[*median] >= halfBufferLength)
			{
				// if we're here this neccessarily means that output == *median
				*accumulator -= pHistogram[*median];
				while(pHistogram[--(*median)] == 0);	// move *median to the first nonzero histogram value towards 0
			}
		}
		return;
	}
	if(inGreaterThanMedian && outGreaterThanMedian)
	{
		return;
	}
	if(inGreaterThanMedian && !outGreaterThanMedian)
	{
		--(*accumulator);
		if(*accumulator >= halfBufferLength)
		{
			return;
		}
		while(pHistogram[++(*median)] == 0);	// move *median to the first nonzero histogram value towards 255
		*accumulator += pHistogram[*median];
		return;
	}

	if(!inGreaterThanMedian && outGreaterThanMedian)
	{
		++(*accumulator);
		if(*accumulator - pHistogram[*median] >= halfBufferLength)
		{
			*accumulator -= pHistogram[*median];
			while(pHistogram[--(*median)] == 0);	// move *median to the first nonzero histogram value towards 0
		}
		return;
	}
	/*
	// findinf median by cumulative summation
	*median = -1;	//-1 is ok for an Ipp8u
	*accumulator = 0;
	while(*accumulator < halfBufferLength)
		*accumulator += pHistogram[++(*median)];
	return;
	*/		
}

// returns the address where the median is stored
Ipp8u* CIppiMedian_8u_C1::Push(Ipp8u* pInput)
{
	PushBuffer(pInput, pOutput);

	Ipp8u** ppMarkerTemp = ppMarker;
	unsigned int** ppHistogramTemp = ppHistogram;
	Ipp8u* pInputTemp = pInput;
	Ipp8u* pOutputTemp = pOutput;
	unsigned int* pAccumulatorTemp = pAccumulator;
	Ipp8u* pMedianTemp = pMedian;

//	ASSERT(size.width != 0);	// this won't happen but check anyway. we should have checked in the constructor also, before we've had done all this.
	for(int row = 0; row<size.height; ++row)
	{
		for(++ppMarkerTemp; *ppMarkerTemp != pOutputTemp; )
		{
			SimulateMedian(	*ppHistogramTemp,
							*(pInputTemp),
							*(pOutputTemp),
							pMedianTemp,
							pAccumulatorTemp
							);
			++pOutputTemp;
			++ppHistogramTemp;
			++pInputTemp;
			++pAccumulatorTemp;
			++pMedianTemp;
		}
		// those allocated with IPP need to have strides at the end of each row
		pOutputTemp += stride;
		pInputTemp +=stride;
		pMedianTemp +=stride;
	}	

	return pMedian;
}

Ipp8u* CIppiMedian_8u_C1::Push(Ipp8u* pInput, Ipp8u** pMiddle) 
{
	PushBuffer(pInput, pOutput);

	Ipp8u** ppMarkerTemp = ppMarker;
	unsigned int** ppHistogramTemp = ppHistogram;
	Ipp8u* pInputTemp = pInput;
	Ipp8u* pOutputTemp = pOutput;
	unsigned int* pAccumulatorTemp = pAccumulator;
	Ipp8u* pMedianTemp = pMedian;

//	ASSERT(size.width != 0);	// this won't happen but check anyway. we should have checked in the constructor also, before we've had done all this.
	for(int row = 0; row<size.height; ++row)
	{
		for(++ppMarkerTemp; *ppMarkerTemp != pOutputTemp; )
		{
			SimulateMedian(	*ppHistogramTemp,
							*(pInputTemp),
							*(pOutputTemp),
							pMedianTemp,
							pAccumulatorTemp
							);
			++pOutputTemp;
			++ppHistogramTemp;
			++pInputTemp;
			++pAccumulatorTemp;
			++pMedianTemp;
		}
		// those allocated with IPP need to have strides at the end of each row
		pOutputTemp += stride;
		pInputTemp +=stride;
		pMedianTemp +=stride;
	}	

	*pMiddle = ppBuffer[(currentIndex+bufferMiddleOffset)%GetSize()];
	return pMedian;
}