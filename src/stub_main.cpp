// stub_main.cpp -- Minimal stub executable required by AppxManifest Application Executable.
// The actual shell extension logic lives in ClaudeFromHere.dll (COM surrogate via dllhost.exe).
// This exe is never invoked; it exists only to satisfy MSIX manifest schema requirements.

#include <windows.h>

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    return 0;
}
