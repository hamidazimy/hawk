#!/usr/bin/env pwsh
$ErrorActionPreference = "Stop"

Write-Host "Hawk Log Analyzer - Build Script (MSVC)"
Write-Host "========================================"
Write-Host ""

if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
    Write-Error "MSVC compiler (cl.exe) not found on PATH. Run this from a 'Developer PowerShell for VS 2022' prompt, or run vcvarsall.bat / Enter-VsDevShell first."
    exit 1
}

Write-Host "Configuring with CMake..."
cmake -S . -B build-msvc -DCMAKE_BUILD_TYPE=Release -DHAWK_STATIC_BUILD=ON -DHAWK_BUILD_TESTS=OFF
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Building..."
cmake --build build-msvc --config Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Build complete!"
Write-Host ""
Write-Host "Binary location: build-msvc\bin\Release\hawk.exe"
Write-Host "(Note: MSVC's generator is multi-config, so the binary lands under a Release\ subdirectory, unlike the Linux/macOS build.)"
Write-Host "To run: .\build-msvc\bin\Release\hawk.exe sample_logs\auth.csv"
Write-Host ""
