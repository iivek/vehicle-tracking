#ifndef PROPERTIES_H
	#define PROPERTIES_H

#ifndef FAME2D_H
	#include "fame2D.h"
#endif

	//_______________________________________________________________________________________
  //			we will derive our property page class from CBasePropertyPage
//______________________________________________________________________


// transform parameters are synchronized with CIPPOpenCVFilter::m_TransformParams
// via IProperties interface.
// they are needed in OnActivate and OnApplyChanges methods
class CFilterPropertyPage : public CBasePropertyPage
{

public:
	// Constructor
	CFilterPropertyPage(LPUNKNOWN lpunk, HRESULT *phr);
	//create instance of our property page
	static CUnknown* WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);
		
	//when property page is activated, deactivated by an application or some changes occured
	HRESULT OnActivate();
	//HRESULT OnDeactivate();
	HRESULT OnApplyChanges();

	HRESULT OnConnect(IUnknown *pUnknown);
    HRESULT OnDisconnect();
	
	BOOL OnReceiveMessage(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam);

	//helper function
	void SetDirty();

private:
	CCritSec PropPageLock;
	IProperties* m_pIProperties;			
	// we store and apply parameters the instant they are changed. when hitting OK, applied
	// parameters should be memorized. when hitting cancel, parameters should revert
	// to values they had the last time property pages were opened (and properties stored).
	// the thing is, i can't make the cancel thing work, so there's no cancelling the changes :P

	//parameters ParametersLastConfirmed;
	Parameters parameters;

	// to set and get data to and from our propertypage
	void SetPropertyPage( Parameters parameters );
	void GetPropertyPage( Parameters* pParameters );
};

#endif