#include "PluginStdAfx.h"

#include "PluginClient.h"
#include "PluginSettings.h"
#include "PluginDictionary.h"
#ifdef SUPPORT_CONFIG
#include "PluginConfig.h"
#endif
#include "PluginTab.h"
#include "PluginDomTraverser.h"
#include "PluginClass.h"

#include "PluginTabBase.h"


int CPluginTabBase::s_dictionaryVersion = 0;
int CPluginTabBase::s_settingsVersion = 1;
#ifdef SUPPORT_FILTER
int CPluginTabBase::s_filterVersion = 0;
#endif
#ifdef SUPPORT_WHITELIST
int CPluginTabBase::s_whitelistVersion = 0;
#endif
#ifdef SUPPORT_CONFIG
int CPluginTabBase::s_configVersion = 0;
#endif


CPluginTabBase::CPluginTabBase(CPluginClass* plugin) : m_plugin(plugin), m_isActivated(false)
{
	//There are no tabs in IE6 and lower, so we won't get the tab activated otherwise.
	//TODO: O.P. In fact it looks like we should create tab already activated for all browsers. Investigate this.
    CPluginClient* client = CPluginClient::GetInstance();
	if (client->GetIEVersion() < 10)
	{
		m_isActivated = true;
	}

    DWORD id;
	m_hThread = ::CreateThread(NULL, 0, ThreadProc, (LPVOID)this, CREATE_SUSPENDED, &id);
	if (m_hThread)
	{
		m_isThreadDone = false;
		::ResumeThread(m_hThread);
	}
	else
	{
		DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_THREAD, PLUGIN_ERROR_TAB_THREAD_CREATE_PROCESS, "Tab::Thread - Failed to create tab thread");
	}

#ifdef SUPPORT_DOM_TRAVERSER
	m_traverser = new CPluginDomTraverser(static_cast<CPluginTab*>(this));
#endif // SUPPORT_DOM_TRAVERSER
}


CPluginTabBase::~CPluginTabBase()
{
#ifdef SUPPORT_DOM_TRAVERSER
	delete m_traverser;
	m_traverser = NULL;
#endif // SUPPORT_DOM_TRAVERSER

	// Close down thread
	if (m_hThread != NULL)
	{
	    m_isThreadDone = true;

		::WaitForSingleObject(m_hThread, INFINITE);
		::CloseHandle(m_hThread);
	}
}

void CPluginTabBase::OnActivate()
{
	m_isActivated = true;
}


void CPluginTabBase::OnUpdate()
{
	m_isActivated = true;
}


bool CPluginTabBase::OnUpdateSettings(bool forceUpdate)
{
	bool isUpdated = false;

	CPluginSettings* settings = CPluginSettings::GetInstance();

    int newSettingsVersion = settings->GetTabVersion(SETTING_TAB_SETTINGS_VERSION);
    if ((s_settingsVersion != newSettingsVersion) || (forceUpdate))
    {
        s_settingsVersion = newSettingsVersion;
        if (!settings->IsMainProcess())
        {
            settings->Read();

			isUpdated = true;
        }
    }

	return isUpdated;
}

bool CPluginTabBase::OnUpdateConfig()
{
	bool isUpdated = false;

#ifdef SUPPORT_CONFIG
	CPluginSettings* settings = CPluginSettings::GetInstance();

	int newConfigVersion = settings->GetTabVersion(SETTING_TAB_CONFIG_VERSION);
    if (s_configVersion != newConfigVersion)
    {
        s_configVersion = newConfigVersion;

#ifdef SUPPORT_DOM_TRAVERSER
#ifdef PRODUCT_DOWNLOADHELPER
		m_traverser->UpdateConfig();
#endif
#endif
        isUpdated = true;
	}

#endif // SUPPORT_CONFIG

	return isUpdated;
}


void CPluginTabBase::OnNavigate(const CString& url)
{
	SetDocumentUrl(url);


#ifdef SUPPORT_FRAME_CACHING
	ClearFrameCache(GetDocumentDomain());
#endif

#ifdef SUPPORT_DOM_TRAVERSER
	m_traverser->ClearCache();
#endif
}

void CPluginTabBase::OnDownloadComplete(IWebBrowser2* browser)
{
#ifdef SUPPORT_DOM_TRAVERSER
//	m_traverser->TraverseDocument(browser, GetDocumentDomain(), GetDocumentUrl());
#endif // SUPPORT_DOM_TRAVERSER
}

void CPluginTabBase::OnDocumentComplete(IWebBrowser2* browser, const CString& url, bool isDocumentBrowser)
{
	CString documentUrl = GetDocumentUrl();

	if (isDocumentBrowser)
	{
		if (url != documentUrl)
		{
			SetDocumentUrl(url);
		}
	}

#ifdef SUPPORT_DOM_TRAVERSER
    if (url.Left(6) != "res://")
    {
		//changes start here
		// Get document
		CComPtr<IDispatch> pDocDispatch;
		HRESULT hr = browser->get_Document(&pDocDispatch);
		if (FAILED(hr) || !pDocDispatch)
		{
			return;
		}

		CComQIPtr<IHTMLDocument2> pDoc = pDocDispatch;
		if (!pDoc)
		{
			return;
		}

		CComPtr<IHTMLStyleSheet> pNewStyleSheet;
		hr = pDoc->createStyleSheet(CComBSTR(CPluginSettings::GetDataPath(L"filter1.txt.css")), 30, &pNewStyleSheet);


		CComPtr<IHTMLFramesCollection2> pFramesCollection;
		pDoc->get_frames(&pFramesCollection);
		long length = 0;
		pFramesCollection->get_length(&length);
//		IHTMLFramesCollection2* pTopFramesCollection = pFramesCollection;
//		IHTMLFramesCollection2* pPrevFramesCollection = NULL;
		int stoppedIndex = -1;
		if (length > 0)
		{
			int i = 0;
			CComVariant index(i);
			CComVariant result;
			while (pFramesCollection->item(&index, &result) == S_OK)
			{
				CComPtr<IHTMLWindow2> element;
				result.pdispVal->QueryInterface(IID_IHTMLWindow2, (void**)&element);

				if (element != NULL)
				{
					CComPtr<IHTMLDocument2> pFrameDoc;
					element->get_document(&pFrameDoc);
					if (pFrameDoc != NULL)
					{
						CComPtr<IHTMLStyleSheet> pNewStyleSheet2;
						pFrameDoc->createStyleSheet(CComBSTR(CPluginSettings::GetDataPath(L"filter1.txt.css")), 30, &pNewStyleSheet2);
//						IHTMLFramesCollection2* pFramesCollection2;
//						pFrameDoc->get_frames(&pFramesCollection2);
//						long length = 0;
//						pFramesCollection->get_length(&length);
//						if (length > 0)
//						{
//							pPrevFramesCollection = pTopFramesCollection;
//							pTopFramesCollection = pFramesCollection2;
//							stoppedIndex = i;
//						}
//						else
//						{
//							pFramesCollection2->Release();
//							pFramesCollection2 = NULL;
//						}
					}
					element.Release();
				}
				CComVariant newIndex(i);
				index = newIndex;
				i++;
			}
		}
		if (pFramesCollection != NULL)
		{
			pFramesCollection.Release();
		}
		pDoc.Release();
		pDocDispatch.Release();

/*		// changes end here
		if (url == documentUrl)
		{
			m_traverser->TraverseDocument(browser, GetDocumentDomain(), url);
		}
		else
		{
			m_traverser->TraverseSubdocument(browser, GetDocumentDomain(), url);
		}				
*/		
    }
#endif
}

CString CPluginTabBase::GetDocumentDomain()
{
    CString domain;

    m_criticalSection.Lock();
    {
        domain = m_documentDomain;
    }
    m_criticalSection.Unlock();

    return domain;
}

void CPluginTabBase::SetDocumentUrl(const CString& url)
{
	CString domain;

    m_criticalSection.Lock();
    {
        m_documentUrl = url;
	    m_documentDomain = CPluginClient::ExtractDomain(url);
    	
	    domain = m_documentDomain;
    }
    m_criticalSection.Unlock();

#ifdef SUPPORT_WHITELIST
	CPluginSettings::GetInstance()->AddDomainToHistory(domain);
#endif
}

CString CPluginTabBase::GetDocumentUrl()
{
    CString url;

    m_criticalSection.Lock();
    {
        url = m_documentUrl;
    }
    m_criticalSection.Unlock();

    return url;
}


// ============================================================================
// Frame caching
// ============================================================================

#ifdef SUPPORT_FRAME_CACHING

bool CPluginTabBase::IsFrameCached(const CString& url)
{
    bool isFrame;
    
    m_criticalSectionCache.Lock();
    {
        isFrame = m_cacheFrames.find(url) != m_cacheFrames.end();
    }
    m_criticalSectionCache.Unlock();
    
    return isFrame;
}

void CPluginTabBase::CacheFrame(const CString& url)
{
    m_criticalSectionCache.Lock();
    {
        m_cacheFrames.insert(url);
    }
    m_criticalSectionCache.Unlock();
}

void CPluginTabBase::ClearFrameCache(const CString& domain)
{
    m_criticalSectionCache.Lock();
    {
        if (domain.IsEmpty() || domain != m_cacheDomain)
        {
            m_cacheFrames.clear();
            m_cacheDomain = domain;
        }
	}
    m_criticalSectionCache.Unlock();
}

#endif // SUPPORT_FRAME_CACHING


DWORD WINAPI CPluginTabBase::ThreadProc(LPVOID pParam)
{
	CPluginTab* tab = static_cast<CPluginTab*>(pParam);

    // Force loading/creation of settings
    CPluginSettings* settings = CPluginSettings::GetInstance();

    settings->SetWorkingThreadId();

    CString threadInfo;
    threadInfo.Format(L"%d.%d", ::GetCurrentProcessId(), ::GetCurrentThreadId());
    
    CString debugText;
    
    debugText += L"================================================================================";
    debugText += L"\nTAB THREAD " + threadInfo;
    debugText += L"\n================================================================================";

    DEBUG_GENERAL(debugText)

	CPluginClient* client = CPluginClient::GetInstance();
        
    // Force loading/creation of dictionary
    CPluginDictionary::GetInstance();

	client->SetLocalization();

    // Force loading/creation of config
#ifdef SUPPORT_CONFIG
    CPluginConfig* config = CPluginConfig::GetInstance();

#ifdef SUPPORT_DOM_TRAVERSER
#ifdef PRODUCT_DOWNLOADHELPER
	tab->m_traverser->UpdateConfig();
#endif // PRODUCT_DOWNLOADHELPER
#endif // SUPPORT_DOM_TRAVERSER

#endif // SUPPORT_CONFIG

	// --------------------------------------------------------------------
	// Tab loop
	// --------------------------------------------------------------------

	DWORD loopCount = 0;
	DWORD tabLoopIteration = 1;
			
	while (!tab->m_isThreadDone)
	{
#ifdef ENABLE_DEBUG_THREAD
	    CStringA sTabLoopIteration;
	    sTabLoopIteration.Format("%u", tabLoopIteration);
	    
        DEBUG_THREAD("--------------------------------------------------------------------------------")
        DEBUG_THREAD("Loop iteration " + sTabLoopIteration);
        DEBUG_THREAD("--------------------------------------------------------------------------------")
#endif
        // Update settings from file
        if (tab->m_isActivated)
        {
            bool isChanged = false;

            settings->RefreshTab();

			tab->OnUpdateSettings(false);

            int newDictionaryVersion = settings->GetTabVersion(SETTING_TAB_DICTIONARY_VERSION);
            if (s_dictionaryVersion != newDictionaryVersion)
            {
                s_dictionaryVersion = newDictionaryVersion;
                client->SetLocalization();
                isChanged = true;
            }

			isChanged = tab->OnUpdateConfig() ? true : isChanged;

#ifdef SUPPORT_WHITELIST
            int newWhitelistVersion = settings->GetTabVersion(SETTING_TAB_WHITELIST_VERSION);
            if (s_whitelistVersion != newWhitelistVersion)
            {
                s_whitelistVersion = newWhitelistVersion;
                settings->RefreshWhitelist();
                client->ClearWhiteListCache();
                isChanged = true;
            }
#endif // SUPPORT_WHITELIST

#ifdef SUPPORT_FILTER
            int newFilterVersion = settings->GetTabVersion(SETTING_TAB_FILTER_VERSION);
            if (s_filterVersion != newFilterVersion)
            {
                s_filterVersion = newFilterVersion;
                client->ReadFilters();
                isChanged = true;
            }
#endif
            if (isChanged)
            {
                tab->m_plugin->UpdateStatusBar();
            }

            tab->m_isActivated = false;
        }

	    // --------------------------------------------------------------------
	    // End loop
	    // --------------------------------------------------------------------

	    // Sleep loop
	    while (!tab->m_isThreadDone && !tab->m_isActivated && (++loopCount % (TIMER_THREAD_SLEEP_TAB_LOOP / 50)) != 0)
	    {
            // Post async plugin error
            CPluginError pluginError;
            if (CPluginClient::PopFirstPluginError(pluginError))
            {
                CPluginClient::LogPluginError(pluginError.GetErrorCode(), pluginError.GetErrorId(), pluginError.GetErrorSubid(), pluginError.GetErrorDescription(), true, pluginError.GetProcessId(), pluginError.GetThreadId());
            }

		    // Non-hanging sleep
		    Sleep(50);
	    }
	    
	    tabLoopIteration++;
	}

	return 0;
}
