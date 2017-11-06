#ifndef IDELAYPROPERTIES_H
#define IDELAYPROPERTIES_H

/* 
	Interface for data transfer with the property pages
*/


// {6774F4B3-7930-401c-87A3-E3B76C55CACD}
DEFINE_GUID(IID_IDelayProperties, 
0x6774f4b3, 0x7930, 0x401c, 0x87, 0xa3, 0xe3, 0xb7, 0x6c, 0x55, 0xca, 0xcd);

// {C26BF02D-FDF9-47e2-BA13-F35F2DA11AFF}
DEFINE_GUID(CLSID_DelayProperties, 
0xc26bf02d, 0xfdf9, 0x47e2, 0xba, 0x13, 0xf3, 0x5f, 0x2d, 0xa1, 0x1a, 0xff);

CLSID_DelayProperties


#ifdef __cplusplus
extern "C" {
#endif

interface IDelayProperties : public IUnknown
{
	// the following methods get called by CTransformPropertyPage methods
	STDMETHOD ( GetDelay )( THIS_ int* pDelay ) PURE;
	STDMETHOD ( SetDelay )( THIS_ int pDelay ) PURE;
};

#ifdef __cplusplus
}
#endif


#endif