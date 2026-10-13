//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		ComServer.cpp
//		Contents:	The kernel as a COM server: its class factory and its
//					entries in the registry.
//

#include "stdafx.h"
#include <windows.h>
#include <ole2.h>
#include <string.h>

#include "clsids.h"
#include "CoCormanLisp.h"

extern TCHAR g_szFileName[MAX_PATH];

const TCHAR *const g_szRegKeyValues[][2] =
{
  { __TEXT("CormanLisp.Lisp"), __TEXT("Lisp") },
  { __TEXT("CormanLisp.Lisp\\CLSID"), __TEXT("{99AA27B3-0EAE-11d1-ACB9-00A024803258}") },
  { __TEXT("CLSID\\{99AA27B3-0EAE-11d1-ACB9-00A024803258}"), __TEXT("Lisp") },
  { __TEXT("CLSID\\{99AA27B3-0EAE-11d1-ACB9-00A024803258}\\InprocServer32"), g_szFileName },
  { __TEXT("CLSID\\{99AA27B3-0EAE-11d1-ACB9-00A024803258}\\ProgID"), __TEXT("CormanLisp.Lisp") },
};

const int g_cRegKeyValues = sizeof(g_szRegKeyValues)
                            /sizeof(*g_szRegKeyValues);

LONG g_cLocks = 0;
void SvcLock()	{ InterlockedIncrement(&g_cLocks); }
void SvcUnlock(){ InterlockedDecrement(&g_cLocks); }

class CoCormanLispClassFactory : public IClassFactory
{
public:
    STDMETHODIMP QueryInterface(REFIID riid, void**ppv)
    {
        if (riid == IID_IUnknown)
            *ppv = (IClassFactory*)this;
        else if (riid == IID_IClassFactory)
            *ppv = (IClassFactory*)this;
        else 
            *ppv = 0;
        if (*ppv)
            ((IUnknown *)*ppv)->AddRef();
        return *ppv ? S_OK : E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef(void)
    {
        SvcLock();
        return 2;
    }

    STDMETHODIMP_(ULONG) Release(void)
    {
        SvcUnlock();
        return 1;
    }

    STDMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv)
    {
        *ppv = 0;
        if (pUnkOuter)
            return CLASS_E_NOAGGREGATION;

        CoCormanLisp *p = new CoCormanLisp;

        if (!p)
            return E_OUTOFMEMORY;

        p->AddRef();
        HRESULT hr = p->QueryInterface(riid, ppv);
        p->Release();
        return hr;
    }

    STDMETHODIMP LockServer(BOOL bLock)
    {
        if (bLock)
            SvcLock();
        else
            SvcUnlock();
        return S_OK;
    }
};

// TODO-Declare an instance of your class factory at global scope.
CoCormanLispClassFactory g_classFactory;

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void**ppv)
{
    *ppv = 0;
    if (rclsid == CLSID_CormanLisp)
        return g_classFactory.QueryInterface(riid, ppv);
    else
        return CLASS_E_CLASSNOTAVAILABLE;
}

// for static linking
__declspec(dllexport) HRESULT StaticGetClassObject(REFCLSID rclsid, REFIID riid, void**ppv)
{
    *ppv = 0;
    if (rclsid == CLSID_CormanLisp)
        return g_classFactory.QueryInterface(riid, ppv);
    else
        return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow()
{
    return g_cLocks ? S_FALSE : S_OK;
}

STDAPI DllUnregisterServer();

STDAPI DllRegisterServer()
{
	if (*g_szFileName == 0)
		strcpy_s(g_szFileName, sizeof(g_szFileName), "CormanLispServer.dll");
    LONG r = ERROR_SUCCESS;
    for (int i = 0; 
         r == ERROR_SUCCESS && i < g_cRegKeyValues; 
         i++)
        r = RegSetValue(HKEY_CLASSES_ROOT,
                        g_szRegKeyValues[i][0], 
                        REG_SZ,
                        g_szRegKeyValues[i][1],
                        lstrlen(g_szRegKeyValues[i][1]));
    if (r != ERROR_SUCCESS)
        DllUnregisterServer();
    return (r == ERROR_SUCCESS) ? S_OK : E_FAIL;
}

STDAPI DllUnregisterServer()
{
    HRESULT result = S_OK;
    for (int i = g_cRegKeyValues - 1; i >= 0; i--)
    {
        LONG r = RegDeleteKey(HKEY_CLASSES_ROOT,
                              g_szRegKeyValues[i][0]);
        if (r != ERROR_SUCCESS)
            result = S_FALSE;
    }
    return result;
}
