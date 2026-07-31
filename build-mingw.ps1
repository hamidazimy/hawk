#!/usr/bin/env pwsh
$ErrorActionPreference = "Stop"

Write-Host "Hawk Log Analyzer - Build Script (native MinGW-w64)"
Write-Host "====================================================="
Write-Host ""

if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
    Write-Error "g++ not found on PATH. Install MinGW-w64 (e.g. via MSYS2) and ensure its bin directory is on PATH before running this script."
    exit 1
}

Write-Host "Configuring with CMake..."
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DHAWK_STATIC_BUILD=ON -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Building..."
cmake --build build-mingw -j
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Build complete!"
Write-Host ""
Write-Host "Binary location: build-mingw\bin\hawk.exe"
Write-Host "To run: .\build-mingw\bin\hawk.exe sample_logs\auth.csv"
Write-Host ""
