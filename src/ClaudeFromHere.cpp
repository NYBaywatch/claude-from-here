// ClaudeFromHere.cpp -- IExplorerCommand + IObjectWithSite implementation
// CLSID: {b2dd8803-e848-41d5-bb0b-598086308dcf}
//
// Implements "Claude from here" Windows 11 modern context menu handler.
// Handles both folder right-click (Directory) and folder-background right-click
// (Directory\Background) via IObjectWithSite traversal.

#include <windows.h>
#include <strsafe.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl_core.h>
#include <exdisp.h>
#include <new>

// -------------------------------------------------------------------------
// Externals from dllmain.cpp
// -------------------------------------------------------------------------

extern HMODULE g_hModule;
extern long    g_cDllRef;
extern const CLSID CLSID_ClaudeFromHere;

// -------------------------------------------------------------------------
// CClaudeFromHere -- IExplorerCommand + IObjectWithSite
// -------------------------------------------------------------------------

class CClaudeFromHere : public IExplorerCommand, public IObjectWithSite
{
public:
    CClaudeFromHere() : _cRef(1), _punkSite(nullptr)
    {
        InterlockedIncrement(&g_cDllRef);
    }

    ~CClaudeFromHere()
    {
        if (_punkSite)
        {
            _punkSite->Release();
            _punkSite = nullptr;
        }
        InterlockedDecrement(&g_cDllRef);
    }

    // -----------------------------------------------------------------------
    // IUnknown
    // -----------------------------------------------------------------------

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == IID_IUnknown || riid == IID_IExplorerCommand)
        {
            *ppv = static_cast<IExplorerCommand*>(this);
        }
        else if (riid == IID_IObjectWithSite)
        {
            *ppv = static_cast<IObjectWithSite*>(this);
        }
        else
        {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override
    {
        return InterlockedIncrement(&_cRef);
    }

    IFACEMETHODIMP_(ULONG) Release() override
    {
        ULONG cRef = InterlockedDecrement(&_cRef);
        if (cRef == 0)
            delete this;
        return cRef;
    }

    // -----------------------------------------------------------------------
    // IExplorerCommand
    // -----------------------------------------------------------------------

    IFACEMETHODIMP GetTitle(IShellItemArray* /*psiItemArray*/, LPWSTR* ppszName) override
    {
        return SHStrDupW(L"Claude from here", ppszName);
    }

    IFACEMETHODIMP GetIcon(IShellItemArray* /*psiItemArray*/, LPWSTR* ppszIcon) override
    {
        // Derive path to claude.ico from DLL module location.
        // The .ico is placed alongside the DLL in the install/build directory.
        WCHAR szPath[MAX_PATH];
        if (!GetModuleFileNameW(g_hModule, szPath, ARRAYSIZE(szPath)))
            return HRESULT_FROM_WIN32(GetLastError());

        PathRemoveFileSpecW(szPath);
        if (!PathAppendW(szPath, L"claude.ico"))
            return E_FAIL;

        return SHStrDupW(szPath, ppszIcon);
    }

    IFACEMETHODIMP GetToolTip(IShellItemArray* /*psiItemArray*/, LPWSTR* ppszInfotip) override
    {
        return SHStrDupW(L"Open Claude Code in this directory", ppszInfotip);
    }

    IFACEMETHODIMP GetCanonicalName(GUID* pguidCommandName) override
    {
        *pguidCommandName = CLSID_ClaudeFromHere;
        return S_OK;
    }

    IFACEMETHODIMP GetState(IShellItemArray* /*psiItemArray*/, BOOL /*fOkToBeSlow*/,
                            EXPCMDSTATE* pCmdState) override
    {
        *pCmdState = ECS_ENABLED;
        return S_OK;
    }

    IFACEMETHODIMP GetFlags(EXPCMDFLAGS* pFlags) override
    {
        *pFlags = ECF_DEFAULT;
        return S_OK;
    }

    IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** ppEnum) override
    {
        *ppEnum = nullptr;
        return E_NOTIMPL;
    }

    IFACEMETHODIMP Invoke(IShellItemArray* psiItemArray, IBindCtx* /*pbc*/) override
    {
        PWSTR pszPath = nullptr;
        HRESULT hr = E_FAIL;

        // Path A: Folder right-click — psiItemArray contains the selected folder item.
        if (psiItemArray)
        {
            IShellItem* psi = nullptr;
            hr = psiItemArray->GetItemAt(0, &psi);
            if (SUCCEEDED(hr) && psi)
            {
                hr = psi->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &pszPath);
                psi->Release();
            }
        }

        // Path B: Folder background right-click — traverse IObjectWithSite chain.
        if (FAILED(hr) || !pszPath)
        {
            hr = _GetFolderPathFromSite(&pszPath);
        }

        if (SUCCEEDED(hr) && pszPath)
        {
            _LaunchClaude(pszPath);
            CoTaskMemFree(pszPath);
        }

        return S_OK;
    }

    // -----------------------------------------------------------------------
    // IObjectWithSite
    // -----------------------------------------------------------------------

    IFACEMETHODIMP SetSite(IUnknown* punkSite) override
    {
        if (_punkSite)
        {
            _punkSite->Release();
            _punkSite = nullptr;
        }
        if (punkSite)
        {
            _punkSite = punkSite;
            _punkSite->AddRef();
        }
        return S_OK;
    }

    IFACEMETHODIMP GetSite(REFIID riid, void** ppvSite) override
    {
        if (!_punkSite)
        {
            *ppvSite = nullptr;
            return E_FAIL;
        }
        return _punkSite->QueryInterface(riid, ppvSite);
    }

private:
    long     _cRef;
    IUnknown* _punkSite;

    // -----------------------------------------------------------------------
    // _GetFolderPathFromSite
    // Traverses: IServiceProvider -> SID_STopLevelBrowser/IShellBrowser ->
    //            QueryActiveShellView/IShellView -> IFolderView -> GetFolder/IShellItem
    // -----------------------------------------------------------------------

    HRESULT _GetFolderPathFromSite(PWSTR* ppszPath)
    {
        *ppszPath = nullptr;

        if (!_punkSite)
            return E_FAIL;

        IServiceProvider* psp = nullptr;
        HRESULT hr = _punkSite->QueryInterface(IID_PPV_ARGS(&psp));
        if (FAILED(hr))
            return hr;

        IShellBrowser* psb = nullptr;
        hr = psp->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&psb));
        psp->Release();
        if (FAILED(hr))
            return hr;

        IShellView* psv = nullptr;
        hr = psb->QueryActiveShellView(&psv);
        psb->Release();
        if (FAILED(hr))
            return hr;

        IFolderView* pfv = nullptr;
        hr = psv->QueryInterface(IID_PPV_ARGS(&pfv));
        psv->Release();
        if (FAILED(hr))
            return hr;

        IShellItem* psi = nullptr;
        hr = pfv->GetFolder(IID_PPV_ARGS(&psi));
        pfv->Release();
        if (FAILED(hr))
            return hr;

        hr = psi->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, ppszPath);
        psi->Release();
        return hr;
    }

    // -----------------------------------------------------------------------
    // _LaunchClaude
    // Multi-stage path detection for wt.exe and claude.exe (LNCH-02, LNCH-03).
    // Reads registry flags from HKCU\Software\ClaudeFromHere (LNCH-01).
    // Shows actionable MessageBox on failure with install instructions (LNCH-04, LNCH-05).
    // Launches: wt.exe -d "<pszPath>" -- cmd /k claude <flags>
    // -----------------------------------------------------------------------

    // FindExecutable: 3-stage detection (SearchPathW -> HKCU App Paths -> HKLM App Paths).
    // Stage 4 (wt.exe execution alias) is handled in _LaunchClaude after this call.
    static BOOL FindExecutable(PCWSTR exeName, PCWSTR appPathsSubkey, PWSTR szOut, DWORD cchOut)
    {
        // Stage 1: SearchPathW (PATH, includes %LOCALAPPDATA%\Microsoft\WindowsApps)
        if (SearchPathW(nullptr, exeName, nullptr, cchOut, szOut, nullptr))
            return TRUE;

        // Stage 2: HKCU App Paths (wt.exe Store install registers here, not HKLM)
        DWORD cb = cchOut * sizeof(WCHAR);
        if (RegGetValueW(HKEY_CURRENT_USER, appPathsSubkey, nullptr,
                RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, szOut, &cb) == ERROR_SUCCESS
            && szOut[0])
            return TRUE;

        // Stage 3: HKLM App Paths (winget / system-wide installs)
        cb = cchOut * sizeof(WCHAR);
        if (RegGetValueW(HKEY_LOCAL_MACHINE, appPathsSubkey, nullptr,
                RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, szOut, &cb) == ERROR_SUCCESS
            && szOut[0])
            return TRUE;

        return FALSE;
    }

    void _LaunchClaude(PCWSTR pszPath)
    {
        // --- Find wt.exe (3-stage + Stage 4 execution alias fallback) ---
        WCHAR szWt[MAX_PATH] = {};
        BOOL wtFound = FindExecutable(L"wt.exe",
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\wt.exe",
            szWt, ARRAYSIZE(szWt));

        // Stage 4: execution alias for wt.exe (Store installs under %LOCALAPPDATA%\Microsoft\WindowsApps)
        if (!wtFound)
        {
            WCHAR szAlias[MAX_PATH] = {};
            ExpandEnvironmentStringsW(
                L"%LOCALAPPDATA%\\Microsoft\\WindowsApps\\wt.exe",
                szAlias, ARRAYSIZE(szAlias));
            if (PathFileExistsW(szAlias))
            {
                StringCbCopyW(szWt, sizeof(szWt), szAlias);
                wtFound = TRUE;
            }
        }

        if (!wtFound)
        {
            MessageBoxW(nullptr,
                L"Windows Terminal was not found on this machine.\n\n"
                L"To install, open Microsoft Store and search for 'Windows Terminal', or run:\n"
                L"    winget install Microsoft.WindowsTerminal\n\n"
                L"After installing, restart Windows Explorer (or sign out and back in).",
                L"Claude From Here",
                MB_OK | MB_ICONERROR);
            return;
        }

        // --- Find claude.exe (3-stage; no execution alias for claude) ---
        WCHAR szClaude[MAX_PATH] = {};
        BOOL claudeFound = FindExecutable(L"claude.exe",
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\claude.exe",
            szClaude, ARRAYSIZE(szClaude));

        if (!claudeFound)
        {
            MessageBoxW(nullptr,
                L"Claude Code was not found on this machine.\n\n"
                L"To install, run in any terminal:\n"
                L"    npm i -g @anthropic-ai/claude-code\n\n"
                L"After installing, restart Windows Explorer (or sign out and back in).",
                L"Claude From Here",
                MB_OK | MB_ICONERROR);
            return;
        }

        // --- Read registry flags from HKCU\Software\ClaudeFromHere ---
        WCHAR szModel[256]         = {};
        WCHAR szAllowedTools[1024] = {};
        WCHAR szExtraFlags[2048]   = {};
        DWORD dwVerbose            = 0;
        DWORD dwContinue                  = 0;
        DWORD dwResume                    = 0;
        DWORD dwDangerSkip                = 0;
        DWORD dwAllowDangerSkip           = 0;
        WCHAR szRemoteControlPrefix[1024] = {};
        WCHAR szChannels[8192]            = {};

        DWORD cb = sizeof(szModel);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere", L"Model",
            RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, szModel, &cb);

        cb = sizeof(dwVerbose);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere", L"Verbose",
            RRF_RT_REG_DWORD | RRF_ZEROONFAILURE, nullptr, &dwVerbose, &cb);

        cb = sizeof(szAllowedTools);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere", L"AllowedTools",
            RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, szAllowedTools, &cb);

        cb = sizeof(szExtraFlags);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere", L"ExtraFlags",
            RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, szExtraFlags, &cb);

        cb = sizeof(dwContinue);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere", L"Continue",
            RRF_RT_REG_DWORD | RRF_ZEROONFAILURE, nullptr, &dwContinue, &cb);

        cb = sizeof(dwResume);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere", L"Resume",
            RRF_RT_REG_DWORD | RRF_ZEROONFAILURE, nullptr, &dwResume, &cb);

        cb = sizeof(dwDangerSkip);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere", L"DangerouslySkipPermissions",
            RRF_RT_REG_DWORD | RRF_ZEROONFAILURE, nullptr, &dwDangerSkip, &cb);

        cb = sizeof(dwAllowDangerSkip);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere", L"AllowDangerouslySkipPermissions",
            RRF_RT_REG_DWORD | RRF_ZEROONFAILURE, nullptr, &dwAllowDangerSkip, &cb);

        cb = sizeof(szRemoteControlPrefix);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere", L"RemoteControlPrefix",
            RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, szRemoteControlPrefix, &cb);

        cb = sizeof(szChannels);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere", L"Channels",
            RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, szChannels, &cb);

        // --- Build flags string ---
        WCHAR szFlags[16384] = {};
        if (szModel[0])
        {
            StringCbCatW(szFlags, sizeof(szFlags), L" --model ");
            StringCbCatW(szFlags, sizeof(szFlags), szModel);
        }
        if (dwVerbose)
        {
            StringCbCatW(szFlags, sizeof(szFlags), L" --verbose");
        }
        if (dwContinue)
        {
            StringCbCatW(szFlags, sizeof(szFlags), L" -c");
        }
        if (dwResume)
        {
            StringCbCatW(szFlags, sizeof(szFlags), L" -r");
        }
        if (dwDangerSkip)
        {
            StringCbCatW(szFlags, sizeof(szFlags), L" --dangerously-skip-permissions");
        }
        if (dwAllowDangerSkip)
        {
            StringCbCatW(szFlags, sizeof(szFlags), L" --allow-dangerously-skip-permissions");
        }
        if (szRemoteControlPrefix[0])
        {
            StringCbCatW(szFlags, sizeof(szFlags), L" --remote-control-session-name-prefix ");
            StringCbCatW(szFlags, sizeof(szFlags), szRemoteControlPrefix);
        }
        if (szAllowedTools[0])
        {
            StringCbCatW(szFlags, sizeof(szFlags), L" --allowedTools ");
            StringCbCatW(szFlags, sizeof(szFlags), szAllowedTools);
        }
        if (szChannels[0])
        {
            // Split szChannels on '|' (matches Phase 5 storage: string.Join("|", _channels)).
            // Trim whitespace per entry, skip empties (handles trailing pipe and "||"),
            // hard cap at 32 entries (D-03) -- no dynamic allocation, finite work.
            WCHAR* context = nullptr;
            WCHAR* token = wcstok_s(szChannels, L"|", &context);
            int channelCount = 0;
            while (token != nullptr && channelCount < 32)
            {
                // Trim leading whitespace
                while (*token == L' ' || *token == L'\t')
                    token++;
                // Trim trailing whitespace
                size_t tlen = wcslen(token);
                while (tlen > 0 && (token[tlen - 1] == L' ' || token[tlen - 1] == L'\t'))
                {
                    token[tlen - 1] = L'\0';
                    tlen--;
                }
                if (tlen > 0)
                {
                    StringCbCatW(szFlags, sizeof(szFlags), L" --channels ");
                    StringCbCatW(szFlags, sizeof(szFlags), token);
                    channelCount++;
                }
                token = wcstok_s(nullptr, L"|", &context);
            }
        }
        if (szExtraFlags[0])
        {
            StringCbCatW(szFlags, sizeof(szFlags), L" ");
            StringCbCatW(szFlags, sizeof(szFlags), szExtraFlags);
        }

        // --- Build command line ---
        // Ensure drive-root paths like "D:" get a trailing backslash ("D:\")
        // because wt.exe rejects bare drive letters as -d arguments.
        WCHAR szPath[MAX_PATH] = {};
        StringCbCopyW(szPath, sizeof(szPath), pszPath);
        size_t len = wcslen(szPath);
        if (len == 2 && szPath[1] == L':')
        {
            szPath[2] = L'\\';
            szPath[3] = L'\0';
        }

        WCHAR szCmdLine[32768] = {};
        HRESULT hr = StringCbPrintfW(szCmdLine, sizeof(szCmdLine),
            L"wt.exe -d \"%s\" -- cmd /k claude%s", szPath, szFlags);
        if (FAILED(hr))
            return;

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};

        // Use lpApplicationName = szWt so spaces in the Store install path
        // (e.g. C:\Program Files\WindowsApps\...) are handled correctly.
        if (CreateProcessW(
                szWt,          // lpApplicationName -- full resolved path, handles spaces
                szCmdLine,     // lpCommandLine
                nullptr,       // lpProcessAttributes
                nullptr,       // lpThreadAttributes
                FALSE,         // bInheritHandles
                0,             // dwCreationFlags
                nullptr,       // lpEnvironment
                nullptr,       // lpCurrentDirectory
                &si,
                &pi))
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
};

// -------------------------------------------------------------------------
// Factory function called by dllmain.cpp CClassFactory::CreateInstance
// -------------------------------------------------------------------------

IUnknown* CreateClaudeFromHereInstance()
{
    return static_cast<IExplorerCommand*>(new (std::nothrow) CClaudeFromHere());
}
