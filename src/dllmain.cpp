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

// CLSIDs for the COM classes — must match AppxManifest.xml exactly.
// CLSID_ClaudeFromHere   {b2dd8803-e848-41d5-bb0b-598086308dcf} -- direct-launch command
// CLSID_ClaudeEffortMenu {d6aefcec-19c4-46ec-b52d-14401dfd9079} -- effort flyout
extern const CLSID CLSID_ClaudeFromHere =
    { 0xb2dd8803, 0xe848, 0x41d5, { 0xbb, 0x0b, 0x59, 0x80, 0x86, 0x30, 0x8d, 0xcf } };
extern const CLSID CLSID_ClaudeEffortMenu =
    { 0xd6aefcec, 0x19c4, 0x46ec, { 0xb5, 0x2d, 0x14, 0x40, 0x1d, 0xfd, 0x90, 0x79 } };

// Forward declarations
class CClaudeFromHere;
class CClaudeEffortMenu;

// -------------------------------------------------------------------------
// CClassFactory -- IClassFactory implementation
// -------------------------------------------------------------------------

// Each registered CLSID maps to a creator function that news up the right class.
typedef IUnknown* (*PFNCREATEINSTANCE)();

class CClassFactory : public IClassFactory
{
public:
    explicit CClassFactory(PFNCREATEINSTANCE pfnCreate)
        : m_cRef(1), m_pfnCreate(pfnCreate) {}

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
    long              m_cRef;
    PFNCREATEINSTANCE m_pfnCreate;
};

// Factory functions for the two COM classes, both defined in ClaudeFromHere.cpp.
extern IUnknown* CreateClaudeFromHereInstance();
extern IUnknown* CreateClaudeEffortMenuInstance();

IFACEMETHODIMP CClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv)
{
    *ppv = nullptr;
    if (pUnkOuter)
        return CLASS_E_NOAGGREGATION;

    IUnknown* pObj = m_pfnCreate();
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

    PFNCREATEINSTANCE pfnCreate = nullptr;
    if (rclsid == CLSID_ClaudeFromHere)
        pfnCreate = CreateClaudeFromHereInstance;
    else if (rclsid == CLSID_ClaudeEffortMenu)
        pfnCreate = CreateClaudeEffortMenuInstance;
    else
        return CLASS_E_CLASSNOTAVAILABLE;

    CClassFactory* pFactory = new (std::nothrow) CClassFactory(pfnCreate);
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
