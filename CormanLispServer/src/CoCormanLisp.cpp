//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		CoCormanLisp.cpp
//		Contents:	COM Class definitions for Corman Lisp	COM server.
//		History:	8/5/97  RGC  Created.
//					10/01/16  Artem Boldarev GetCurrentUserName method.
//						 	  global UserInfo object.
//					11/22/16  Artem Boldarev
//							  GetImageLoadsCount() method.
//
#include "Stdafx.h"

#include <windows.h>
#include <initguid.h>
#include <wchar.h>
#include <ole2.h>
#include <olectl.h>
#include <string.h>

#include "CoCormanLisp.h"
#include "clsids.h"
#include "CormanLispServer.h"
#include "CoEnumConnectionPoints.H"
#include "CoConnectionPoint.H"


#include "Lisp.h"
#include "UserInfo.h"

extern void SvcLock();
extern void SvcUnlock();

CoCormanLisp* CormanLispServer = 0;

//////////////////////////////////////////////////////////////////////
// The kernel's calls to its host, passed on to whoever is connected
static void comOutputText(void*, const char* text, long numChars)
{
	CormanLispServer->OutputText((char*)text, numChars);
}

static void comShutdown(void*, const char* text, long numChars)
{
	CormanLispServer->LispShutdown((char*)text, numChars);
}

static void comSetMessage(void*, const char* text)
{
	CormanLispServer->SetMessage((char*)text);
}

static void comGetMessage(void*, char* text, long maxMessageLength)
{
	CormanLispServer->GetMessage(text, maxMessageLength);
}

static void comSetDefaultMessage(void*)
{
	CormanLispServer->SetDefaultMessage();
}

static long comOpenEditWindow(void*, const char* file, void** window)
{
	return CormanLispServer->OpenEditWindow((char*)file, (HWND*)window);
}

static long comOpenURL(void*, const char* file, void** window)
{
	return CormanLispServer->OpenURL((char*)file, (HWND*)window);
}

static long comAddMenu(void*, const char* menuName)
{
	return CormanLispServer->AddMenu((char*)menuName);
}

static long comAddMenuItem(void*, const char* menuName, const char* menuItem)
{
	return CormanLispServer->AddMenuItem((char*)menuName, (char*)menuItem);
}

static void comReplaceSelection(void*, const char* text, long numChars)
{
	CormanLispServer->ReplaceSelection((char*)text, numChars);
}

static void* comAppInstance(void*)
{
	HINSTANCE appInstance = 0;
	CormanLispServer->GetAppInstance(&appInstance);
	return appInstance;
}

static void* comAppMainWindow(void*)
{
	HWND appMainWindow = 0;
	CormanLispServer->GetAppMainWindow(&appMainWindow);
	return appMainWindow;
}

static const CormanLispHost ComHost =
{
	0,
	comOutputText,
	comShutdown,
	comSetMessage,
	comGetMessage,
	comSetDefaultMessage,
	comOpenEditWindow,
	comOpenURL,
	comAddMenu,
	comAddMenuItem,
	comReplaceSelection,
	comAppInstance,
	comAppMainWindow
};

//////////////////////////////////////////////////////////////////////
// ctor and dtor
CoCormanLisp::CoCormanLisp():
pUserInfo(NULL)
{
	m_cRef = 0;
	m_pcpPointSink = 0;
	m_pcpShutdownPointSink = 0;

 	SvcLock();
	CormanLispServer = this;
}

CoCormanLisp::~CoCormanLisp()
{
	UserInfo *info = (UserInfo *)pUserInfo;
	SvcUnlock();
	delete info;
}


//////////////////////////////////////////////////////////////////////
// IUnknown interfaces

STDMETHODIMP CoCormanLisp::QueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_IUnknown)
        *ppv = (ICormanLisp*)this;
    else if (riid == IID_ICormanLisp)
        *ppv = (ICormanLisp*)this;
    else if (riid == IID_IConnectionPointContainer)
        *ppv = (IConnectionPointContainer*)this;
    else if (riid == IID_ICormanLispDirectCall)
        *ppv = (ICormanLispDirectCall*)this;
    else
        *ppv = 0;
    if (*ppv)
        ((IUnknown*)*ppv)->AddRef();
    return *ppv ? S_OK : E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CoCormanLisp::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CoCormanLisp::Release()
{
    if (InterlockedDecrement(&m_cRef) != 0)
        return m_cRef;
    delete this;
    return 0;
}

//////////////////////////////////////////////////////////////////////
// ICormanLisp interface

// client type should be WIN_APP_CLIENT, CONSOLE_CLIENT or IDE_CLIENT
STDMETHODIMP CoCormanLisp::Initialize(IUnknown* clientInterface, 
			const char* lispImage, int clientType)
{
	if (lispImage)
		strncpy_s(LispImageName, LispImageNameMax, lispImage, LispImageNameMax);
	CormanLispClientType = clientType;
	if (pUserInfo == NULL)
	{
		UserInfo *info = new UserInfo;
		if (!UserInfo::FillUserInfo(*info))
		{
			delete info;
			info = NULL;
			return S_FALSE;
		}
		pUserInfo = info;
	}
	InitializeCormanLisp(&ComHost, (UserInfo *)pUserInfo);
	return S_OK;
}

// client type should be WIN_APP_CLIENT, CONSOLE_CLIENT or IDE_CLIENT
STDMETHODIMP CoCormanLisp::InitializeEx(IUnknown* clientInterface, 
			const char* lispImage, int clientType,
            int heapReserve, int heapInitialSize, 
            int ephemeralHeap1Size, int ephemeralHeap2Size)
{
	if (lispImage)
		strncpy_s(LispImageName, LispImageNameMax, lispImage, LispImageNameMax);
	CormanLispClientType = clientType;
	CormanLispSetHeapSizes(heapReserve, heapInitialSize, ephemeralHeap1Size, ephemeralHeap2Size);

	if (pUserInfo == NULL)
	{
		UserInfo *info = new UserInfo;
		if (!UserInfo::FillUserInfo(*info))
		{
			delete info;
			info = NULL;
			return S_FALSE;
		}
		pUserInfo = info;
	}

	InitializeCormanLisp(&ComHost, (UserInfo *)pUserInfo);
	return S_OK;
}

STDMETHODIMP CoCormanLisp::Run(HANDLE* thread)
{
	RunCormanLisp(thread);
	return S_OK;
}

STDMETHODIMP CoCormanLisp::ProcessSource(char* text, long numChars)
{
	ProcessLispSource(text, numChars);
	return S_OK;
}

STDMETHODIMP CoCormanLisp::GetNumThreads(long* numThreads)
{
	*numThreads	= GetNumLispThreads();
	return S_OK;
}

STDMETHODIMP CoCormanLisp::UserException()
{
	::ThrowUserException();
	return S_OK;
}

STDMETHODIMP CoCormanLisp::AbortThread()
{
	::AbortLispThread();
	return S_OK;
}

//////////////////////////////////////////////////////////////////////
// IConnectionPointContainer methods
STDMETHODIMP CoCormanLisp::EnumConnectionPoints(IEnumConnectionPoints** ppEnumConnectionPoints)
{
	*ppEnumConnectionPoints = NULL;
	IConnectionPoint* rgConnectionPoint = (IConnectionPoint*)m_pcpPointSink;

	// Create the enumerator, we have only one connection point
	CoEnumConnectionPoints* pEnum = 
		new CoEnumConnectionPoints((ICormanLisp*)this, 1, 
			&rgConnectionPoint);

	if (!pEnum)
		return E_OUTOFMEMORY;

	pEnum->AddRef();
	*ppEnumConnectionPoints = pEnum;
	
	return S_OK;	
}

STDMETHODIMP 
CoCormanLisp::FindConnectionPoint(REFIID riid, IConnectionPoint** ppConnectionPoint)
{
	*ppConnectionPoint = 0;

	if (riid == IID_ICormanLispTextOutput)
	{
		if (!m_pcpPointSink)
		{
			m_pcpPointSink = new CoConnectionPoint(IID_ICormanLispTextOutput,
			 (IConnectionPointContainer*)this);
			m_pcpPointSink->AddRef();
		}

		m_pcpPointSink->QueryInterface(IID_IConnectionPoint, (void**)ppConnectionPoint);
	}

	else if (riid == IID_ICormanLispShutdown)
	{
		if (!m_pcpShutdownPointSink)
		{
			m_pcpShutdownPointSink = new CoConnectionPoint(IID_ICormanLispShutdown,
			 (IConnectionPointContainer*)this);
			m_pcpShutdownPointSink->AddRef();
		}

		m_pcpShutdownPointSink->QueryInterface(IID_IConnectionPoint, (void**)ppConnectionPoint);
	}

	return *ppConnectionPoint ? S_OK : CONNECT_E_NOCONNECTION;
}

STDMETHODIMP 
CoCormanLisp::SetMessage(char* text)
{
	HRESULT hr = S_OK;
	IEnumConnections* pEnum = 0;
	CONNECTDATA cd = { 0 };
	ICormanLispStatusMessage* messager = 0;

	if (!m_pcpPointSink)
		return hr;

	if (FAILED(m_pcpPointSink->EnumConnections(&pEnum)))
		return hr;

	while (pEnum->Next(1, &cd, NULL) == NOERROR)
	{
		if (SUCCEEDED(cd.pUnk->QueryInterface(IID_ICormanLispStatusMessage, 
				(void**)&messager)))
		{
			hr = messager->SetMessage(text);
			messager->Release();
		}
		cd.pUnk->Release();
	}
	pEnum->Release();
	return hr;
}

STDMETHODIMP 
CoCormanLisp::GetMessage(char* text, long maxMessageLength)
{
	HRESULT hr = S_OK;
	IEnumConnections* pEnum = 0;
	CONNECTDATA cd = { 0 };
	ICormanLispStatusMessage* messager = 0;

	if (!m_pcpPointSink)
		return hr;

	if (FAILED(m_pcpPointSink->EnumConnections(&pEnum)))
		return hr;

	while (pEnum->Next(1, &cd, NULL) == NOERROR)
	{
		if (SUCCEEDED(cd.pUnk->QueryInterface(IID_ICormanLispStatusMessage, 
				(void**)&messager)))
		{
			hr = messager->GetMessage(text, maxMessageLength);
			messager->Release();
		}
		cd.pUnk->Release();
	}
	pEnum->Release();
	return hr;
}

STDMETHODIMP 
CoCormanLisp::SetDefaultMessage()
{
	HRESULT hr = S_OK;
	IEnumConnections* pEnum = 0;
	CONNECTDATA cd = { 0 };
	ICormanLispStatusMessage* messager = 0;

	if (!m_pcpPointSink)
		return hr;

	if (FAILED(m_pcpPointSink->EnumConnections(&pEnum)))
		return hr;

	while (pEnum->Next(1, &cd, NULL) == NOERROR)
	{
		if (SUCCEEDED(cd.pUnk->QueryInterface(IID_ICormanLispStatusMessage, 
				(void**)&messager)))
		{
			hr = messager->SetDefaultMessage();
			messager->Release();
		}
		cd.pUnk->Release();
	}
	pEnum->Release();
	return hr;
}

STDMETHODIMP 
CoCormanLisp::OutputText(char* text, long numChars)
{
	HRESULT hr = S_OK;
	IEnumConnections* pEnum = 0;
	CONNECTDATA cd = { 0 };
	ICormanLispTextOutput* output = 0;

	if (!m_pcpPointSink)
		return hr;

	if (FAILED(m_pcpPointSink->EnumConnections(&pEnum)))
		return hr;

	while (pEnum->Next(1, &cd, NULL) == NOERROR)
	{
		if (SUCCEEDED(cd.pUnk->QueryInterface(IID_ICormanLispTextOutput, 
				(void**)&output)))
		{
			hr = output->OutputText(text, numChars);
			output->Release();
		}
		cd.pUnk->Release();
	}
	pEnum->Release();
	return hr;
}

STDMETHODIMP 
CoCormanLisp::LispShutdown(char* text, long numChars)
{
	HRESULT hr = S_OK;
	IEnumConnections* pEnum = 0;
	CONNECTDATA cd = { 0 };
	ICormanLispShutdown* output = 0;

	if (!m_pcpShutdownPointSink)
		return hr;

	if (FAILED(m_pcpShutdownPointSink->EnumConnections(&pEnum)))
		return hr;

	while (pEnum->Next(1, &cd, NULL) == NOERROR)
	{
		if (SUCCEEDED(cd.pUnk->QueryInterface(IID_ICormanLispShutdown, 
				(void**)&output)))
		{
			hr = output->LispShutdown(text, numChars);
			output->Release();
		}
		cd.pUnk->Release();
	}
	pEnum->Release();
	return hr;
}

STDMETHODIMP CoCormanLisp::GetAppInstance(HINSTANCE* appInstance)
{
	HRESULT hr = S_OK;
	IEnumConnections* pEnum = 0;
	CONNECTDATA cd = { 0 };
	ICormanLispTextOutput* output = 0;

	*appInstance = 0;

	if (!m_pcpPointSink)
		return hr;

	if (FAILED(m_pcpPointSink->EnumConnections(&pEnum)))
		return hr;

	while (pEnum->Next(1, &cd, NULL) == NOERROR)
	{
		if (SUCCEEDED(cd.pUnk->QueryInterface(IID_ICormanLispTextOutput, 
				(void**)&output)))
		{
			hr = output->GetAppInstance(appInstance);
			output->Release();
		}
		cd.pUnk->Release();
	}
	pEnum->Release();
	return hr;
}

STDMETHODIMP CoCormanLisp::GetAppMainWindow(HWND* appMainWindow)
{
	HRESULT hr = S_OK;
	IEnumConnections* pEnum = 0;
	CONNECTDATA cd = { 0 };
	ICormanLispTextOutput* output = 0;

	*appMainWindow = 0;

	if (!m_pcpPointSink)
		return hr;

	if (FAILED(m_pcpPointSink->EnumConnections(&pEnum)))
		return hr;

	while (pEnum->Next(1, &cd, NULL) == NOERROR)
	{
		if (SUCCEEDED(cd.pUnk->QueryInterface(IID_ICormanLispTextOutput, 
				(void**)&output)))
		{
			hr = output->GetAppMainWindow(appMainWindow);
			output->Release();
		}
		cd.pUnk->Release();
	}
	pEnum->Release();
	return hr;
}

STDMETHODIMP CoCormanLisp::OpenEditWindow(char* file, HWND* wnd)
{
	HRESULT hr = S_OK;
	IEnumConnections* pEnum = 0;
	CONNECTDATA cd = { 0 };
	ICormanLispStatusMessage* messager = 0;

	if (!m_pcpPointSink)
		return hr;

	if (FAILED(m_pcpPointSink->EnumConnections(&pEnum)))
		return hr;

	while (pEnum->Next(1, &cd, NULL) == NOERROR)
	{
		if (SUCCEEDED(cd.pUnk->QueryInterface(IID_ICormanLispStatusMessage, 
				(void**)&messager)))
		{
			hr = messager->OpenEditWindow(file, wnd);
			messager->Release();
		}
		cd.pUnk->Release();
	}
	pEnum->Release();
	return hr;
}

STDMETHODIMP CoCormanLisp::OpenURL(char* file, HWND* wnd)
{
	HRESULT hr = S_OK;
	IEnumConnections* pEnum = 0;
	CONNECTDATA cd = { 0 };
	ICormanLispStatusMessage* messager = 0;

	if (!m_pcpPointSink)
		return hr;

	if (FAILED(m_pcpPointSink->EnumConnections(&pEnum)))
		return hr;

	while (pEnum->Next(1, &cd, NULL) == NOERROR)
	{
		if (SUCCEEDED(cd.pUnk->QueryInterface(IID_ICormanLispStatusMessage, 
				(void**)&messager)))
		{
			hr = messager->OpenURL(file, wnd);
			messager->Release();
		}
		cd.pUnk->Release();
	}
	pEnum->Release();
	return hr;
}

STDMETHODIMP CoCormanLisp::AddMenu(char* menuName)
{
	HRESULT hr = S_OK;
	IEnumConnections* pEnum = 0;
	CONNECTDATA cd = { 0 };
	ICormanLispStatusMessage* messager = 0;

	if (!m_pcpPointSink)
		return hr;

	if (FAILED(m_pcpPointSink->EnumConnections(&pEnum)))
		return hr;

	while (pEnum->Next(1, &cd, NULL) == NOERROR)
	{
		if (SUCCEEDED(cd.pUnk->QueryInterface(IID_ICormanLispStatusMessage, 
				(void**)&messager)))
		{
			hr = messager->AddMenu(menuName);
			messager->Release();
		}
		cd.pUnk->Release();
	}
	pEnum->Release();
	return hr;
}

STDMETHODIMP CoCormanLisp::AddMenuItem(char* menuName, char* menuItem)
{
	HRESULT hr = S_OK;
	IEnumConnections* pEnum = 0;
	CONNECTDATA cd = { 0 };
	ICormanLispStatusMessage* messager = 0;

	if (!m_pcpPointSink)
		return hr;

	if (FAILED(m_pcpPointSink->EnumConnections(&pEnum)))
		return hr;

	while (pEnum->Next(1, &cd, NULL) == NOERROR)
	{
		if (SUCCEEDED(cd.pUnk->QueryInterface(IID_ICormanLispStatusMessage, 
				(void**)&messager)))
		{
			hr = messager->AddMenuItem(menuName, menuItem);
			messager->Release();
		}
		cd.pUnk->Release();
	}
	pEnum->Release();
	return hr;
}

STDMETHODIMP 
CoCormanLisp::ReplaceSelection(char* text, long numChars)
{
	HRESULT hr = S_OK;
	IEnumConnections* pEnum = 0;
	CONNECTDATA cd = { 0 };
	ICormanLispStatusMessage* messager = 0;

	if (!m_pcpPointSink)
		return hr;

	if (FAILED(m_pcpPointSink->EnumConnections(&pEnum)))
		return hr;

	while (pEnum->Next(1, &cd, NULL) == NOERROR)
	{
		if (SUCCEEDED(cd.pUnk->QueryInterface(IID_ICormanLispStatusMessage, 
				(void**)&messager)))
		{
			hr = messager->ReplaceSelection(text, numChars);
			messager->Release();
		}
		cd.pUnk->Release();
	}
	pEnum->Release();
	return hr;
}

STDMETHODIMP CoCormanLisp::BlessThread()
{
	::BlessThread();
	return S_OK;
}

STDMETHODIMP CoCormanLisp::UnblessThread()
{
	::UnblessThread();
	return S_OK;
}

STDMETHODIMP CoCormanLisp::GetFunctionAddress(wchar_t* functionName, wchar_t* packageName, void** funcptr)
{
	*funcptr = GetCallbackFunctionPointer(functionName, packageName);
	return S_OK;
}

STDMETHODIMP CoCormanLisp::HandleStructuredException(long exception, LPEXCEPTION_POINTERS info, long* result)
{
	*result = handleStructuredException(exception, info);
	return S_OK;
}

STDMETHODIMP CoCormanLisp::GetCurrentUserName(char *UserName, size_t *len)
{
	if (CurrentUserInfo == NULL || len == NULL)
	{
		return S_FALSE;
	}

	size_t old_len = *len;
	*len = CurrentUserInfo->GetNameLength();
	if (UserName != NULL)
	{
		if (old_len < *len)
			return S_FALSE;

		strcpy_s(UserName, CurrentUserInfo->GetNameLength() + 1, CurrentUserInfo->GetName());
	}

	return S_OK;
}

STDMETHODIMP CoCormanLisp::GetCurrentUserProfileDirectory(char *profileDirectory, size_t *len)
{
	if (CurrentUserInfo == NULL || len == NULL)
	{
		return S_FALSE;
	}

	size_t old_len = *len;
	*len = CurrentUserInfo->GetProfileDirectoryLength();
	if (profileDirectory != NULL)
	{
		if (old_len < *len)
			return S_FALSE;

		strcpy_s(profileDirectory,
			CurrentUserInfo->GetProfileDirectoryLength() + 1,
			CurrentUserInfo->GetProfileDirectory());
	}

	return S_OK;
}

STDMETHODIMP CoCormanLisp::GetCurrentUserPersonalDirectory(char *personalDirectory, size_t *len)
{
	if (CurrentUserInfo == NULL || len == NULL)
	{
		return S_FALSE;
	}

	size_t old_len = *len;
	*len = CurrentUserInfo->GetPersonalDirectoryLength();
	if (personalDirectory != NULL)
	{
		if (old_len < *len)
			return S_FALSE;

		strcpy_s(personalDirectory,
			CurrentUserInfo->GetPersonalDirectoryLength() + 1,
			CurrentUserInfo->GetPersonalDirectory());
	}

	return S_OK;
}

STDMETHODIMP CoCormanLisp::GetImageLoadsCount(LONG *count)
{
	if (count == NULL)
	{
		return S_FALSE;
	}
	
	*count = getImageLoadsCount();

	return S_OK;
}

//////////////////////////////////////////////////////////////////////
// Factory method

extern "C" IUnknown* CreateCormanLisp()
{
    ICormanLisp* p = new CoCormanLisp;
    if (p)
        p->AddRef();
    return p;
}
