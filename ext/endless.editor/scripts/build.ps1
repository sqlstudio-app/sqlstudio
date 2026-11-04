#requires -Version 7.0
# Copyright (c) 2026 dev4fun. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms, as
# designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
# This program is distributed in the hope that it will be useful,  but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
# the GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

<#
.SYNOPSIS
Build the React frontend and/or the native host on Windows, macOS, or Linux.
.EXAMPLE
pwsh ./scripts/build.ps1
.EXAMPLE
pwsh ./scripts/build.ps1 -Target React
.EXAMPLE
pwsh ./scripts/build.ps1 -Target Native -Configuration Debug
.EXAMPLE
pwsh ./scripts/build.ps1 -Platform Windows -InstallDependencies
.EXAMPLE
pwsh ./scripts/build.ps1 -Start
#>
[CmdletBinding()]
param(
    [ValidateSet('All', 'React', 'Native')]
    [string] $Target = 'All',

    [ValidateSet('Auto', 'Windows', 'macOS', 'Linux')]
    [string] $Platform = 'Auto',

    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',

    [switch] $InstallDependencies,

    [switch] $Start
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$workspacePath = Split-Path -Parent $PSScriptRoot

function Require-Tool([string] $Name, [string] $Instructions) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required tool '$Name' was not found. $Instructions"
    }
}

function Invoke-BuildTool([string] $Name, [string[]] $Arguments) {
    Write-Host "> $Name $($Arguments -join ' ')" -ForegroundColor Cyan
    & $Name @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE."
    }
}

function Test-BackendHealth([string] $Url) {
    try {
        $response = Invoke-WebRequest -Uri "$Url/api/health" -TimeoutSec 2
    }
    catch { return $false }
    if ($response.Content -notmatch '"status"\s*:\s*"ok"') {
        throw 'Port 8000 is serving an unexpected application. Stop that server before starting this backend.'
    }
    return $true
}

function Start-Backend {
    $apiUrl = 'http://127.0.0.1:8000'
    if (Test-BackendHealth $apiUrl) {
        Write-Host "Using running backend: $apiUrl" -ForegroundColor Green
        return
    }

    $backendDirectory = Join-Path $workspacePath 'backend'
    $pythonPath = if ($IsWindows) {
        Join-Path $backendDirectory '.venv\Scripts\python.exe'
    }
    else {
        Join-Path $backendDirectory '.venv/bin/python'
    }
    if (-not (Test-Path -LiteralPath $pythonPath)) {
        $pythonRelativePath = if ($IsWindows) { '.venv\Scripts\python.exe' } else { '.venv/bin/python' }
        throw "Backend environment is missing. From the repository root, run: python -m venv backend/.venv; backend/$pythonRelativePath -m pip install -r backend/requirements.txt"
    }

    $logDirectory = Join-Path $workspacePath 'build/logs'
    New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
    $processOptions = @{
        FilePath = $pythonPath
        ArgumentList = @('-m', 'uvicorn', 'app.main:app', '--app-dir', '.', '--host', '127.0.0.1', '--port', '8000')
        WorkingDirectory = $backendDirectory
        RedirectStandardOutput = Join-Path $logDirectory 'backend.log'
        RedirectStandardError = Join-Path $logDirectory 'backend-error.log'
        PassThru = $true
    }
    if ($IsWindows) { $processOptions.WindowStyle = 'Hidden' }
    $backendProcess = Start-Process @processOptions
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    try {
        while ($true) {
            if (Test-BackendHealth $apiUrl) {
                Write-Host "Backend ready: $apiUrl (PID $($backendProcess.Id))" -ForegroundColor Green
                return
            }
            if ($backendProcess.HasExited) { throw "Backend exited. See $logDirectory/backend-error.log." }
            if ([DateTime]::UtcNow -ge $deadline) { throw "Backend did not become ready in 30 seconds. See $logDirectory/backend-error.log." }
            Start-Sleep -Milliseconds 250
        }
    }
    catch {
        if (-not $backendProcess.HasExited) { Stop-Process -Id $backendProcess.Id -ErrorAction SilentlyContinue }
        throw
    }
}

$hostPlatform = if ($IsWindows) { 'Windows' } elseif ($IsMacOS) { 'macOS' } elseif ($IsLinux) { 'Linux' } else { throw 'Unsupported operating system.' }
$selectedPlatform = if ($Platform -eq 'Auto') { $hostPlatform } else { $Platform }
$buildReact = $Target -in 'All', 'React'
$buildNative = $Target -in 'All', 'Native'

# Resolve every project path relative to the workspace, regardless of caller cwd.
Push-Location -LiteralPath $workspacePath
try {
    if ($buildNative) {
        if ($selectedPlatform -ne $hostPlatform) {
            throw "Build the $selectedPlatform host on $selectedPlatform. This machine runs $hostPlatform; native cross-compilation is not configured. Use -Target React to build only the shared frontend."
        }
        switch ($selectedPlatform) {
            'Windows' { Require-Tool 'dotnet' 'Install the .NET 10 SDK.' }
            'macOS' { Require-Tool 'swift' 'Install Xcode Command Line Tools with Swift 5.9 or newer.' }
            'Linux' {
                Require-Tool 'meson' 'Install Meson, Ninja, a C compiler, GTK 4 and WebKitGTK 6 development packages.'
                Require-Tool 'ninja' 'Install Ninja.'
            }
        }
    }

    if ($buildReact -or $Start) {
        Require-Tool 'node' 'Install a Node.js version supported by web/package.json and Vite.'
        $npm = if ($IsWindows) { 'npm.cmd' } else { 'npm' }
        Require-Tool $npm 'Install Node.js with npm.'
        if ($InstallDependencies -or -not (Test-Path -LiteralPath 'web/node_modules')) {
            Invoke-BuildTool $npm @('--prefix', 'web', 'ci')
        }
        if ($buildReact) {
            Invoke-BuildTool $npm @('--prefix', 'web', 'run', 'build')
            Write-Host "React output: $(Join-Path $workspacePath 'web/dist')" -ForegroundColor Green
        }
    }

    if ($buildNative) {
        if (-not $Start -and -not (Test-Path -LiteralPath 'web/dist/index.html')) {
            throw 'React output is missing. Run ./scripts/build.ps1 -Target React first, or use -Target All.'
        }
        $nativeConfig = $Configuration.ToLowerInvariant()
        switch ($selectedPlatform) {
            'Windows' {
                Invoke-BuildTool 'dotnet' @('build', 'hosts/windows/WindowsHost.csproj', '--configuration', $Configuration, '--nologo')
                $output = "hosts/windows/bin/$Configuration/net10.0-windows/WindowsHost.exe"
            }
            'macOS' {
                Invoke-BuildTool 'swift' @('build', '--package-path', 'hosts/macos', '--configuration', $nativeConfig)
                $output = "hosts/macos/.build/$nativeConfig/endless-control-mac"
            }
            'Linux' {
                $buildDirectory = "build/linux/$nativeConfig"
                $setup = @('setup', $buildDirectory, 'hosts/linux', '--buildtype', $nativeConfig)
                if (Test-Path -LiteralPath "$buildDirectory/meson-private/coredata.dat") {
                    $setup += '--reconfigure'
                }
                Invoke-BuildTool 'meson' $setup
                Invoke-BuildTool 'meson' @('compile', '-C', $buildDirectory)
                $output = "$buildDirectory/endless-control"
            }
        }
        Write-Host "Native output: $(Join-Path $workspacePath $output)" -ForegroundColor Green
        if ($selectedPlatform -ne 'Windows') {
            Write-Host "When launching, set ENDLESS_CONTROL_WEB_ROOT to $(Join-Path $workspacePath 'web/dist')."
        }
    }

    if ($Start) {
        Start-Backend
        $launchArguments = @{}
        if ($buildNative) { $launchArguments.NativeExecutable = Join-Path $workspacePath $output }
        & (Join-Path $PSScriptRoot 'start-dev.ps1') @launchArguments
    }
}
catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}
finally {
    Pop-Location
}
