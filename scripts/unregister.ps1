#Requires -Version 5.1
<#
.SYNOPSIS
    Remove the ClaudeFromHere shell extension package and optionally delete the dev certificate.

.DESCRIPTION
    Removes the registered AppxPackage and restarts Explorer.
    With -RemoveCert, also deletes the self-signed certificate from both the
    LocalMachine\TrustedPeople store (requires elevation) and CurrentUser\My.

.PARAMETER RemoveCert
    Also remove the ClaudeFromHere self-signed certificate from all stores.

.PARAMETER CertSubject
    DN of the cert to remove. Default: CN=ClaudeFromHere

.EXAMPLE
    .\unregister.ps1
    .\unregister.ps1 -RemoveCert
#>

param(
    [switch]$RemoveCert,
    [string]$CertSubject = "CN=ClaudeFromHere"
)

$ErrorActionPreference = "Stop"

function Test-IsElevated {
    $id = [System.Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object System.Security.Principal.WindowsPrincipal($id)
    return $principal.IsInRole([System.Security.Principal.WindowsBuiltInRole]::Administrator)
}

Write-Host "ClaudeFromHere unregistration" -ForegroundColor Yellow

# ---------------------------------------------------------------------------
# Remove AppxPackage
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "==> Removing package..." -ForegroundColor Cyan

$pkg = Get-AppxPackage -Name "ClaudeFromHere" -ErrorAction SilentlyContinue
if ($pkg) {
    Write-Host "    Removing: $($pkg.PackageFullName)"
    Remove-AppxPackage -Package $pkg.PackageFullName
    Write-Host "    Package removed."
} else {
    Write-Host "    No ClaudeFromHere package found. Nothing to remove."
}

# ---------------------------------------------------------------------------
# Remove certificate (optional)
# ---------------------------------------------------------------------------

if ($RemoveCert) {
    Write-Host ""
    Write-Host "==> Removing certificate ($CertSubject)..." -ForegroundColor Cyan

    # Remove from CurrentUser\My (no elevation needed)
    $userCerts = Get-ChildItem "Cert:\CurrentUser\My" -ErrorAction SilentlyContinue |
        Where-Object { $_.Subject -eq $CertSubject }

    foreach ($c in $userCerts) {
        Remove-Item "Cert:\CurrentUser\My\$($c.Thumbprint)" -Force
        Write-Host "    Removed from CurrentUser\My: $($c.Thumbprint)"
    }

    # Remove from LocalMachine\TrustedPeople (requires elevation)
    $machineCerts = Get-ChildItem "Cert:\LocalMachine\TrustedPeople" -ErrorAction SilentlyContinue |
        Where-Object { $_.Subject -eq $CertSubject }

    if ($machineCerts) {
        if (Test-IsElevated) {
            foreach ($c in $machineCerts) {
                Remove-Item "Cert:\LocalMachine\TrustedPeople\$($c.Thumbprint)" -Force
                Write-Host "    Removed from LocalMachine\TrustedPeople: $($c.Thumbprint)"
            }
        } else {
            Write-Host "    Admin elevation required to remove from LocalMachine\TrustedPeople."
            $thumbprints = ($machineCerts | ForEach-Object { $_.Thumbprint }) -join "','"
            $removeCmd = "Get-ChildItem 'Cert:\LocalMachine\TrustedPeople' | Where-Object { @('$thumbprints') -contains `$_.Thumbprint } | Remove-Item -Force"
            Start-Process powershell -Verb RunAs -ArgumentList "-NoProfile -Command $removeCmd" -Wait
            Write-Host "    Elevated removal complete."
        }
    } else {
        Write-Host "    No cert found in LocalMachine\TrustedPeople."
    }
}

# ---------------------------------------------------------------------------
# Restart Explorer
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "==> Restarting Explorer..." -ForegroundColor Cyan
Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "Unregistered. 'Claude from here' context menu item removed." -ForegroundColor Green
