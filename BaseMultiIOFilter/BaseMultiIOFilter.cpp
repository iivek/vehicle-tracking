#pragma warning(disable:4312)

#ifndef BASEMULTIIOFILTER_H
	#include "..\BaseMultiIOFilter\BaseMultiIOFilter.h"
#endif

#ifndef STDIO_H
	#include <stdio.h>
	#define STDIO_H
#endif


CBaseMultiIOFilter :: CBaseMultiIOFilter(TCHAR *pObjectName, LPUNKNOWN lpUnk, CLSID clsid,
	int initialInput, int finalInput, int initialOutput, int finalOutput) :
	CBaseFilter(pObjectName, lpUnk, &m_csMultiIO, clsid)	{

		m_pinCountIn = m_pinCountOut = 0;
		SetNrPins(initialInput, finalInput, initialOutput, finalOutput);
		InitializePins();

		DbgLog((LOG_TRACE,0,TEXT("We're in da konstrukkta")));

}

CBaseMultiIOFilter :: ~CBaseMultiIOFilter()	{

	DbgLog((LOG_TRACE,0,TEXT("We're in destructor")));

	// release (because we AddRef'fed ) all existing pins
	if(!m_inputPins.empty())	{
		INPUT_PINS::iterator iter = m_inputPins.begin();
		do	{
			DbgLog((LOG_TRACE,0,TEXT("%d	"), iter->first));
			(iter++)->second->Release();
			// notice that in Release() we remove the pin from the map
		}	while(iter != m_inputPins.end());
	}

	// release (because we AddRef'fed ) and delete all existing pins
	if(!m_outputPins.empty())	{
		OUTPUT_PINS::iterator iter = m_outputPins.begin(); 
		do	{
			DbgLog((LOG_TRACE,0,TEXT("%d	"), iter->first));
			(iter++)->second->Release();
			// notice that in Release() we remove the pin from the map
		}	while(iter != m_outputPins.end());
	}
	
}


HRESULT CBaseMultiIOFilter::InitializePins()	{

	DbgLog((LOG_TRACE,0,TEXT("CBaseMultiIOFilter::InitializePins()	")));

	for (unsigned int i = 0; i < m_nrInitialIn; ++i)	{
		WCHAR szbuf[20];
		swprintf(szbuf, L"Input%d\0", m_pinCountIn);
		AddInputPin(szbuf);
	}

	for (unsigned int i = 0; i < m_nrInitialOut; ++i)
	{
		WCHAR szbuf[20];
		swprintf(szbuf, L"Output%d\0", m_pinCountOut);
		AddOutputPin(szbuf);
	}

	return S_OK;
}

int CBaseMultiIOFilter::GetPinCount()
{
	return (unsigned int)(m_inputPins.size() + m_outputPins.size());
}


CBasePin* CBaseMultiIOFilter::GetPin( int n )
{
	DbgLog((LOG_TRACE,0,TEXT("getpin")));

	if ((n >= GetPinCount()) || (n < 0))
		return NULL;

	if (n < (int) m_inputPins.size())	{
		// iterate through the map and return the appropriate pin
		INPUT_PINS::iterator iter = m_inputPins.begin();
		int counter = 0;
		while(iter != m_inputPins.end())	{
			if (counter == n)	{
				//return the input pin
				DbgLog((LOG_TRACE,0,TEXT("...returned %d, %d"), n, iter->first));
				return iter->second;
			}
			++counter;
			++iter;
		}
	}
	else
	{
		OUTPUT_PINS::iterator iter = m_outputPins.begin();
		int counter = (int )m_inputPins.size();
		while(iter != m_outputPins.end())	{
			if (counter == n)	{
				//return the output pin
				DbgLog((LOG_TRACE,0,TEXT("...returned %d, %d"), n, iter->first));
				return iter->second;
			}
			++counter;
			++iter;
		}
	}
	return NULL;
}

HRESULT CBaseMultiIOFilter::AddInputPin(const LPCWSTR name)
{
	HRESULT hr = NOERROR;	

	CBaseMultiIOInputPin* pInputPin = new CBaseMultiIOInputPin(
		this, &m_csMultiIO, &hr, name, m_pinCountIn);
	if (FAILED(hr) || pInputPin == NULL) {
		delete pInputPin;
		return hr;
	}

	pInputPin->AddRef();
	IncrementPinVersion();
	m_inputPins[m_pinCountIn++] = pInputPin;
	// ensure enumerator is refreshed

	DbgLog((LOG_TRACE,0,TEXT("InputPin added, %d"), (int)pInputPin->m_pinIndex));

	return hr;
}

HRESULT CBaseMultiIOFilter::AddOutputPin(const LPCWSTR name)
{
	HRESULT hr = NOERROR;	

	CBaseMultiIOOutputPin* pOutputPin = new CBaseMultiIOOutputPin(
		this, &m_csMultiIO, &hr, name, m_pinCountOut);
	if (FAILED(hr) || pOutputPin == NULL) {
		delete pOutputPin;
		return hr;
	}
	pOutputPin->AddRef();
	IncrementPinVersion();
	m_outputPins[m_pinCountOut++] = pOutputPin;
	// ensure enumerator is refreshed

	DbgLog((LOG_TRACE,0,TEXT("OutputPin added, %d"), pOutputPin->m_pinIndex));	

	return hr;
}

void CBaseMultiIOFilter :: SetNrPins(
			int initialInput, int finalInput, int initialOutput, int finalOutput)	{

	m_nrInitialIn = initialInput;
	m_nrInitialOut = initialOutput;
	m_nrFinalIn = finalInput;
	m_nrFinalOut = finalOutput;
	return;
}

HRESULT CBaseMultiIOFilter :: CheckInputType(const CMediaType *pmt)	{

	// by default we support anything
	return NOERROR;
}

HRESULT CBaseMultiIOFilter::CheckOutputType(const CMediaType* pMediaType)	{

	// everything is OK by default
	return NOERROR;
}

// Override to know when an input media type gets set
HRESULT CBaseMultiIOFilter::SetInputType(CBaseMultiIOInputPin* caller, const CMediaType *pmt)	{
	
	return NOERROR;
}

int CBaseMultiIOFilter ::AllInputConnected()	{

	INPUT_PINS::iterator iter = m_inputPins.begin();
	int counter = 0;
	while(iter != m_inputPins.end())	{
		if( iter->second->IsConnected() == TRUE)	{
			++counter;
		}
		++iter;
	}	

	return counter;
}

int CBaseMultiIOFilter :: AllOutputConnected()	{

	OUTPUT_PINS::iterator iter = m_outputPins.begin();
	int counter = 0;
	while(iter != m_outputPins.end())	{	
		if( iter->second->IsConnected() == TRUE)	{
			++counter;
		}
		++iter;
	}

	return counter;
}

HRESULT CBaseMultiIOFilter::OnDisconnect(CBaseMultiIOInputPin* caller)	{

	//CAutoLock lock(&m_csMultiIO);

	// if you want to do something else when a pin gets disconnected, this is the place

	return NOERROR;
}

HRESULT CBaseMultiIOFilter::OnDisconnect(CBaseMultiIOOutputPin* caller)	{

	//CAutoLock lock(&m_csMultiIO);

	// if you want to do something else when a pin gets disconnected, this is the place

	return NOERROR;
}


// we spawn the pins here, if needed
HRESULT CBaseMultiIOFilter::OnConnect(CBaseMultiIOInputPin* caller )	{
	
	DbgLog((LOG_TRACE,0,TEXT("OnConnect")));		

	if(m_inputPins.size() >= m_nrInitialIn && m_inputPins.size()- AllInputConnected() < 1 &&
		m_inputPins.size() < m_nrFinalIn )	{
		// as in inftee sample filter
		WCHAR szbuf[20];             // Temporary scratch buffer
		swprintf(szbuf, L"Input%d\0", m_pinCountIn);

		DbgLog((LOG_TRACE,0,TEXT("OnConnect, AddInputPin")));		
		AddInputPin(szbuf);
		}

	return S_OK;
}

// we spawn the pins here, if needed
HRESULT CBaseMultiIOFilter::OnConnect(CBaseMultiIOOutputPin* caller )	{
	
	if(m_outputPins.size() >= m_nrInitialOut && m_outputPins.size()- AllOutputConnected() < 1 &&
		m_outputPins.size() < m_nrFinalOut )	{
		// as in inftee sample filter
		WCHAR szbuf[20];             // Temporary scratch buffer
		swprintf(szbuf, L"Output%d\0", m_pinCountOut);
		
		DbgLog((LOG_TRACE,0,TEXT("OnConnect, AddOutputPin")));		
		AddOutputPin(szbuf);
	}

	return S_OK;
}

// Override these if you have queued data or a worker thread or your filter does independent
// actions with each input stream
HRESULT CBaseMultiIOFilter::EndOfStream( CBaseMultiIOInputPin* caller )
{
	CAutoLock lock(&m_csReceive);

	HRESULT hr;
	// pass the message downstream via every output pin
	OUTPUT_PINS::iterator iter = m_outputPins.begin();
	while(iter != m_outputPins.end())	{
		CBaseMultiIOOutputPin* pOutputPin = iter->second;

		if(pOutputPin != NULL)	{
			if(pOutputPin->IsConnected())	{
				hr = pOutputPin->DeliverEndOfStream();
				if(FAILED(hr))	{
					return hr;
				}
			}
        }
		++iter;
	}

	return NOERROR;
}

HRESULT CBaseMultiIOFilter::BeginFlush( CBaseMultiIOInputPin* caller )
{
	
    CAutoLock lock(&m_csMultiIO);

	HRESULT hr = NOERROR;

	DbgLog((LOG_TRACE,0,TEXT("in the filter's beginflush: %d")));

	// pass the message downstream via every output pin
	OUTPUT_PINS::iterator iter = m_outputPins.begin();
	while(iter != m_outputPins.end())	{
		CBaseMultiIOOutputPin* pOutputPin = iter->second;

		if(pOutputPin != NULL)	{
			if(pOutputPin->IsConnected())	{

				// we have to be careful here not to call DeliverBeginFlush() on
				// a pin that is already flushing
									DbgLog((LOG_TRACE,0,TEXT("			IsFlushing called! 3")));
				if(pOutputPin->m_pOutputQueue)
				{
					if(!pOutputPin->m_pOutputQueue->IsFlushing())	{
						DbgLog((LOG_TRACE,0,TEXT("nr DBFs: %d"), iter->first));
						hr = pOutputPin->DeliverBeginFlush();
						if(FAILED(hr))
						return hr;	
					}
				}
			}
		}
		++iter;
	}

	return hr;
}

HRESULT CBaseMultiIOFilter::EndFlush( CBaseMultiIOInputPin* caller )	{

    CAutoLock lock(&m_csMultiIO);
	HRESULT hr = NOERROR;

	// pass the message downstream via every output pin
	OUTPUT_PINS::iterator iter = m_outputPins.begin();
	while( iter != m_outputPins.end() )	{

		CBaseMultiIOOutputPin* pOutputPin = iter->second;

		if(pOutputPin != NULL)	{
			if(pOutputPin->IsConnected())	{

				// we have to be careful here not to call DeliverEndFlush() on
				// a pin that is not already flushing
									DbgLog((LOG_TRACE,0,TEXT("			IsFlushing called! 4")));
				if(pOutputPin->m_pOutputQueue)
				{
					if(pOutputPin->m_pOutputQueue->IsFlushing())	{
						DbgLog((LOG_TRACE,0,TEXT("nr DEFs: %d"), iter->first));
						hr = pOutputPin->DeliverEndFlush();
						if(FAILED(hr))
						return hr;	
					}
				}
			}
		} 
		++iter;
	}

	return NOERROR;

}


HRESULT CBaseMultiIOFilter::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate, CBaseMultiIOInputPin* caller )
{
	HRESULT hr = NOERROR;
	// pass the message downstream via every output pin
	OUTPUT_PINS::iterator iter = m_outputPins.begin();
	while( iter != m_outputPins.end())	{

		CBaseMultiIOOutputPin* pOutputPin = iter->second;

		if(pOutputPin != NULL)	{
           hr = pOutputPin->DeliverNewSegment(tStart, tStop, dRate);
		   if(FAILED(hr))
                return hr;
        }
		++iter;
	}

    return hr;
}

// a basic implementation, overload to release interfaces such as a seek interface
void CBaseMultiIOFilter::DeleteInputPin(CBaseMultiIOInputPin *pPin)	{

	CAutoLock lock(&m_csMultiIO);

	ASSERT(!pPin->IsConnected());
	m_inputPins.erase(pPin->m_pinIndex);
	delete pPin;
	IncrementPinVersion();

	DbgLog((LOG_TRACE,0,TEXT("InputPin removed")));
}

// a basic implementation, overload to release interfaces such as a seek interface
void CBaseMultiIOFilter::DeleteOutputPin(CBaseMultiIOOutputPin *pPin)	{

	CAutoLock lock(&m_csMultiIO);

	ASSERT(!pPin->IsConnected());
	m_outputPins.erase(pPin->m_pinIndex);
	delete pPin;
	IncrementPinVersion();

	DbgLog((LOG_TRACE,0,TEXT("OutputPin removed")));
}