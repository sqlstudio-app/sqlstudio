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

[CmdletBinding()]
param([string] $NativeExecutable)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$workspacePath = Split-Path -Parent $PSScriptRoot
$devUrl = 'http://127.0.0.1:5174'
$server = $null
$previousUrl = $env:ENDLESS_CONTROL_WEB_URL

function Test-DevServer {
    try {
        $response = Invoke-WebRequest -Uri $devUrl -TimeoutSec 2
    }
    catch { return $false }
    if ($response.Content -notmatch '/@vite/client' -or $response.Content -notmatch 'SQL Notebook \| Endless Control') {
        throw "Port 5173 is serving another application. Stop that server before starting this notebook."
    }
    return $true
}

try {
    if (-not (Test-DevServer)) {
        $logDirectory = Join-Path $workspacePath 'build/logs'
        New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
        $nodePath = (Get-Command node -ErrorAction Stop).Source
        $vitePath = Join-Path $workspacePath 'web/node_modules/vite/bin/vite.js'
        if (-not (Test-Path -LiteralPath $vitePath)) { throw 'Vite is missing. Run ./scripts/build.ps1 -InstallDependencies -Start.' }
        $processOptions = @{
            FilePath = $nodePath
            ArgumentList = @('"' + $vitePath + '"', '--host', '127.0.0.1', '--port', '5173', '--strictPort')
            WorkingDirectory = Join-Path $workspacePath 'web'
            RedirectStandardOutput = Join-Path $logDirectory 'vite.log'
            RedirectStandardError = Join-Path $logDirectory 'vite-error.log'
            PassThru = $true
        }
        if ($IsWindows) { $processOptions.WindowStyle = 'Hidden' }
        $server = Start-Process @processOptions
        $deadline = [DateTime]::UtcNow.AddSeconds(30)
        while (-not (Test-DevServer)) {
            if ($server.HasExited) { throw "React server exited. See $logDirectory/vite-error.log." }
            if ([DateTime]::UtcNow -ge $deadline) { throw "React server did not become ready in 30 seconds. See $logDirectory/vite-error.log." }
            Start-Sleep -Milliseconds 250
        }
        Write-Host "React server ready: $devUrl (PID $($server.Id))" -ForegroundColor Green
    }
    else { Write-Host "Using running React server: $devUrl" -ForegroundColor Green }

    if ($NativeExecutable) {
        if (-not (Test-Path -LiteralPath $NativeExecutable)) { throw "Native executable is missing: $NativeExecutable" }
        $env:ENDLESS_CONTROL_WEB_URL = $devUrl
        $nativeProcess = Start-Process -FilePath $NativeExecutable -WorkingDirectory $workspacePath -PassThru
        Write-Host "Native app started (PID $($nativeProcess.Id)), using $devUrl with hot reload." -ForegroundColor Green
    }
    Write-Host 'The React server remains running after this script exits.'
}
catch {
    # Stop only a server created by this invocation if startup fails.
    if ($null -ne $server -and -not $server.HasExited) { Stop-Process -Id $server.Id -ErrorAction SilentlyContinue }
    throw
}
finally { $env:ENDLESS_CONTROL_WEB_URL = $previousUrl }
