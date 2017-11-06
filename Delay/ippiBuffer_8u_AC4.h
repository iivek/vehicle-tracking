#ifndef IPPIBUFFER_8U_AC4_H
	#define IPPIBUFFER_8U_AC4_H

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



#endif