#!/usr/bin/env pwsh
# Build script for phoenix-sdr-utils (Windows/PowerShell)

param(
    [switch]$Clean,
    [switch]$Rebuild,
    [string]$BuildType = "Release"
)

$ErrorActionPreference = "Stop"

$BUILD_DIR = "build"

# Clean if requested
if ($Clean -or $Rebuild) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path $BUILD_DIR) {
        Remove-Item -Recurse -Force $BUILD_DIR
    }
}

# Create build directory
if (-not (Test-Path $BUILD_DIR)) {
    New-Item -ItemType Directory -Path $BUILD_DIR | Out-Null
}

# Configure with CMake
Write-Host "Configuring with CMake..." -ForegroundColor Cyan
Push-Location $BUILD_DIR
try {
    cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=$BuildType ..
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }

    # Build
    Write-Host "Building..." -ForegroundColor Cyan
    cmake --build . --config $BuildType
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }

    Write-Host "`nBuild complete!" -ForegroundColor Green
    Write-Host "Executables in: $BUILD_DIR" -ForegroundColor Green
    
    # List built executables
    Write-Host "`nBuilt executables:" -ForegroundColor Cyan
    Get-ChildItem -Path . -Filter *.exe | ForEach-Object {
        Write-Host "  - $($_.Name)" -ForegroundColor White
    }
}
finally {
    Pop-Location
}
