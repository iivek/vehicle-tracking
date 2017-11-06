#ifndef TEST_H
#define TEST_H

#ifndef BASEMULTIIOFILTER_H
	#include "BaseMultiIOFilter.h"
#endif

#ifndef INITGUID_H
	#include <initguid.h>   // declares DEFINE_GUID to declare an EXTERN_C const.
	#define INITGUID_H
#endif


#define FILTER_NAME L"TEST"

// {D297096A-D507-4808-AA72-525864F53FF9}
DEFINE_GUID(CLSID_TEST, 
0xd297096a, 0xd507, 0x4808, 0xaa, 0x72, 0x52, 0x58, 0x64, 0xf5, 0x3f, 0xf9);

// {2DFA1B73-816E-46de-8226-382DB230BFFE}
DEFINE_GUID(CLSID_MotionTrackingCategory, 
0x2dfa1b73, 0x816e, 0x46de, 0x82, 0x26, 0x38, 0x2d, 0xb2, 0x30, 0xbf, 0xfe);

// {C7654E95-DF70-47d9-BE15-42206C0F6DAC}
DEFINE_GUID(MEDIASUBTYPE_8BitMonochrome_internal, 
0xc7654e95, 0xdf70, 0x47d9, 0xbe, 0x15, 0x42, 0x20, 0x6c, 0xf, 0x6d, 0xac);



class CTest : CBaseMultiIOFilter	{

public:

	CMediaType* outputMediaType;

	CTest( TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr ) :
	  CBaseMultiIOFilter( tszName, punk, CLSID_TEST, 2, 5, 1, 1 ){
			// the only output media subtype is MEDIASUBTYPE_8BitMonochrome_internal...
			// Our output MediaType is defined as follows:
			  outputMediaType = new CMediaType();
			  outputMediaType->InitMediaType();
			  outputMediaType->AllocFormatBuffer(sizeof(FORMAT_VideoInfo));

              outputMediaType->SetType(&MEDIATYPE_Video);
			  //outputMediaType->SetSubtype(&MEDIASUBTYPE_8BitMonochrome_internal);
			  outputMediaType->SetSubtype(&MEDIASUBTYPE_RGB24);
			  outputMediaType->SetFormatType(&FORMAT_VideoInfo);
			  outputMediaType->bFixedSizeSamples = TRUE;
			  outputMediaType->SetTemporalCompression(FALSE);
			  outputMediaType->cbFormat = sizeof(VIDEOINFOHEADER);
	  
	  };
	~CTest(void){}


	HRESULT DecideBufferSize(
		IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pRequestProperties, int nIndex);
	HRESULT GetMediaType(int iPosition, CMediaType* pmt, int nOutputPinIndex);
	HRESULT CheckInputType( const CMediaType *mtIn );

	HRESULT Receive(IMediaSample *pSample, int nIndex );

	static CUnknown* WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

protected:
	// ALLOCATOR_PROPERTIES
	ALLOCATOR_PROPERTIES inputAllocProps;
	ALLOCATOR_PROPERTIES outputAllocProps;

private:
	int stride_8u_C1, stride_8u_C3, stride_32f_C1;
	int bytesperpixel_8u_C1;
	int bytesperpixel_8u_C3;
	int step_8u_C1, step_8u_C3, step_32f_C1;
	static const int IPPalignment = 32;
};


#endif