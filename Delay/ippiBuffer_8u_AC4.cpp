#include "ippiBuffer_8u_AC4.h"


// for debugging only
#ifndef STREAMS_H
	#include <Streams.h>    // DirectShow (includes windows.h)
	#define STREAMS_H
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
	Ipp8u init[] = {0, 0, 0, 0};
	int change = (int)200/bufferLength;
	for(unsigned int i = 0; i< bufferLength; ++i)
	{
		ppBuffer[i] = ippsMalloc_8u(size.height*stepInBytes);	// how to check for memory shortage from the constructor?
		// initialize it to init
		ippiSet_8u_AC4R(init, ppBuffer[i], stepInBytes, size);
		// just for kicks
		init[0] += change;
		init[1] += change;
		init[2] += change;

		// write out something nice

	}
}

CCircularIppiBuffer::~CCircularIppiBuffer()
{
	DbgLog((LOG_TRACE,0,TEXT("	~CCircularIppiBuffer")));
	for(unsigned int i = 0; i<= maxIndex; ++i)
	{
		DbgLog((LOG_TRACE,0,TEXT("i = %d"), i));
		ippsFree(ppBuffer[i]);
	}
	delete [] ppBuffer;
	ppBuffer = NULL;
}
	
unsigned int CCircularIppiBuffer::GetSize()
{
	return maxIndex+1;
}
	
void CCircularIppiBuffer::PushBuffer(Ipp8u* input, Ipp8u* output)
{
	ippiCopy_8u_AC4R( ppBuffer[currentIndex], stepInBytes, output, stepInBytes, size );
	ippiCopy_8u_AC4R( input, stepInBytes, ppBuffer[currentIndex], stepInBytes, size );
	if(currentIndex <maxIndex)
		++currentIndex;
	else
		currentIndex = 0;
	return;
}