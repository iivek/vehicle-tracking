#ifndef ALLOCATORS_H

//#ifndef ALLOCATORTEMPLATES_H
	#include "..\allocatorTemplates.h"
	#define ALLOCATORTEMPLATES_H
//#endif

#ifndef Y2RENDERER_H
	#include "y2renderer.h"
#define Y2RENDERER_H
#endif


class CIPPAllocator_8u_C1 : public TIPPAllocator_8u_C1<CYToRenderer>	{

	friend class CPinInput;

public:
	CIPPAllocator_8u_C1(CTransformFilter* pFilter,TCHAR * pName, LPUNKNOWN pUk, HRESULT * pHr) :
	TIPPAllocator_8u_C1<CYToRenderer>(pFilter, pName, pUk, pHr){	};
};

class CIPPAllocator_8u_C3 : public TIPPAllocator_8u_C3<CYToRenderer>	{

	friend class CPinOutput;

public:
	CIPPAllocator_8u_C3(CTransformFilter* pFilter,TCHAR * pName, LPUNKNOWN pUk, HRESULT * pHr) :
		TIPPAllocator_8u_C3<CYToRenderer>(pFilter, pName, pUk, pHr){	};

};


#endif