<#
.SYNOPSIS
    Build MTEngineSDLDummyApp for Windows.
.DESCRIPTION
    Builds MTEngineSDL then MTEngineSDLDummyApp via MSBuild.
    Output: platform/Windows/bin/<Platform>/<Configuration>/
.PARAMETER Platform
    Target architecture: x64 or ARM64. Default: auto-detect from OS.
.PARAMETER Configuration
    Build configuration: Debug or Release. Default: Release.
.PARAMETER Compiler
    Compiler toolchain: Clang (ClangCL) or MSVC (v143). Default: Clang.
    Clang requires "C++ Clang Compiler for Windows" VS component.
.PARAMETER Clean
    Clean all build artifacts and exit.
.EXAMPLE
    .\build-windows.ps1
    .\build-windows.ps1 -Compiler MSVC
    .\build-windows.ps1 -Clean
    .\build-windows.ps1 -Platform ARM64 -Configuration Debug
#>
param(
    [ValidateSet('x64','ARM64')]
    [string]$Platform,

    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Release',

    [ValidateSet('Clang','MSVC')]
    [string]$Compiler = 'Clang',

    [switch]$Clean,
    [switch]$Help
)

if ($Help) {
    Write-Host @"
MTEngineSDLDummyApp Windows Build Script

Usage: .\build-windows.ps1 [options]

  -Platform <x64|ARM64>            Target architecture (default: auto-detect)
  -Configuration <Debug|Release>   Build configuration (default: Release)
  -Compiler <Clang|MSVC>           Compiler toolchain (default: Clang)
                                    Clang requires "C++ Clang Compiler for Windows" VS component
  -Clean                           Clean all build artifacts and exit
  -Help                            Show this help message

Examples:
  .\build-windows.ps1
  .\build-windows.ps1 -Compiler MSVC
  .\build-windows.ps1 -Clean
  .\build-windows.ps1 -Platform ARM64 -Configuration Debug
"@
    exit 0
}

$ErrorActionPreference = 'Stop'

if (-not $Platform) {
    $Platform = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') { 'ARM64' } else { 'x64' }
    Write-Host "Auto-detected platform: $Platform"
}

$appDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$mtDir  = Join-Path (Split-Path -Parent $appDir) 'MTEngineSDL'

foreach ($tool in @('cmake', 'git')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Write-Error "$tool not found in PATH"; exit 1
    }
}

$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
    -latest -requires Microsoft.Component.MSBuild `
    -find "MSBuild\**\Bin\MSBuild.exe" 2>$null | Select-Object -First 1
if (-not $msbuild) {
    Write-Error "MSBuild not found. Install Visual Studio 2022 with C++ workload."
    exit 1
}
Write-Host "Using MSBuild: $msbuild"

$toolsetArgs = if ($Compiler -eq 'MSVC') { @('/p:PlatformToolset=v143') } else { @() }
Write-Host "Compiler: $Compiler" -ForegroundColor Cyan

# Add MSBuild to PATH so child processes can find it
$env:PATH = (Split-Path $msbuild) + ";$env:PATH"

# Init MTEngineSDL submodules
Write-Host "`n=== Initializing MTEngineSDL submodules ===" -ForegroundColor Cyan
Push-Location $mtDir
git submodule update --init --recursive
Pop-Location

if ($Clean) {
    Write-Host "`n=== Cleaning ===" -ForegroundColor Yellow
    & $msbuild "$mtDir\platform\Windows\MTEngineSDL.sln" `
        /t:Clean /p:Configuration=$Configuration /p:Platform=$Platform `
        @toolsetArgs /v:minimal /nologo 2>$null
    & $msbuild "$appDir\platform\Windows\MTEngineSDLDummyApp.sln" `
        /t:Clean /p:Configuration=$Configuration /p:Platform=$Platform `
        @toolsetArgs /v:minimal /nologo 2>$null
    Write-Host "Clean complete." -ForegroundColor Green
    exit 0
}

Write-Host "`n=== Building MTEngineSDL ($Platform $Configuration $Compiler) ===" -ForegroundColor Cyan
& $msbuild "$mtDir\platform\Windows\MTEngineSDL.sln" `
    /t:MTEngineSDL `
    /p:Configuration=$Configuration /p:Platform=$Platform `
    @toolsetArgs /m /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { Write-Error "MTEngineSDL build failed"; exit 1 }

Write-Host "`n=== Building MTEngineSDLDummyApp ($Platform $Configuration $Compiler) ===" -ForegroundColor Cyan
& $msbuild "$appDir\platform\Windows\MTEngineSDLDummyApp.sln" `
    /p:Configuration=$Configuration /p:Platform=$Platform `
    @toolsetArgs /m /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { Write-Error "MTEngineSDLDummyApp build failed"; exit 1 }

$outDir = "$appDir\platform\Windows\bin\$Platform\$Configuration"
$exe    = Join-Path $outDir "MTEngineSDLDummyApp.exe"

if (Test-Path $exe) {
    Write-Host "`n=== Build successful ===" -ForegroundColor Green
    Write-Host "Output: $outDir"
} else {
    Write-Error "Build completed but executable not found at $outDir"
    exit 1
}
