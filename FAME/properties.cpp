#include "properties.h"
#include "resource.h"
#include "stdio.h"
#include <tchar.h>


//helper
void CFilterPropertyPage :: SetDirty()
{
	m_bDirty = TRUE;
	if (m_pPageSite)
	{
		//m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);	// apply button will always be disabled
	}
}

void CFilterPropertyPage :: SetPropertyPage( Parameters parameters )
{
	TCHAR   sz[STR_MAX_LENGTH];

	_stprintf( sz, TEXT("%d"), parameters.minstep );
	SetDlgItemText( m_Dlg, IDC_EDIT1, sz );
	_stprintf( sz, TEXT("%.1f"), parameters.epsilon );
	SetDlgItemText( m_Dlg, IDC_EDIT2, sz );
}


void CFilterPropertyPage :: GetPropertyPage( Parameters* pParameters )
{
	TCHAR   sz[STR_MAX_LENGTH];
	TCHAR	sz_clear[STR_MAX_LENGTH];
	_stprintf( sz_clear, TEXT("0"), 0 );
	
	GetDlgItemText( m_Dlg, IDC_EDIT1, sz, STR_MAX_LENGTH );
	if( float( atof( sz ) ) < 0 )
	{
		SetDlgItemText( m_Dlg, IDC_EDIT1, sz_clear );
		return;
	}
	pParameters->minstep = atof(sz);

	GetDlgItemText( m_Dlg, IDC_EDIT2, sz, STR_MAX_LENGTH );
	if( float( atof( sz ) ) < 0 )
	{
		SetDlgItemText( m_Dlg, IDC_EDIT2, sz_clear );
		return;
	}
	pParameters->epsilon = atof(sz);

	return;
}


//CBasePropertyPage constructor with two resource IDs as arguments
CFilterPropertyPage :: CFilterPropertyPage( LPUNKNOWN pUnk, HRESULT *phr ) :
					CBasePropertyPage( NAME( "Property Page" ),
					pUnk, IDD_PROPPAGE_SMALL, IDD_TITLE ),
					m_pIProperties( NULL )
{
    ASSERT(phr);
}

// we return a pointer to our filter interface
HRESULT CFilterPropertyPage :: OnConnect( IUnknown *pUnknown )
{
	ASSERT( m_pIProperties == NULL );
	
	HRESULT hr = pUnknown->QueryInterface( IID_IProperties, ( void** )&m_pIProperties );
	if (FAILED(hr))
	{
		return E_NOINTERFACE;
    }
	if (pUnknown == NULL)
    {
        return E_POINTER;
    }

	//m_pIProperties->GetTransformParams( &ParametersLastConfirmed );
	//ASSERT(&ParametersLastConfirmed != NULL);

	return NOERROR;
}

// Initialize the dialog
HRESULT CFilterPropertyPage::OnActivate()
{
	m_pIProperties->GetParameters( &parameters );
	SetPropertyPage( parameters );

	return NOERROR;
}


// Called when a property is changed
BOOL CFilterPropertyPage::OnReceiveMessage( HWND hwnd,
									UINT uMsg,
									WPARAM wParam,
									LPARAM lParam )		{
	// all the IProperties methods are locked internally, so we don't have to lock
	// the section here

	BOOL flag = TRUE;

	DbgLog((LOG_TRACE,1,TEXT("OnReceiveMessage, nr.1")));

	switch( uMsg )
	{
	case WM_INITDIALOG:  // init property page
		break;

	case WM_COMMAND:
		{
			// we don't treat each object separately nor do we analyze type of WM_COMMAND

			// if Reset Tracker Button pushed
			if( LOWORD(wParam) == IDC_BUTTON1 )
			{

				CheckDlgButton( m_Dlg, IDC_BUTTON1,	BST_CHECKED );
				m_pIProperties->ResetFilter();
				// dialog object deactivates automatically
				flag = FALSE;
				CheckDlgButton( m_Dlg, IDC_BUTTON1,	BST_UNCHECKED );
				break;
			}

			// if Revert to Defaults Button pushed
			if( LOWORD(wParam) == IDC_BUTTON2 )
			{
				m_pIProperties->RevertToDefaults();
				SetPropertyPage( parameters );
				// dialog object deactivates automatically
				flag = FALSE;
				CheckDlgButton( m_Dlg, IDC_BUTTON2,	BST_UNCHECKED );
			}
	
			if( flag )
			{
				GetPropertyPage( &parameters );									
				m_pIProperties->SetParameters( parameters );

				SetDirty();
			}
			m_pIProperties->PersistSetDirty( TRUE );	// flag for CPersistStream to save to stream
			return ( LRESULT )1;
		}
	}

	DbgLog((LOG_TRACE,1,TEXT("OnReceiveMessage, nr.2")));

	return CBasePropertyPage::OnReceiveMessage( hwnd, uMsg, wParam, lParam );
}


// commit changes in properties. inside, we set output function pointer.
HRESULT CFilterPropertyPage :: OnApplyChanges()
{
	if( m_bDirty )	{	
	// commit parameters by IProperties interface methods
	//	m_pIProperties->GetTransformParams( &ParametersLastConfirmed );
		m_bDirty = FALSE;	// not dirty anymore
	}
   	return NOERROR;
}

// We are being deactivated
//HRESULT CFilterPropertyPage :: OnDeactivate( void )
//{
//		sleep
//
//	return NOERROR;
//}



// Cleanup.
HRESULT CFilterPropertyPage::OnDisconnect(void)
{
//	can't figure out why this won't work... guess there won't be no cancel functionality
//	if( m_bDirty)	//means that cancel button has been hit
//	{
//		m_pIProperties->SetTransformParams( ParametersLastConfirmed );
		// return display function pointer as it has been before opening property pages
//		m_pIProperties->SetDisplayFunction( ParametersLastConfirmed.OutputFunction_val );
//	}

	// release IProperties interface before disconnecting the property page
	if( m_pIProperties == NULL )
	{
		return E_UNEXPECTED;
	}
	m_pIProperties->Release();
	m_pIProperties = NULL;

	return S_OK;
}


// CreateInstance - COM object
CUnknown *CFilterPropertyPage::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
    CUnknown *punk = new CFilterPropertyPage( lpunk, phr );
    if (punk == NULL)
	{
	  *phr = E_OUTOFMEMORY;
    }
    return punk;
}