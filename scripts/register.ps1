#Requires -Version 5.1
<#
.SYNOPSIS
    Full dev registration workflow: build DLL, create/import cert, pack MSIX, sign, and register
    the ClaudeFromHere shell extension.

.DESCRIPTION
    Steps:
      0  Build the DLL via CMake + MSVC (unless -SkipBuild)
      1  Generate self-signed cert and import to LocalMachine\TrustedPeople (admin once)
      2  Copy assets (claude.ico) to build directory
      3  Pack MSIX with MakeAppx
      4  Sign MSIX with SignTool
      5  Unregister any existing package (idempotent)
      6  Register with Add-AppxPackage -ExternalLocation
      7  Restart Explorer so the DLL is loaded

.PARAMETER BuildDir
    Path to the build output directory. Default: ..\build (relative to this script).

.PARAMETER PackageDir
    Path to the directory containing AppxManifest.xml and Assets\. Default: ..\package.

.PARAMETER CertSubject
    Distinguished name for the self-signed certificate. Must match AppxManifest Publisher exactly.
    Default: CN=ClaudeFromHere

.PARAMETER CertFriendlyName
    Friendly name for the certificate shown in Cert Manager. Default: ClaudeFromHere Dev Cert

.PARAMETER SkipBuild
    Skip Step 0 (CMake configure + build). Use when the DLL is already up to date.

.EXAMPLE
    .\register.ps1
    .\register.ps1 -SkipBuild
    .\register.ps1 -BuildDir C:\MyBuild -SkipBuild
#>

param(
    [string]$BuildDir = "$PSScriptRoot\..\build",
    [string]$PackageDir = "$PSScriptRoot\..\package",
    [string]$CertSubject = "CN=ClaudeFromHere",
    [string]$CertFriendlyName = "ClaudeFromHere Dev Cert",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

function Write-Step([string]$Msg) {
    Write-Host ""
    Write-Host "==> $Msg" -ForegroundColor Cyan
}

function Invoke-ExternalTool([string]$Description, [string]$Exe, [string[]]$Args) {
    Write-Host "    Running: $Exe $Args"
    & $Exe @Args
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE"
    }
}

function Test-IsElevated {
    $id = [System.Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object System.Security.Principal.WindowsPrincipal($id)
    return $principal.IsInRole([System.Security.Principal.WindowsBuiltInRole]::Administrator)
}

# ---------------------------------------------------------------------------
# Resolve paths to absolute
# ---------------------------------------------------------------------------

$BuildDir   = [System.IO.Path]::GetFullPath($BuildDir)
$PackageDir = [System.IO.Path]::GetFullPath($PackageDir)
$AssetsDir  = [System.IO.Path]::GetFullPath("$PSScriptRoot\..\assets")

Write-Host "ClaudeFromHere dev registration workflow" -ForegroundColor Green
Write-Host "  BuildDir   : $BuildDir"
Write-Host "  PackageDir : $PackageDir"
Write-Host "  CertSubject: $CertSubject"

# ---------------------------------------------------------------------------
# SDK tool paths
# ---------------------------------------------------------------------------

$MakeAppx = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\makeappx.exe"
$SignTool  = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"
$CMake    = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

foreach ($tool in @($MakeAppx, $SignTool)) {
    if (-not (Test-Path $tool)) {
        throw "Required SDK tool not found: $tool`nInstall Windows SDK 10.0.26100.0"
    }
}

# ---------------------------------------------------------------------------
# Step 0 -- Build DLL via CMake
# ---------------------------------------------------------------------------

if (-not $SkipBuild) {
    Write-Step "Step 0: Building DLL via CMake + MSVC"

    if (-not (Test-Path $CMake)) {
        throw "CMake not found at: $CMake`nInstall Visual Studio 2022 Build Tools with C++ CMake support"
    }

    $SourceDir = [System.IO.Path]::GetFullPath("$PSScriptRoot\..")

    # Ensure build directory exists before cmake configure
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
        Write-Host "    Created build directory: $BuildDir"
    }

    Invoke-ExternalTool "CMake configure" $CMake @(
        "-S", $SourceDir,
        "-B", $BuildDir,
        "-G", "Visual Studio 17 2022",
        "-A", "x64"
    )

    Invoke-ExternalTool "CMake build" $CMake @(
        "--build", $BuildDir,
        "--config", "Release"
    )

    # CMake with VS generator puts output in Release/ subdirectory
    $DllReleasePath = "$BuildDir\Release\ClaudeFromHere.dll"
    if (-not (Test-Path $DllReleasePath)) {
        throw "Build succeeded but DLL not found at expected path: $DllReleasePath"
    }

    # Copy DLL up one level so -ExternalLocation finds it alongside the MSIX
    Write-Host "    Copying DLL from Release\ to build root..."
    Copy-Item -Path $DllReleasePath -Destination "$BuildDir\ClaudeFromHere.dll" -Force
    Write-Host "    DLL ready at: $BuildDir\ClaudeFromHere.dll"
} else {
    Write-Step "Step 0: Skipped (SkipBuild specified)"

    if (-not (Test-Path "$BuildDir\ClaudeFromHere.dll")) {
        throw "SkipBuild specified but DLL not found at: $BuildDir\ClaudeFromHere.dll"
    }
    Write-Host "    Using existing DLL: $BuildDir\ClaudeFromHere.dll"
}

# ---------------------------------------------------------------------------
# Step 1 -- Certificate: generate or reuse
# ---------------------------------------------------------------------------

Write-Step "Step 1: Certificate setup"

$pfxPath      = "$BuildDir\ClaudeFromHere.pfx"
$pwdPath      = "$BuildDir\cert-password.txt"
$certThumbprint = $null
$pfxPassword  = $null

# Check if the cert is already in the machine trust store
$existingCert = Get-ChildItem "Cert:\LocalMachine\TrustedPeople" -ErrorAction SilentlyContinue |
    Where-Object { $_.Subject -eq $CertSubject } |
    Select-Object -First 1

if ($existingCert) {
    Write-Host "    Certificate already imported (thumbprint: $($existingCert.Thumbprint)), skipping generation."
    $certThumbprint = $existingCert.Thumbprint

    # We still need a PFX to sign with. Look for it in the user store.
    $userCert = Get-ChildItem "Cert:\CurrentUser\My" -ErrorAction SilentlyContinue |
        Where-Object { $_.Thumbprint -eq $certThumbprint } |
        Select-Object -First 1

    if ($userCert -and (Test-Path $pwdPath)) {
        $pfxPassword = (Get-Content $pwdPath).Trim()
        Write-Host "    Found existing PFX password, will re-export PFX for signing."
        $secPwd = ConvertTo-SecureString -String $pfxPassword -Force -AsPlainText
        Export-PfxCertificate -Cert $userCert -FilePath $pfxPath -Password $secPwd | Out-Null
    } elseif (Test-Path $pfxPath) {
        if (Test-Path $pwdPath) {
            $pfxPassword = (Get-Content $pwdPath).Trim()
            Write-Host "    Using existing PFX at: $pfxPath"
        } else {
            throw "PFX exists at $pfxPath but cert-password.txt not found. Cannot sign MSIX."
        }
    } else {
        throw "Cert is imported but private key not found in CurrentUser\My and no PFX on disk.`nRun without -SkipBuild from a clean state to regenerate."
    }
} else {
    Write-Host "    Generating self-signed certificate..."

    $cert = New-SelfSignedCertificate `
        -Type Custom `
        -Subject $CertSubject `
        -FriendlyName $CertFriendlyName `
        -KeyUsage DigitalSignature `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3")

    $certThumbprint = $cert.Thumbprint
    Write-Host "    Certificate created (thumbprint: $certThumbprint)"

    # Generate random PFX password and persist it
    $pfxPassword = [System.Guid]::NewGuid().ToString()
    $secPwd = ConvertTo-SecureString -String $pfxPassword -Force -AsPlainText

    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }

    Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $secPwd | Out-Null
    $pfxPassword | Out-File -FilePath $pwdPath -Encoding ascii -NoNewline
    Write-Host "    PFX exported to: $pfxPath"
    Write-Host "    Password saved to: $pwdPath"

    # Import to LocalMachine\TrustedPeople (requires admin)
    if (Test-IsElevated) {
        Write-Host "    Running elevated: importing cert to LocalMachine\TrustedPeople..."
        Import-PfxCertificate -FilePath $pfxPath -CertStoreLocation "Cert:\LocalMachine\TrustedPeople" -Password $secPwd | Out-Null
        Write-Host "    Certificate imported successfully."
    } else {
        Write-Host "    Admin elevation required for cert import to LocalMachine\TrustedPeople..."
        Write-Host "    Launching elevated PowerShell to import cert. Accept the UAC prompt."

        $importCmd = "Import-PfxCertificate -FilePath '$pfxPath' -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople' -Password (ConvertTo-SecureString -String '$pfxPassword' -Force -AsPlainText)"
        Start-Process powershell -Verb RunAs -ArgumentList "-NoProfile -Command $importCmd" -Wait

        # Verify import succeeded
        $importedCert = Get-ChildItem "Cert:\LocalMachine\TrustedPeople" -ErrorAction SilentlyContinue |
            Where-Object { $_.Subject -eq $CertSubject } |
            Select-Object -First 1

        if (-not $importedCert) {
            throw "Cert import to LocalMachine\TrustedPeople failed or was cancelled. Registration cannot continue."
        }
        Write-Host "    Certificate import confirmed."
    }
}

# ---------------------------------------------------------------------------
# Step 2 -- Copy assets to build directory
# ---------------------------------------------------------------------------

Write-Step "Step 2: Copying assets to build directory"

$icoSource = "$AssetsDir\claude.ico"
$icoDest   = "$BuildDir\claude.ico"

if (Test-Path $icoSource) {
    Copy-Item -Path $icoSource -Destination $icoDest -Force
    Write-Host "    Copied claude.ico to: $icoDest"
} else {
    Write-Warning "    claude.ico not found at $icoSource -- icon will be missing from context menu"
}

# ---------------------------------------------------------------------------
# Step 3 -- Pack MSIX
# ---------------------------------------------------------------------------

Write-Step "Step 3: Packing MSIX"

$msixPath = "$BuildDir\ClaudeFromHere.msix"

Invoke-ExternalTool "MakeAppx pack" $MakeAppx @(
    "pack",
    "/o",
    "/d", $PackageDir,
    "/nv",
    "/p", $msixPath
)

Write-Host "    MSIX packed: $msixPath"

# ---------------------------------------------------------------------------
# Step 4 -- Sign MSIX
# ---------------------------------------------------------------------------

Write-Step "Step 4: Signing MSIX"

Invoke-ExternalTool "SignTool sign" $SignTool @(
    "sign",
    "/fd", "SHA256",
    "/a",
    "/f", $pfxPath,
    "/p", $pfxPassword,
    $msixPath
)

Write-Host "    MSIX signed successfully."

# ---------------------------------------------------------------------------
# Step 5 -- Unregister existing package (idempotent)
# ---------------------------------------------------------------------------

Write-Step "Step 5: Removing any existing registration"

$existing = Get-AppxPackage -Name "ClaudeFromHere" -ErrorAction SilentlyContinue
if ($existing) {
    Write-Host "    Removing: $($existing.PackageFullName)"
    Remove-AppxPackage -Package $existing.PackageFullName
    Write-Host "    Removed."
} else {
    Write-Host "    No existing registration found."
}

# ---------------------------------------------------------------------------
# Step 6 -- Register with ExternalLocation
# ---------------------------------------------------------------------------

Write-Step "Step 6: Registering package"

Write-Host "    ExternalLocation: $BuildDir"
Add-AppxPackage -Path $msixPath -ExternalLocation $BuildDir

$registered = Get-AppxPackage -Name "ClaudeFromHere" -ErrorAction SilentlyContinue
if (-not $registered) {
    throw "Package registration failed. Run Get-AppxPackage -Name ClaudeFromHere to diagnose."
}
Write-Host "    Package registered: $($registered.PackageFullName)"

# ---------------------------------------------------------------------------
# Step 7 -- Restart Explorer
# ---------------------------------------------------------------------------

Write-Step "Step 7: Restarting Explorer to load extension"

Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
# Explorer auto-restarts on Windows 11 via Desktop Window Manager

Write-Host "    Explorer restarted."

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "Registration complete!" -ForegroundColor Green
Write-Host "Right-click a folder (or folder background) in Explorer to test 'Claude from here'."
