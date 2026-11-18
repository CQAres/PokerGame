# Prepares a PowerShell session for building the project with MSYS2/MinGW-w64.
# Usage:
#   .\scripts\dev-shell.ps1
# or, to keep all changes in the current scope:
#   . .\scripts\dev-shell.ps1

$ErrorActionPreference = 'Stop'

$mingwRoot = 'C:\msys64\mingw64'
$msysRoot = 'C:\msys64\usr'
$gxx = Join-Path $mingwRoot 'bin\g++.exe'
$cmake = Join-Path $mingwRoot 'bin\cmake.exe'
$ninja = Join-Path $mingwRoot 'bin\ninja.exe'

foreach ($tool in @($gxx, $cmake, $ninja)) {
    if (-not (Test-Path $tool)) {
        Write-Error "Required tool not found: $tool. Install MSYS2 and the MinGW-w64 packages first."
    }
}

$env:PATH = "$mingwRoot\bin;$msysRoot\bin;$env:PATH"
$env:CMAKE_PREFIX_PATH = $mingwRoot

Write-Host 'MSYS2 MinGW-w64 environment ready.'
& $gxx --version | Select-Object -First 1
& $cmake --version | Select-Object -First 1
& $ninja --version
