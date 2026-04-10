; ClaudeFromHere Inno Setup Installer Script
; Produces ClaudeFromHere-Setup.exe for per-user installation
;
; Key behaviors:
;   - Installs to %LOCALAPPDATA%\ClaudeFromHere\ (no admin required)
;   - Registers pre-signed MSIX sparse package via Add-AppxPackage
;   - Handles upgrade: unregisters old MSIX + kills Explorer BEFORE file copy
;   - Clean uninstall: removes MSIX registration, restarts Explorer
;   - Preserves HKCU\Software\ClaudeFromHere user settings across uninstall/reinstall

; ---------------------------------------------------------------------------
; [Setup]
; ---------------------------------------------------------------------------

[Setup]
AppId={{648d3dc8-04a3-461b-b4f8-23753c3ffa60}
AppName=Claude From Here
AppVersion=1.0.0
AppPublisher=Claude From Here
DefaultDirName={localappdata}\ClaudeFromHere
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline
OutputBaseFilename=ClaudeFromHere-Setup
OutputDir=..\dist
Compression=lzma
SolidCompression=yes
DisableDirPage=yes
DisableProgramGroupPage=yes
WizardStyle=modern
SetupIconFile=..\assets\claude.ico
UninstallDisplayIcon={app}\claude.ico
UninstallDisplayName=Claude From Here

; ---------------------------------------------------------------------------
; [Files]
; ---------------------------------------------------------------------------

[Files]
Source: "..\build\ClaudeFromHere.dll";              DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\ClaudeFromHere.exe";              DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\ClaudeFromHereConfig.exe";        DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\ClaudeFromHereConfig.exe.config"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\claude.ico";                      DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\AppxManifest.xml";                DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Assets\*";                        DestDir: "{app}\Assets"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\ClaudeFromHere.msix";             DestDir: "{app}"; Flags: ignoreversion
; Self-signed dev cert -- only present for local builds (not Azure-signed CI builds)
; skipifsourcedoesntexist allows the same .iss to work for both signed and dev builds
Source: "..\build\ClaudeFromHere-dev.cer";          DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; ---------------------------------------------------------------------------
; [Icons]
; ---------------------------------------------------------------------------

[Icons]
Name: "{userprograms}\Claude From Here\Claude From Here Settings"; Filename: "{app}\ClaudeFromHereConfig.exe"; IconFilename: "{app}\claude.ico"

; ---------------------------------------------------------------------------
; [Run] -- post-install actions
; ---------------------------------------------------------------------------

[Run]
; Import self-signed dev cert to LocalMachine\TrustedPeople (only present for local/dev builds).
; The .cer file is absent for Azure-signed CI builds, so this step is a no-op for production.
; certutil -addstore TrustedPeople requires admin; Inno Setup's Check function gates it.
; Note: In [Run] Parameters, {app} is expanded by Inno Setup before the shell sees it.
Filename: "powershell.exe"; \
  Parameters: "-ExecutionPolicy Bypass -NonInteractive -Command ""if (Test-Path '{app}\ClaudeFromHere-dev.cer') {{ Start-Process certutil -Verb RunAs -Wait -ArgumentList '-addstore TrustedPeople ""{app}\ClaudeFromHere-dev.cer""' }}"""; \
  Flags: runhidden waituntilterminated; \
  StatusMsg: "Importing signing certificate (dev build)..."

; Register MSIX sparse package with ExternalLocation pointing to install dir
Filename: "powershell.exe"; \
  Parameters: "-ExecutionPolicy Bypass -NonInteractive -Command ""Add-AppxPackage -Path '{app}\ClaudeFromHere.msix' -ExternalLocation '{app}' -ErrorAction Stop"""; \
  Flags: runhidden waituntilterminated; \
  StatusMsg: "Activating shell extension..."

; Explorer restart -- interactive mode (checkbox on final wizard page)
Filename: "powershell.exe"; \
  Parameters: "-ExecutionPolicy Bypass -NonInteractive -Command ""Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue; Start-Sleep -Milliseconds 500; Start-Process explorer.exe"""; \
  Flags: runhidden waituntilterminated skipifsilent postinstall; \
  Description: "Restart File Explorer now (required to activate 'Claude from here')"

; Explorer restart -- silent mode (automatic, no user interaction)
Filename: "powershell.exe"; \
  Parameters: "-ExecutionPolicy Bypass -NonInteractive -Command ""Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue; Start-Sleep -Milliseconds 500; Start-Process explorer.exe"""; \
  Flags: runhidden waituntilterminated skipifnotsilent

; ---------------------------------------------------------------------------
; [UninstallRun] -- uninstall actions
; ---------------------------------------------------------------------------

[UninstallRun]
; Remove MSIX registration
Filename: "powershell.exe"; \
  Parameters: "-ExecutionPolicy Bypass -NonInteractive -Command ""$pkg = Get-AppxPackage -Name 'ClaudeFromHere' -ErrorAction SilentlyContinue; if ($pkg) {{ Remove-AppxPackage -Package $pkg.PackageFullName }}"""; \
  Flags: runhidden waituntilterminated; \
  RunOnceId: "UnregisterMSIX"

; Restart Explorer to unload DLL
Filename: "powershell.exe"; \
  Parameters: "-ExecutionPolicy Bypass -NonInteractive -Command ""Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue; Start-Sleep -Milliseconds 500; Start-Process explorer.exe"""; \
  Flags: runhidden waituntilterminated; \
  RunOnceId: "RestartExplorer"

; ---------------------------------------------------------------------------
; [Code] -- Pascal scripting for upgrade DLL lock prevention
; ---------------------------------------------------------------------------

[Code]
// CurStepChanged(ssInstall) fires BEFORE Inno Setup copies files.
// On upgrade, Explorer holds a lock on ClaudeFromHere.dll from the previous
// installation. We must unregister the MSIX and kill Explorer to release
// the lock before file copy begins.
//
// Note: In [Code] Pascal strings, braces are NOT escaped (no double-brace).
// The Inno Setup constant preprocessor does not process Pascal string literals.

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssInstall then
  begin
    Exec('powershell.exe',
      '-ExecutionPolicy Bypass -NonInteractive -Command "' +
      '$pkg = Get-AppxPackage -Name ''ClaudeFromHere'' -ErrorAction SilentlyContinue; ' +
      'if ($pkg) { Remove-AppxPackage -Package $pkg.PackageFullName -ErrorAction SilentlyContinue }; ' +
      'Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue; ' +
      'Start-Sleep -Milliseconds 500"',
      '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    // ResultCode intentionally not checked -- fresh install has no existing package
  end;
end;
