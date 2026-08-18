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
    WCHAR smallModel[128]; // ANTHROPIC_SMALL_FAST_MODEL; falls back to model
    WCHAR subkey[64];  // registry subkey name; the launch script decrypts the
                       // DPAPI AuthToken from it in-terminal, so the secret
                       // never appears on a process command line
    WCHAR effort[16];  // "", or a whitelisted --effort token
    WCHAR tabColor[16];     // "#RRGGBB" for wt --tabColor, or ""
    WCHAR extraFlags[1024]; // appended after the global flags
    DWORD dwContinue;
    DWORD dwResume;
    DWORD dwVerbose;
};

// wt.exe --tabColor input goes onto a command line; accept strictly #RRGGBB.
static bool IsValidTabColor(PCWSTR c)
{
    if (!c || c[0] != L'#' || wcslen(c) != 7) return false;
    for (int i = 1; i < 7; i++)
        if (!iswxdigit(c[i])) return false;
    return true;
}

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

        cb = sizeof(p.smallModel);
        RegGetValueW(hSub, nullptr, L"SmallFastModel",
            RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, p.smallModel, &cb);

        cb = sizeof(p.tabColor);
        RegGetValueW(hSub, nullptr, L"TabColor",
            RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, p.tabColor, &cb);

        cb = sizeof(p.extraFlags);
        RegGetValueW(hSub, nullptr, L"ExtraFlags",
            RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, p.extraFlags, &cb);

        cb = sizeof(p.dwContinue);
        RegGetValueW(hSub, nullptr, L"Continue",
            RRF_RT_REG_DWORD | RRF_ZEROONFAILURE, nullptr, &p.dwContinue, &cb);

        cb = sizeof(p.dwResume);
        RegGetValueW(hSub, nullptr, L"Resume",
            RRF_RT_REG_DWORD | RRF_ZEROONFAILURE, nullptr, &p.dwResume, &cb);

        cb = sizeof(p.dwVerbose);
        RegGetValueW(hSub, nullptr, L"Verbose",
            RRF_RT_REG_DWORD | RRF_ZEROONFAILURE, nullptr, &p.dwVerbose, &cb);

        StringCbCopyW(p.subkey, sizeof(p.subkey), szSubkey);

        RegCloseKey(hSub);

        // A profile with no name and no base URL is junk -- skip it.
        if (p.name[0] || p.baseUrl[0])
            count++;
    }

    RegCloseKey(hKey);
    return count;
}

// Reads the EffortLevels registry value (pipe-joined tokens, e.g. "low|max")
// into a bitmask over kEffortLevels. Zero = no effort entries in the menu.
static DWORD GetEffortLevelMask()
{
    WCHAR szLevels[128] = {};
    DWORD cb = sizeof(szLevels);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere", L"EffortLevels",
        RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, szLevels, &cb);

    DWORD mask = 0;
    WCHAR* ctx = nullptr;
    for (WCHAR* tok = wcstok_s(szLevels, L"|", &ctx); tok; tok = wcstok_s(nullptr, L"|", &ctx))
        for (int i = 0; i < (int)ARRAYSIZE(kEffortLevels); i++)
            if (wcscmp(tok, kEffortLevels[i].token) == 0)
                mask |= (1u << i);
    return mask;
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
// Launches: wt.exe -d "<pszPath>" -- cmd /k claude <flags>
// Provider launches instead run `powershell -EncodedCommand <script>` which
// sets the provider env vars and decrypts the DPAPI token in-terminal — never
// via the CreateProcess environment block (wt.exe hands the tab to an
// already-running WindowsTerminal.exe under windowingBehavior=useExisting,
// dropping inherited environments) and never on a command line (visible to
// Task Manager and process-creation audit logs).
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

// Append a PowerShell single-quoted literal ('...' with embedded ' doubled).
static void AppendPsQuoted(PWSTR szDest, size_t cbDest, PCWSTR szValue)
{
    StringCbCatW(szDest, cbDest, L"'");
    for (PCWSTR p = szValue; *p; p++)
    {
        WCHAR ch[3] = { *p, (*p == L'\'') ? L'\'' : L'\0', L'\0' };
        StringCbCatW(szDest, cbDest, ch);
    }
    StringCbCatW(szDest, cbDest, L"'");
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

    // --- Per-profile flag additions (stack on top of the global flags) ---
    if (pProvider)
    {
        if (pProvider->dwContinue && !dwContinue)
            StringCbCatW(szFlags, sizeof(szFlags), L" -c");
        if (pProvider->dwResume && !dwResume)
            StringCbCatW(szFlags, sizeof(szFlags), L" -r");
        if (pProvider->dwVerbose && !dwVerbose)
            StringCbCatW(szFlags, sizeof(szFlags), L" --verbose");
        if (pProvider->extraFlags[0])
        {
            StringCbCatW(szFlags, sizeof(szFlags), L" ");
            StringCbCatW(szFlags, sizeof(szFlags), pProvider->extraFlags);
        }
    }

    // --- Windows Terminal tab options (title = profile name, optional color) ---
    WCHAR szWtOpts[256] = {};
    if (pProvider && pProvider->name[0])
    {
        StringCbCatW(szWtOpts, sizeof(szWtOpts), L" --title \"");
        StringCbCatW(szWtOpts, sizeof(szWtOpts), pProvider->name);
        StringCbCatW(szWtOpts, sizeof(szWtOpts), L"\"");
    }
    if (pProvider && IsValidTabColor(pProvider->tabColor))
    {
        StringCbCatW(szWtOpts, sizeof(szWtOpts), L" --tabColor \"");
        StringCbCatW(szWtOpts, sizeof(szWtOpts), pProvider->tabColor);
        StringCbCatW(szWtOpts, sizeof(szWtOpts), L"\"");
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
    HRESULT hr;
    if (pProvider && pProvider->baseUrl[0])
    {
        // Provider launch runs through `powershell -EncodedCommand`: the script
        // decrypts the DPAPI AuthToken from the registry inside the new terminal,
        // so the secret never appears on any process command line (Task Manager,
        // 4688/Sysmon audit logs). Env vars set here also survive Windows
        // Terminal's useExisting windowing, which drops inherited environments.
        WCHAR szScript[16384] = {};
        StringCbCatW(szScript, sizeof(szScript), L"$env:ANTHROPIC_BASE_URL=");
        AppendPsQuoted(szScript, sizeof(szScript), pProvider->baseUrl);
        // A globally-set Anthropic key would conflict with the routed token
        // (OpenRouter's docs require it blank).
        StringCbCatW(szScript, sizeof(szScript), L";$env:ANTHROPIC_API_KEY='';");
        if (pProvider->model[0])
        {
            StringCbCatW(szScript, sizeof(szScript), L"$env:ANTHROPIC_MODEL=");
            AppendPsQuoted(szScript, sizeof(szScript), pProvider->model);
            StringCbCatW(szScript, sizeof(szScript), L";");
        }
        // Background/haiku calls must not hit the third-party endpoint with an
        // Anthropic model name it doesn't serve; a dedicated small/fast model
        // wins, otherwise mirror the main model.
        PCWSTR smallModel = pProvider->smallModel[0] ? pProvider->smallModel : pProvider->model;
        if (smallModel[0])
        {
            StringCbCatW(szScript, sizeof(szScript), L"$env:ANTHROPIC_SMALL_FAST_MODEL=");
            AppendPsQuoted(szScript, sizeof(szScript), smallModel);
            StringCbCatW(szScript, sizeof(szScript), L";");
        }
        if (pProvider->subkey[0])
        {
            WCHAR szRegPath[256] = {};
            StringCbPrintfW(szRegPath, sizeof(szRegPath),
                L"HKCU:\\Software\\ClaudeFromHere\\Providers\\%s", pProvider->subkey);
            StringCbCatW(szScript, sizeof(szScript),
                L"$b=(Get-ItemProperty -ErrorAction SilentlyContinue ");
            AppendPsQuoted(szScript, sizeof(szScript), szRegPath);
            StringCbCatW(szScript, sizeof(szScript),
                L").AuthToken;if($b){Add-Type -AssemblyName System.Security;"
                L"$env:ANTHROPIC_AUTH_TOKEN=[System.Text.Encoding]::Unicode.GetString("
                L"[System.Security.Cryptography.ProtectedData]::Unprotect($b,$null,'CurrentUser'))};");
        }
        StringCbCatW(szScript, sizeof(szScript), L"& ");
        AppendPsQuoted(szScript, sizeof(szScript), szClaude);
        StringCbCatW(szScript, sizeof(szScript), szFlags);

        // Base64-encode the UTF-16LE script for -EncodedCommand: keeps wt.exe from
        // splitting on ';' and sidesteps nested-quoting entirely.
        WCHAR szEncoded[24576] = {};
        DWORD cchEncoded = ARRAYSIZE(szEncoded);
        size_t cchScript = wcslen(szScript);
        if (!CryptBinaryToStringW(
                reinterpret_cast<const BYTE*>(szScript),
                static_cast<DWORD>(cchScript * sizeof(WCHAR)),
                CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                szEncoded, &cchEncoded))
            return;

        hr = StringCbPrintfW(szCmdLine, sizeof(szCmdLine),
            L"wt.exe -d \"%s\"%s -- powershell -NoLogo -NoExit -EncodedCommand %s",
            szPath, szWtOpts, szEncoded);
    }
    else
    {
        hr = StringCbPrintfW(szCmdLine, sizeof(szCmdLine),
            L"wt.exe -d \"%s\"%s -- cmd /k claude%s", szPath, szWtOpts, szFlags);
    }
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
        *pFlags = (LoadProviders(profiles, MAX_PROVIDERS) > 0 || GetEffortLevelMask() != 0)
            ? ECF_HASSUBCOMMANDS
            : ECF_DEFAULT;
        return S_OK;
    }

    IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** ppEnum) override
    {
        *ppEnum = nullptr;

        ProviderProfile profiles[MAX_PROVIDERS];
        int count = LoadProviders(profiles, MAX_PROVIDERS);
        DWORD effortMask = GetEffortLevelMask();
        if (count == 0 && effortMask == 0)
            return E_NOTIMPL;

        CEnumSubCommands* pEnum = new (std::nothrow) CEnumSubCommands();
        if (!pEnum)
            return E_OUTOFMEMORY;

        IExplorerCommand* pDefault =
            new (std::nothrow) CClaudeSubCommand(this, nullptr);
        if (pDefault)
            pEnum->Add(pDefault);

        // Effort-only entries (PR #4, credit Hugo Karlsson): same launch as
        // default, plus --effort <token>. Only the levels the user enabled.
        for (int i = 0; i < (int)ARRAYSIZE(kEffortLevels); i++)
        {
            if (!(effortMask & (1u << i)))
                continue;
            ProviderProfile p = {};
            StringCbCopyW(p.name, sizeof(p.name), kEffortLevels[i].title);
            StringCbCopyW(p.effort, sizeof(p.effort), kEffortLevels[i].token);
            IExplorerCommand* pCmd =
                new (std::nothrow) CClaudeSubCommand(this, &p);
            if (pCmd)
                pEnum->Add(pCmd);
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
