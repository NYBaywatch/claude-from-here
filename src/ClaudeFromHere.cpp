// ClaudeFromHere.cpp -- IExplorerCommand + IObjectWithSite implementation
// CLSID: {b2dd8803-e848-41d5-bb0b-598086308dcf}
//
// Implements "Claude from here" Windows 11 modern context menu handler.
// Handles both folder right-click (Directory) and folder-background right-click
// (Directory\Background) via IObjectWithSite traversal.
//
// When provider profiles exist under HKCU\Software\ClaudeFromHere\Providers,
// the command becomes a flyout: "Claude (default)" plus one entry per profile.
// A provider launch routes Claude Code to that provider's Anthropic-compatible
// endpoint via ANTHROPIC_BASE_URL / ANTHROPIC_AUTH_TOKEN / ANTHROPIC_MODEL.

#include <windows.h>
#include <strsafe.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl_core.h>
#include <exdisp.h>
#include <wincrypt.h>
#include <new>

// -------------------------------------------------------------------------
// Externals from dllmain.cpp
// -------------------------------------------------------------------------

extern HMODULE g_hModule;
extern long    g_cDllRef;
extern const CLSID CLSID_ClaudeFromHere;

// -------------------------------------------------------------------------
// Provider profiles (HKCU\Software\ClaudeFromHere\Providers\<subkey>)
// Values: Name (SZ), BaseUrl (SZ), Model (SZ), AuthToken (BINARY, DPAPI blob)
// -------------------------------------------------------------------------

const int MAX_PROVIDERS = 8;

struct ProviderProfile
{
    WCHAR name[128];
    WCHAR baseUrl[512];
    WCHAR model[128];
    WCHAR token[1024]; // decrypted at load; per-user DPAPI protects it at rest
    WCHAR effort[16];  // "", or a whitelisted --effort token
};

// Defense-in-depth (from PR #4, credit Hugo Karlsson): validate an effort token
// against the whitelist before it ever reaches the command line. The config app
// only writes combo-box constants, but this guarantees nothing else can.
static bool IsValidEffort(PCWSTR effort)
{
    if (!effort || !effort[0]) return false;
    static const PCWSTR kValid[] = { L"low", L"medium", L"high", L"xhigh", L"max" };
    for (PCWSTR v : kValid)
        if (wcscmp(effort, v) == 0)
            return true;
    return false;
}

// The five standard effort-only flyout entries, shown when the
// ShowEffortLevels registry flag is set.
struct EffortLevel { PCWSTR token; PCWSTR title; };
static const EffortLevel kEffortLevels[] = {
    { L"low",    L"Low effort"        },
    { L"medium", L"Medium effort"     },
    { L"high",   L"High effort"       },
    { L"xhigh",  L"Extra high effort" },
    { L"max",    L"Max effort"        },
};

// Decrypt a DPAPI blob written by the config app (UTF-16 payload, no terminator).
static void DecryptAuthToken(const BYTE* blob, DWORD cbBlob, PWSTR szOut, size_t cchOut)
{
    szOut[0] = L'\0';
    if (!blob || cbBlob == 0)
        return;

    DATA_BLOB in = { cbBlob, const_cast<BYTE*>(blob) };
    DATA_BLOB out = {};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out))
        return;

    size_t cch = out.cbData / sizeof(WCHAR);
    if (cch >= cchOut)
        cch = cchOut - 1;
    memcpy(szOut, out.pbData, cch * sizeof(WCHAR));
    szOut[cch] = L'\0';

    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
}

// Returns the number of profiles loaded (0 if the Providers key is absent).
static int LoadProviders(ProviderProfile* profiles, int maxProfiles)
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere\\Providers",
            0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return 0;

    int count = 0;
    for (DWORD i = 0; count < maxProfiles; i++)
    {
        WCHAR szSubkey[256];
        DWORD cchSubkey = ARRAYSIZE(szSubkey);
        if (RegEnumKeyExW(hKey, i, szSubkey, &cchSubkey,
                nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;

        HKEY hSub = nullptr;
        if (RegOpenKeyExW(hKey, szSubkey, 0, KEY_READ, &hSub) != ERROR_SUCCESS)
            continue;

        ProviderProfile& p = profiles[count];
        ZeroMemory(&p, sizeof(p));

        DWORD cb = sizeof(p.name);
        RegGetValueW(hSub, nullptr, L"Name",
            RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, p.name, &cb);

        cb = sizeof(p.baseUrl);
        RegGetValueW(hSub, nullptr, L"BaseUrl",
            RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, p.baseUrl, &cb);

        cb = sizeof(p.model);
        RegGetValueW(hSub, nullptr, L"Model",
            RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, p.model, &cb);

        cb = sizeof(p.effort);
        RegGetValueW(hSub, nullptr, L"Effort",
            RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, p.effort, &cb);

        BYTE tokenBlob[4096];
        cb = sizeof(tokenBlob);
        if (RegGetValueW(hSub, nullptr, L"AuthToken",
                RRF_RT_REG_BINARY | RRF_ZEROONFAILURE, nullptr, tokenBlob, &cb) == ERROR_SUCCESS)
        {
            DecryptAuthToken(tokenBlob, cb, p.token, ARRAYSIZE(p.token));
        }

        RegCloseKey(hSub);

        // A profile with no name and no base URL is junk -- skip it.
        if (p.name[0] || p.baseUrl[0])
            count++;
    }

    RegCloseKey(hKey);
    return count;
}

static bool ShowEffortLevels()
{
    DWORD dwShow = 0;
    DWORD cb = sizeof(dwShow);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere", L"ShowEffortLevels",
        RRF_RT_REG_DWORD | RRF_ZEROONFAILURE, nullptr, &dwShow, &cb);
    return dwShow != 0;
}

// -------------------------------------------------------------------------
// Icon path helper (claude.ico sits alongside the DLL)
// -------------------------------------------------------------------------

static HRESULT GetClaudeIconPath(LPWSTR* ppszIcon)
{
    WCHAR szPath[MAX_PATH];
    if (!GetModuleFileNameW(g_hModule, szPath, ARRAYSIZE(szPath)))
        return HRESULT_FROM_WIN32(GetLastError());

    PathRemoveFileSpecW(szPath);
    if (!PathAppendW(szPath, L"claude.ico"))
        return E_FAIL;

    return SHStrDupW(szPath, ppszIcon);
}

// -------------------------------------------------------------------------
// LaunchClaude
// Multi-stage path detection for wt.exe and claude.exe (LNCH-02, LNCH-03).
// Reads registry flags from HKCU\Software\ClaudeFromHere (LNCH-01).
// Shows actionable MessageBox on failure with install instructions (LNCH-04, LNCH-05).
// Launches: wt.exe -d "<pszPath>" -- cmd /k [set env &&] claude <flags>
//
// Provider env vars are injected through cmd `set` commands rather than the
// CreateProcess environment block: wt.exe hands the tab to an already-running
// WindowsTerminal.exe under windowingBehavior=useExisting, which would drop
// an inherited environment.
// -------------------------------------------------------------------------

// FindExecutable: 3-stage detection (SearchPathW -> HKCU App Paths -> HKLM App Paths).
// Stage 4 (wt.exe execution alias) is handled in LaunchClaude after this call.
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

static void LaunchClaude(PCWSTR pszPath, const ProviderProfile* pProvider)
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

    // --- Build provider env prefix (set "VAR=value" && ...) ---
    WCHAR szEnvPrefix[8192] = {};
    if (pProvider)
    {
        if (pProvider->baseUrl[0])
        {
            StringCbCatW(szEnvPrefix, sizeof(szEnvPrefix), L"set \"ANTHROPIC_BASE_URL=");
            StringCbCatW(szEnvPrefix, sizeof(szEnvPrefix), pProvider->baseUrl);
            StringCbCatW(szEnvPrefix, sizeof(szEnvPrefix), L"\" && ");
            // A globally-set Anthropic key would conflict with the routed token
            // (OpenRouter's docs require it blank).
            StringCbCatW(szEnvPrefix, sizeof(szEnvPrefix), L"set \"ANTHROPIC_API_KEY=\" && ");
        }
        if (pProvider->token[0])
        {
            StringCbCatW(szEnvPrefix, sizeof(szEnvPrefix), L"set \"ANTHROPIC_AUTH_TOKEN=");
            StringCbCatW(szEnvPrefix, sizeof(szEnvPrefix), pProvider->token);
            StringCbCatW(szEnvPrefix, sizeof(szEnvPrefix), L"\" && ");
        }
        if (pProvider->model[0])
        {
            StringCbCatW(szEnvPrefix, sizeof(szEnvPrefix), L"set \"ANTHROPIC_MODEL=");
            StringCbCatW(szEnvPrefix, sizeof(szEnvPrefix), pProvider->model);
            StringCbCatW(szEnvPrefix, sizeof(szEnvPrefix), L"\" && ");
            // Background/haiku calls must not hit the third-party endpoint with an
            // Anthropic model name it doesn't serve.
            StringCbCatW(szEnvPrefix, sizeof(szEnvPrefix), L"set \"ANTHROPIC_SMALL_FAST_MODEL=");
            StringCbCatW(szEnvPrefix, sizeof(szEnvPrefix), pProvider->model);
            StringCbCatW(szEnvPrefix, sizeof(szEnvPrefix), L"\" && ");
        }
    }

    // --- Build flags string ---
    WCHAR szFlags[16384] = {};
    // The provider's model (via ANTHROPIC_MODEL) wins over the global --model flag.
    if (szModel[0] && !(pProvider && pProvider->model[0]))
    {
        StringCbCatW(szFlags, sizeof(szFlags), L" --model ");
        StringCbCatW(szFlags, sizeof(szFlags), szModel);
    }
    if (pProvider && IsValidEffort(pProvider->effort))
    {
        StringCbCatW(szFlags, sizeof(szFlags), L" --effort ");
        StringCbCatW(szFlags, sizeof(szFlags), pProvider->effort);
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
        L"wt.exe -d \"%s\" -- cmd /k %sclaude%s", szPath, szEnvPrefix, szFlags);
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

    SecureZeroMemory(szEnvPrefix, sizeof(szEnvPrefix));
    SecureZeroMemory(szCmdLine, sizeof(szCmdLine));
}

// -------------------------------------------------------------------------
// Forward declaration -- subcommands resolve the folder path through the
// top-level command's site chain on folder-background clicks.
// -------------------------------------------------------------------------

class CClaudeFromHere;
static HRESULT ResolveInvokePath(IShellItemArray* psiItemArray, CClaudeFromHere* pParent, PWSTR* ppszPath);

// -------------------------------------------------------------------------
// CClaudeSubCommand -- one flyout entry ("Claude (default)" or a provider)
// -------------------------------------------------------------------------

class CClaudeSubCommand : public IExplorerCommand
{
public:
    // pProvider == nullptr means the default (Anthropic) entry.
    CClaudeSubCommand(CClaudeFromHere* pParent, const ProviderProfile* pProvider);
    ~CClaudeSubCommand();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == IID_IUnknown || riid == IID_IExplorerCommand)
        {
            *ppv = static_cast<IExplorerCommand*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
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

    // IExplorerCommand
    IFACEMETHODIMP GetTitle(IShellItemArray*, LPWSTR* ppszName) override
    {
        return SHStrDupW(_hasProvider ? _profile.name : L"Claude (default)", ppszName);
    }

    IFACEMETHODIMP GetIcon(IShellItemArray*, LPWSTR* ppszIcon) override
    {
        return GetClaudeIconPath(ppszIcon);
    }

    IFACEMETHODIMP GetToolTip(IShellItemArray*, LPWSTR* ppszInfotip) override
    {
        return SHStrDupW((_hasProvider && _profile.baseUrl[0])
            ? L"Open Claude Code here, routed to this provider"
            : L"Open Claude Code in this directory", ppszInfotip);
    }

    IFACEMETHODIMP GetCanonicalName(GUID* pguidCommandName) override
    {
        *pguidCommandName = GUID_NULL;
        return S_OK;
    }

    IFACEMETHODIMP GetState(IShellItemArray*, BOOL, EXPCMDSTATE* pCmdState) override
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

    IFACEMETHODIMP Invoke(IShellItemArray* psiItemArray, IBindCtx*) override
    {
        PWSTR pszPath = nullptr;
        if (SUCCEEDED(ResolveInvokePath(psiItemArray, _pParent, &pszPath)) && pszPath)
        {
            LaunchClaude(pszPath, _hasProvider ? &_profile : nullptr);
            CoTaskMemFree(pszPath);
        }
        return S_OK;
    }

private:
    long             _cRef;
    CClaudeFromHere* _pParent; // AddRef'd; supplies the site chain for background clicks
    bool             _hasProvider;
    ProviderProfile  _profile;
};

// -------------------------------------------------------------------------
// CEnumSubCommands -- IEnumExplorerCommand over the flyout entries
// -------------------------------------------------------------------------

class CEnumSubCommands : public IEnumExplorerCommand
{
public:
    CEnumSubCommands() : _cRef(1), _count(0), _index(0)
    {
        ZeroMemory(_items, sizeof(_items));
        InterlockedIncrement(&g_cDllRef);
    }

    ~CEnumSubCommands()
    {
        for (UINT i = 0; i < _count; i++)
            if (_items[i])
                _items[i]->Release();
        InterlockedDecrement(&g_cDllRef);
    }

    // Takes ownership of one reference to pCmd.
    void Add(IExplorerCommand* pCmd)
    {
        if (_count < ARRAYSIZE(_items))
            _items[_count++] = pCmd;
        else
            pCmd->Release();
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == IID_IUnknown || riid == IID_IEnumExplorerCommand)
        {
            *ppv = static_cast<IEnumExplorerCommand*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
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

    // IEnumExplorerCommand
    IFACEMETHODIMP Next(ULONG celt, IExplorerCommand** pUICommand, ULONG* pceltFetched) override
    {
        ULONG fetched = 0;
        while (fetched < celt && _index < _count)
        {
            pUICommand[fetched] = _items[_index];
            pUICommand[fetched]->AddRef();
            fetched++;
            _index++;
        }
        if (pceltFetched)
            *pceltFetched = fetched;
        return (fetched == celt) ? S_OK : S_FALSE;
    }

    IFACEMETHODIMP Skip(ULONG celt) override
    {
        _index += celt;
        if (_index > _count)
            _index = _count;
        return S_OK;
    }

    IFACEMETHODIMP Reset() override
    {
        _index = 0;
        return S_OK;
    }

    IFACEMETHODIMP Clone(IEnumExplorerCommand** ppEnum) override
    {
        *ppEnum = nullptr;
        return E_NOTIMPL;
    }

private:
    long              _cRef;
    IExplorerCommand* _items[MAX_PROVIDERS + 1 + ARRAYSIZE(kEffortLevels)];
    UINT              _count;
    UINT              _index;
};

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
        return GetClaudeIconPath(ppszIcon);
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
        // Flyout only when profiles or effort levels are configured; single command otherwise.
        ProviderProfile profiles[MAX_PROVIDERS];
        *pFlags = (LoadProviders(profiles, MAX_PROVIDERS) > 0 || ShowEffortLevels())
            ? ECF_HASSUBCOMMANDS
            : ECF_DEFAULT;
        return S_OK;
    }

    IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** ppEnum) override
    {
        *ppEnum = nullptr;

        ProviderProfile profiles[MAX_PROVIDERS];
        int count = LoadProviders(profiles, MAX_PROVIDERS);
        bool showEffort = ShowEffortLevels();
        if (count == 0 && !showEffort)
            return E_NOTIMPL;

        CEnumSubCommands* pEnum = new (std::nothrow) CEnumSubCommands();
        if (!pEnum)
            return E_OUTOFMEMORY;

        IExplorerCommand* pDefault =
            new (std::nothrow) CClaudeSubCommand(this, nullptr);
        if (pDefault)
            pEnum->Add(pDefault);

        if (showEffort)
        {
            // Standard effort-only entries (PR #4, credit Hugo Karlsson):
            // same launch as default, plus --effort <token>.
            for (const EffortLevel& lvl : kEffortLevels)
            {
                ProviderProfile p = {};
                StringCbCopyW(p.name, sizeof(p.name), lvl.title);
                StringCbCopyW(p.effort, sizeof(p.effort), lvl.token);
                IExplorerCommand* pCmd =
                    new (std::nothrow) CClaudeSubCommand(this, &p);
                if (pCmd)
                    pEnum->Add(pCmd);
            }
        }

        for (int i = 0; i < count; i++)
        {
            IExplorerCommand* pCmd =
                new (std::nothrow) CClaudeSubCommand(this, &profiles[i]);
            if (pCmd)
                pEnum->Add(pCmd);
        }

        *ppEnum = pEnum;
        return S_OK;
    }

    IFACEMETHODIMP Invoke(IShellItemArray* psiItemArray, IBindCtx* /*pbc*/) override
    {
        PWSTR pszPath = nullptr;
        if (SUCCEEDED(ResolveInvokePath(psiItemArray, this, &pszPath)) && pszPath)
        {
            LaunchClaude(pszPath, nullptr);
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

    // -----------------------------------------------------------------------
    // GetFolderPathFromSite
    // Traverses: IServiceProvider -> SID_STopLevelBrowser/IShellBrowser ->
    //            QueryActiveShellView/IShellView -> IFolderView -> GetFolder/IShellItem
    // Public so flyout subcommands can resolve background-click paths too.
    // -----------------------------------------------------------------------

    HRESULT GetFolderPathFromSite(PWSTR* ppszPath)
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

private:
    long     _cRef;
    IUnknown* _punkSite;
};

// -------------------------------------------------------------------------
// ResolveInvokePath
// Path A: folder right-click -- psiItemArray contains the selected folder.
// Path B: folder-background right-click -- traverse the parent's site chain.
// -------------------------------------------------------------------------

static HRESULT ResolveInvokePath(IShellItemArray* psiItemArray, CClaudeFromHere* pParent, PWSTR* ppszPath)
{
    *ppszPath = nullptr;
    HRESULT hr = E_FAIL;

    if (psiItemArray)
    {
        IShellItem* psi = nullptr;
        hr = psiItemArray->GetItemAt(0, &psi);
        if (SUCCEEDED(hr) && psi)
        {
            hr = psi->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, ppszPath);
            psi->Release();
        }
    }

    if ((FAILED(hr) || !*ppszPath) && pParent)
    {
        hr = pParent->GetFolderPathFromSite(ppszPath);
    }

    return hr;
}

// -------------------------------------------------------------------------
// CClaudeSubCommand out-of-line members (need the full CClaudeFromHere type)
// -------------------------------------------------------------------------

CClaudeSubCommand::CClaudeSubCommand(CClaudeFromHere* pParent, const ProviderProfile* pProvider)
    : _cRef(1), _pParent(pParent), _hasProvider(pProvider != nullptr)
{
    InterlockedIncrement(&g_cDllRef);
    if (_pParent)
        _pParent->AddRef();
    if (pProvider)
        _profile = *pProvider;
    else
        ZeroMemory(&_profile, sizeof(_profile));
}

CClaudeSubCommand::~CClaudeSubCommand()
{
    SecureZeroMemory(&_profile, sizeof(_profile));
    if (_pParent)
        _pParent->Release();
    InterlockedDecrement(&g_cDllRef);
}

// -------------------------------------------------------------------------
// Factory function called by dllmain.cpp CClassFactory::CreateInstance
// -------------------------------------------------------------------------

IUnknown* CreateClaudeFromHereInstance()
{
    return static_cast<IExplorerCommand*>(new (std::nothrow) CClaudeFromHere());
}
