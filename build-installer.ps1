#Requires -Version 5.1
<#
.SYNOPSIS
    Build orchestration script for ClaudeFromHere installer.
    Compiles DLL, builds config app, copies assets, packs and signs MSIX,
    then invokes Inno Setup to produce the final .exe installer.

.DESCRIPTION
    Steps:
      1  Build DLL via CMake + MSVC
      2  Build config app via dotnet
      3  Copy assets (icons, AppxManifest, package assets) to build/
      4  Pack MSIX with MakeAppx
      5  Sign MSIX with Azure Trusted Signing (optional)
      6  Build installer with Inno Setup ISCC

    Output: dist\ClaudeFromHere-Setup.exe

.PARAMETER SkipBuild
    Skip Steps 1-2 (reuse existing compiled artifacts in build/).

.PARAMETER SkipSign
    Skip Step 5 (produce unsigned MSIX for local dev builds).

.EXAMPLE
    .\build-installer.ps1
    .\build-installer.ps1 -SkipBuild
    .\build-installer.ps1 -SkipSign
    .\build-installer.ps1 -SkipBuild -SkipSign

.NOTES
    Environment variables for Azure Trusted Signing (Step 5):
      CFH_SIGNING_ENDPOINT  - Signing endpoint URL (default: https://eus.codesigning.azure.net/)
      CFH_SIGNING_ACCOUNT   - Artifact Signing account name (required for signing)
      CFH_SIGNING_PROFILE   - Certificate profile name (required for signing)
#>

param(
    [switch]$SkipBuild,
    [switch]$SkipSign
)

$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

function Write-Step([string]$Msg) {
    Write-Host ""
    Write-Host "==> $Msg" -ForegroundColor Cyan
}

function Invoke-ExternalTool([string]$Description, [string]$Exe, [string[]]$ToolArgs) {
    Write-Host "    Running: $Exe $ToolArgs"
    & $Exe @ToolArgs
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE"
    }
}

# ---------------------------------------------------------------------------
# Resolve paths
# ---------------------------------------------------------------------------

$RepoRoot   = $PSScriptRoot
$BuildDir   = Join-Path $RepoRoot "build"
$PackageDir = Join-Path $RepoRoot "package"
$AssetsDir  = Join-Path $RepoRoot "assets"
$DistDir    = Join-Path $RepoRoot "dist"

Write-Host "ClaudeFromHere Installer Build" -ForegroundColor Green
Write-Host "  RepoRoot : $RepoRoot"
Write-Host "  BuildDir : $BuildDir"

# ---------------------------------------------------------------------------
# SDK tool paths
# ---------------------------------------------------------------------------

$CMake    = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$MakeAppx = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\makeappx.exe"
$ISCC     = if (Test-Path "C:\Program Files (x86)\Inno Setup 6\ISCC.exe") {
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
} elseif (Test-Path "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe") {
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
} else {
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"  # fallback, will fail with helpful error
}

# ---------------------------------------------------------------------------
# Step 1: Build DLL via CMake + MSVC
# ---------------------------------------------------------------------------

if (-not $SkipBuild) {
    Write-Step "Step 1: Building DLL via CMake + MSVC"

    if (-not (Test-Path $CMake)) {
        throw "CMake not found at: $CMake`nInstall Visual Studio 2022 Build Tools with C++ CMake support"
    }

    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
        Write-Host "    Created build directory: $BuildDir"
    }

    Invoke-ExternalTool "CMake configure" $CMake @(
        "-S", $RepoRoot,
        "-B", $BuildDir,
        "-G", "Visual Studio 17 2022",
        "-A", "x64"
    )

    Invoke-ExternalTool "CMake build" $CMake @(
        "--build", $BuildDir,
        "--config", "Release"
    )

    # CMake with VS generator puts output in Release/ subdirectory
    $DllReleasePath = Join-Path $BuildDir "Release\ClaudeFromHere.dll"
    $ExeReleasePath = Join-Path $BuildDir "Release\ClaudeFromHere.exe"

    if (-not (Test-Path $DllReleasePath)) {
        throw "Build succeeded but DLL not found at: $DllReleasePath"
    }

    Write-Host "    Copying artifacts from Release\ to build root..."
    Copy-Item -Path $DllReleasePath -Destination (Join-Path $BuildDir "ClaudeFromHere.dll") -Force
    Write-Host "    DLL ready at: $BuildDir\ClaudeFromHere.dll"

    if (Test-Path $ExeReleasePath) {
        Copy-Item -Path $ExeReleasePath -Destination (Join-Path $BuildDir "ClaudeFromHere.exe") -Force
        Write-Host "    Stub EXE ready at: $BuildDir\ClaudeFromHere.exe"
    } else {
        Write-Warning "    Stub EXE not found at $ExeReleasePath"
    }
} else {
    Write-Step "Step 1: Skipped (SkipBuild specified)"

    if (-not (Test-Path (Join-Path $BuildDir "ClaudeFromHere.dll"))) {
        throw "SkipBuild specified but DLL not found at: $BuildDir\ClaudeFromHere.dll"
    }
    Write-Host "    Using existing DLL: $BuildDir\ClaudeFromHere.dll"
}

# ---------------------------------------------------------------------------
# Step 2: Build config app
# ---------------------------------------------------------------------------

if (-not $SkipBuild) {
    Write-Step "Step 2: Building config app (ClaudeFromHereConfig)"

    $ConfigCsproj = Join-Path $RepoRoot "src\ClaudeFromHereConfig\ClaudeFromHereConfig.csproj"

    if (-not (Test-Path $ConfigCsproj)) {
        throw "Config app project not found at: $ConfigCsproj"
    }

    Invoke-ExternalTool "dotnet build config app" "dotnet" @(
        "build", $ConfigCsproj,
        "--configuration", "Release",
        "--nologo",
        "-v", "minimal"
    )

    $ConfigExe = Join-Path $BuildDir "ClaudeFromHereConfig.exe"
    if (-not (Test-Path $ConfigExe)) {
        throw "Config app build succeeded but exe not found at: $ConfigExe"
    }
    Write-Host "    Config app ready at: $ConfigExe"
} else {
    Write-Step "Step 2: Skipped (SkipBuild specified)"

    $ConfigExe = Join-Path $BuildDir "ClaudeFromHereConfig.exe"
    if (-not (Test-Path $ConfigExe)) {
        Write-Warning "    Config app exe not found at: $ConfigExe"
    } else {
        Write-Host "    Using existing config app: $ConfigExe"
    }
}

# ---------------------------------------------------------------------------
# Step 3: Copy assets to build directory
# ---------------------------------------------------------------------------

Write-Step "Step 3: Copying assets to build directory"

# Copy claude.ico
$IcoSource = Join-Path $AssetsDir "claude.ico"
$IcoDest   = Join-Path $BuildDir "claude.ico"

if (Test-Path $IcoSource) {
    Copy-Item -Path $IcoSource -Destination $IcoDest -Force
    Write-Host "    Copied claude.ico to: $IcoDest"
} else {
    Write-Warning "    claude.ico not found at $IcoSource"
}

# Copy AppxManifest.xml
$ManifestSource = Join-Path $PackageDir "AppxManifest.xml"
$ManifestDest   = Join-Path $BuildDir "AppxManifest.xml"

if (Test-Path $ManifestSource) {
    Copy-Item -Path $ManifestSource -Destination $ManifestDest -Force
    Write-Host "    Copied AppxManifest.xml to: $ManifestDest"
} else {
    throw "AppxManifest.xml not found at: $ManifestSource"
}

# Copy Assets folder
$PkgAssetsSource = Join-Path $PackageDir "Assets"
$PkgAssetsDest   = Join-Path $BuildDir "Assets"

if (Test-Path $PkgAssetsSource) {
    if (-not (Test-Path $PkgAssetsDest)) {
        New-Item -ItemType Directory -Path $PkgAssetsDest | Out-Null
    }
    Copy-Item -Path (Join-Path $PkgAssetsSource "*") -Destination $PkgAssetsDest -Recurse -Force
    Write-Host "    Copied Assets\ to: $PkgAssetsDest"
} else {
    throw "Package Assets folder not found at: $PkgAssetsSource"
}

# ---------------------------------------------------------------------------
# Step 4: Pack MSIX
# ---------------------------------------------------------------------------

Write-Step "Step 4: Packing MSIX"

if (-not (Test-Path $MakeAppx)) {
    throw "MakeAppx.exe not found at: $MakeAppx`nInstall Windows SDK 10.0.26100.0"
}

$MsixPath = Join-Path $BuildDir "ClaudeFromHere.msix"

Invoke-ExternalTool "MakeAppx pack" $MakeAppx @(
    "pack",
    "/o",
    "/d", $PackageDir,
    "/nv",
    "/p", $MsixPath
)

Write-Host "    MSIX packed: $MsixPath"

# ---------------------------------------------------------------------------
# Step 5: Sign MSIX with Azure Trusted Signing
# ---------------------------------------------------------------------------

if (-not $SkipSign) {
    Write-Step "Step 5: Signing MSIX"

    $tsEndpoint = if ($env:CFH_SIGNING_ENDPOINT) { $env:CFH_SIGNING_ENDPOINT } else { "https://eus.codesigning.azure.net/" }
    $tsAccount  = if ($env:CFH_SIGNING_ACCOUNT)  { $env:CFH_SIGNING_ACCOUNT  } else { $null }
    $tsProfile  = if ($env:CFH_SIGNING_PROFILE)  { $env:CFH_SIGNING_PROFILE  } else { $null }

    if ($tsAccount -and $tsProfile) {
        # --- Azure Trusted Signing (CI / production) ---
        $signTool = Get-Command sign -ErrorAction SilentlyContinue
        if (-not $signTool) {
            throw "'sign' dotnet tool not found. Install with: dotnet tool install --global sign --prerelease"
        }

        Write-Host "    Endpoint: $tsEndpoint"
        Write-Host "    Account:  $tsAccount"
        Write-Host "    Profile:  $tsProfile"

        sign code artifact-signing $MsixPath `
            --artifact-signing-endpoint $tsEndpoint `
            --artifact-signing-account $tsAccount `
            --artifact-signing-certificate-profile $tsProfile `
            --azure-credential-type azure-cli

        if ($LASTEXITCODE -ne 0) {
            throw "MSIX signing failed with exit code $LASTEXITCODE"
        }

        Write-Host "    MSIX signed via Azure Trusted Signing."
    } else {
        # --- Self-signed fallback (local dev builds) ---
        Write-Host "    Azure Trusted Signing env vars not set. Falling back to self-signed cert for local build."
        Write-Host "    Set CFH_SIGNING_ACCOUNT and CFH_SIGNING_PROFILE to use Azure Trusted Signing."

        $SignTool     = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"
        if (-not (Test-Path $SignTool)) {
            throw "signtool.exe not found at: $SignTool`nInstall Windows SDK 10.0.26100.0"
        }

        # Publisher CN must match AppxManifest exactly
        $CertSubject  = "CN=Joseph Fago, O=Joseph Fago, L=Newark, S=New Jersey, C=US"
        $PfxPath      = Join-Path $BuildDir "ClaudeFromHere-dev.pfx"
        $PwdPath      = Join-Path $BuildDir "ClaudeFromHere-dev-cert-password.txt"
        $CerPath      = Join-Path $BuildDir "ClaudeFromHere-dev.cer"

        # Reuse existing cert if already in the machine trust store
        $existingCert = Get-ChildItem "Cert:\LocalMachine\TrustedPeople" -ErrorAction SilentlyContinue |
            Where-Object { $_.Subject -eq $CertSubject } |
            Select-Object -First 1

        $pfxPassword = $null

        if ($existingCert -and (Test-Path $PfxPath) -and (Test-Path $PwdPath)) {
            Write-Host "    Reusing existing self-signed cert (thumbprint: $($existingCert.Thumbprint))"
            $pfxPassword = (Get-Content $PwdPath).Trim()
        } else {
            Write-Host "    Generating self-signed certificate for local build..."

            $cert = New-SelfSignedCertificate `
                -Type Custom `
                -Subject $CertSubject `
                -KeyUsage DigitalSignature `
                -CertStoreLocation "Cert:\CurrentUser\My" `
                -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3")

            $pfxPassword = [System.Guid]::NewGuid().ToString()
            $secPwd = ConvertTo-SecureString -String $pfxPassword -Force -AsPlainText

            Export-PfxCertificate -Cert $cert -FilePath $PfxPath -Password $secPwd | Out-Null
            $pfxPassword | Out-File -FilePath $PwdPath -Encoding ascii -NoNewline
            Export-Certificate -Cert $cert -FilePath $CerPath -Type CERT | Out-Null

            Write-Host "    Certificate created (thumbprint: $($cert.Thumbprint))"
            Write-Host "    PFX: $PfxPath"
            Write-Host "    CER: $CerPath"

            # Import to LocalMachine\TrustedPeople - required for Add-AppxPackage to trust the MSIX
            # This step needs admin elevation
            $id        = [System.Security.Principal.WindowsIdentity]::GetCurrent()
            $principal = New-Object System.Security.Principal.WindowsPrincipal($id)
            $isAdmin   = $principal.IsInRole([System.Security.Principal.WindowsBuiltInRole]::Administrator)

            if ($isAdmin) {
                Import-PfxCertificate -FilePath $PfxPath -CertStoreLocation "Cert:\LocalMachine\TrustedPeople" -Password $secPwd | Out-Null
                Write-Host "    Certificate imported to LocalMachine\TrustedPeople."
            } else {
                Write-Host "    Requesting elevation to import cert to LocalMachine\TrustedPeople..."
                $importCmd = "Import-PfxCertificate -FilePath '$PfxPath' -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople' -Password (ConvertTo-SecureString -String '$pfxPassword' -Force -AsPlainText)"
                Start-Process powershell -Verb RunAs -ArgumentList "-NoProfile -Command $importCmd" -Wait

                $imported = Get-ChildItem "Cert:\LocalMachine\TrustedPeople" -ErrorAction SilentlyContinue |
                    Where-Object { $_.Subject -eq $CertSubject } |
                    Select-Object -First 1
                if (-not $imported) {
                    throw "Cert import to LocalMachine\TrustedPeople failed or was cancelled. Cannot sign MSIX."
                }
                Write-Host "    Certificate import confirmed."
            }
        }

        # Export .cer if not already present (needed by installer to trust the signed MSIX on the target machine)
        if (-not (Test-Path $CerPath)) {
            $userCert = Get-ChildItem "Cert:\CurrentUser\My" -ErrorAction SilentlyContinue |
                Where-Object { $_.Subject -eq $CertSubject } |
                Select-Object -First 1
            if ($userCert) {
                Export-Certificate -Cert $userCert -FilePath $CerPath -Type CERT | Out-Null
                Write-Host "    CER exported: $CerPath"
            } else {
                Write-Warning "    Could not export .cer - installer will not import cert on target machine."
            }
        }

        Invoke-ExternalTool "SignTool sign" $SignTool @(
            "sign",
            "/fd", "SHA256",
            "/a",
            "/f", $PfxPath,
            "/p", $pfxPassword,
            $MsixPath
        )

        Write-Host "    MSIX signed with self-signed cert (local dev build)."
        Write-Host "    NOTE: The installer will import the self-signed cert to the user's machine."
    }
} else {
    Write-Step "Step 5: Skipped (SkipSign specified)"
    Write-Warning "    MSIX is unsigned. Install will fail unless Developer Mode is enabled."
    Write-Warning "    Use -SkipSign only for inspecting the package, not for testing registration."
}

# ---------------------------------------------------------------------------
# Step 6: Build installer with ISCC
# ---------------------------------------------------------------------------

Write-Step "Step 6: Building installer with Inno Setup"

if (-not (Test-Path $ISCC)) {
    throw "Inno Setup compiler not found at: $ISCC`nInstall with: winget install --id JRSoftware.InnoSetup"
}

$IssFile = Join-Path $RepoRoot "installer\ClaudeFromHere.iss"

if (-not (Test-Path $IssFile)) {
    throw "Inno Setup script not found at: $IssFile"
}

Invoke-ExternalTool "ISCC compile" $ISCC @($IssFile)

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------

$SetupExe = Join-Path $DistDir "ClaudeFromHere-Setup.exe"

Write-Host ""
Write-Host "Build complete!" -ForegroundColor Green

if (Test-Path $SetupExe) {
    $size = [math]::Round((Get-Item $SetupExe).Length / 1MB, 1)
    Write-Host "  Installer: $SetupExe" -ForegroundColor Green
    Write-Host "  Size: $size MB" -ForegroundColor Green
} else {
    Write-Host "  Expected output at: $SetupExe" -ForegroundColor Yellow
    Write-Host "  Check Inno Setup output configuration." -ForegroundColor Yellow
}
