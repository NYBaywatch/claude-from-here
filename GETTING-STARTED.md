# Claude From Here

A Windows shell extension that adds **"Claude from here"** to the right-click context menu, launching Claude Code in the selected directory.

## What's in this project

### Working now (classic context menu)

- `claude-from-here.reg` — Adds "Claude from here" to the classic right-click menu
- `claude-from-here-uninstall.reg` — Removes the entries

These work on **Windows 10** (top-level) and **Windows 11** (under "Show more options").

**To install:** Double-click `claude-from-here.reg` and confirm the prompt.

### Goal: Windows 11 top-level context menu

Windows 11 introduced a new "modern" context menu. Getting a custom item into the top level requires one of these approaches:

| Approach | Complexity | Requires |
|----------|-----------|----------|
| **Sparse Package (MSIX)** | Medium | AppxManifest + PowerShell registration |
| **COM Shell Extension (C#)** | High | .NET SDK, COM interop, DLL registration |
| **COM Shell Extension (C++)** | High | Visual Studio, C++ COM boilerplate |

**Recommended: Sparse Package** — No DLL compilation needed. You declare the context menu item in an `AppxManifest.xml` and register it with `Add-AppxPackage`. The manifest points to a simple `.exe` or script that launches Claude Code.

## Prerequisites

- Windows 10/11
- [Claude Code CLI](https://docs.anthropic.com/en/docs/claude-code) installed at `C:\Users\jfago\.local\bin\claude.exe`
- [Windows Terminal](https://aka.ms/terminal) installed
- For the sparse package approach: PowerShell (admin)

## Next steps

1. Start Claude Code in this directory
2. Ask Claude to build the sparse package shell extension
3. Test by right-clicking a folder in Explorer
