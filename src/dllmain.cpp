// dllmain.cpp -- DLL entry point, class factory, and COM exports
// CLSID: {b2dd8803-e848-41d5-bb0b-598086308dcf}
// Must match AppxManifest.xml com:Class Id

#include <windows.h>
#include <unknwn.h>
#include <shlobj.h>
#include <new>

// -------------------------------------------------------------------------
// Globals
// -------------------------------------------------------------------------

HMODULE g_hModule = nullptr;  // Captured in DllMain; used by GetIcon
long    g_cDllRef = 0;         // Reference count for DllCanUnloadNow

// CLSID for this COM class — must match AppxManifest.xml exactly
// {b2dd8803-e848-41d5-bb0b-598086308dcf}
extern const CLSID CLSID_ClaudeFromHere =
    { 0xb2dd8803, 0xe848, 0x41d5, { 0xbb, 0x0b, 0x59, 0x80, 0x86, 0x30, 0x8d, 0xcf } };

// Forward declaration
class CClaudeFromHere;

// -------------------------------------------------------------------------
// CClassFactory -- IClassFactory implementation
// -------------------------------------------------------------------------

class CClassFactory : public IClassFactory
{
public:
    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == IID_IUnknown || riid == IID_IClassFactory)
        {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override
    {
        return InterlockedIncrement(&m_cRef);
    }

    IFACEMETHODIMP_(ULONG) Release() override
    {
        ULONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0)
            delete this;
        return cRef;
    }

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override;

    IFACEMETHODIMP LockServer(BOOL fLock) override
    {
        if (fLock)
            InterlockedIncrement(&g_cDllRef);
        else
            InterlockedDecrement(&g_cDllRef);
        return S_OK;
    }

private:
    long m_cRef = 1;
};

// CClaudeFromHere is defined in ClaudeFromHere.cpp; we instantiate it via this factory.
extern IUnknown* CreateClaudeFromHereInstance();

IFACEMETHODIMP CClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv)
{
    *ppv = nullptr;
    if (pUnkOuter)
        return CLASS_E_NOAGGREGATION;

    IUnknown* pObj = CreateClaudeFromHereInstance();
    if (!pObj)
        return E_OUTOFMEMORY;

    HRESULT hr = pObj->QueryInterface(riid, ppv);
    pObj->Release();
    return hr;
}

// -------------------------------------------------------------------------
// DllMain
// -------------------------------------------------------------------------

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID /*lpReserved*/)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

// -------------------------------------------------------------------------
// COM exports
// -------------------------------------------------------------------------

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    *ppv = nullptr;

    if (rclsid != CLSID_ClaudeFromHere)
        return CLASS_E_CLASSNOTAVAILABLE;

    CClassFactory* pFactory = new (std::nothrow) CClassFactory();
    if (!pFactory)
        return E_OUTOFMEMORY;

    HRESULT hr = pFactory->QueryInterface(riid, ppv);
    pFactory->Release();
    return hr;
}

STDAPI DllCanUnloadNow()
{
    return (g_cDllRef == 0) ? S_OK : S_FALSE;
}
