/*#ifndef FAME_H
#define FAME_H

// this is of no use to us, managing one object for each pixel
// TODO: a factory of FAME filters


class CFameFilter
{
private:
	static float minStep;
	static float epsilon;
	float estimate;
	float step;

	void FameInitialization(unsigned char);
	void FameIteration(unsigned char value);
	//pointer to appropriate function for filter update
	void (CFameFilter::*fpFeedFilter)(unsigned char);

public:
	CFameFilter(float minStep);

	void FeedFilter(unsigned char value);
	unsigned char GetEstimate();
	float GetMinStep();
	void SetMinStep(float minStep);
	float GetEpsilon();
	void SetEpsilon(float epsilon);

};

#endif
*/