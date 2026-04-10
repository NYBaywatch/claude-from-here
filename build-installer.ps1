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
$ISCC     = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"

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
    Write-Step "Step 5: Signing MSIX with Azure Trusted Signing"

    $tsEndpoint = if ($env:CFH_SIGNING_ENDPOINT) { $env:CFH_SIGNING_ENDPOINT } else { "https://eus.codesigning.azure.net/" }
    $tsAccount  = if ($env:CFH_SIGNING_ACCOUNT)  { $env:CFH_SIGNING_ACCOUNT  } else { $null }
    $tsProfile  = if ($env:CFH_SIGNING_PROFILE)  { $env:CFH_SIGNING_PROFILE  } else { $null }

    if (-not $tsAccount -or -not $tsProfile) {
        Write-Warning "    Azure Trusted Signing env vars not set. MSIX will be unsigned."
        Write-Warning "    Set CFH_SIGNING_ACCOUNT and CFH_SIGNING_PROFILE to enable signing."
        Write-Warning "    Optionally set CFH_SIGNING_ENDPOINT (default: https://eus.codesigning.azure.net/)"
    } else {
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

        Write-Host "    MSIX signed successfully."
    }
} else {
    Write-Step "Step 5: Skipped (SkipSign specified)"
    Write-Host "    MSIX will be unsigned (dev build)."
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
