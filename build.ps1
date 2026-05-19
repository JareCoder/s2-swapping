# build.ps1
# C++ Build script for Source2 Swapping project

$ErrorActionPreference = "Stop"

Write-Host "Locating Visual Studio installation..." -ForegroundColor Cyan
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "Visual Studio Installer (vswhere.exe) not found. Please ensure Visual Studio is installed."
}

$vsPath = & $vswhere -latest -property installationPath
if (-not $vsPath) {
    Write-Error "No Visual Studio installation detected."
}
Write-Host "Found Visual Studio at: $vsPath" -ForegroundColor Green

$msbuild = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    Write-Error "MSBuild.exe not found at expected path: $msbuild"
}

$vsVersion = & $vswhere -latest -property installationVersion
$majorVersion = [int]($vsVersion.Split('.')[0])
$toolset = "v143"

if ($majorVersion -eq 17) {
    $toolset = "v143" # VS 2022
} elseif ($majorVersion -eq 18) {
    $toolset = "v145" # VS 2026
} else {
    Write-Host "Warning: Unknown VS version ($majorVersion). Defaulting platform toolset to v145." -ForegroundColor Yellow
    $toolset = "v145"
}
Write-Host "Using Platform Toolset: $toolset" -ForegroundColor Green

$minhookDir = Join-Path $PSScriptRoot "minhook"
if (-not (Test-Path $minhookDir)) {
    Write-Host "MinHook dependency not found. Cloning repository..." -ForegroundColor Cyan
    git clone https://github.com/TsudaKageyu/minhook.git minhook
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to clone MinHook repository."
    }
}

Write-Host "Checking for vcpkg..." -ForegroundColor Cyan
$vcpkgExe = $null

$commonPaths = @(
    "D:\VSCode\AA_Tools\vcpkg\vcpkg.exe",
    "${env:VCPKG_ROOT}\vcpkg.exe",
    "C:\vcpkg\vcpkg.exe"
)
foreach ($path in $commonPaths) {
    if (Test-Path $path) {
        $vcpkgExe = $path
        break
    }
}

if ($null -eq $vcpkgExe) {
    $vcpkgPath = Get-Command vcpkg -ErrorAction SilentlyContinue
    if ($vcpkgPath) {
        $vcpkgExe = $vcpkgPath.Source
    }
}

if ($vcpkgExe) {
    Write-Host "Found vcpkg at: $vcpkgExe" -ForegroundColor Green
    Write-Host "Ensuring fmt:x64-windows-static is installed..." -ForegroundColor Cyan
    & $vcpkgExe install fmt:x64-windows-static
} else {
    Write-Host "Warning: vcpkg not found. Make sure you have installed 'fmt:x64-windows-static' manually." -ForegroundColor Yellow
}

$minhookLib = Join-Path $minhookDir "build\VC16\lib\Release\libMinHook.x64.lib"
if (-not (Test-Path $minhookLib)) {
    Write-Host "Building MinHook library..." -ForegroundColor Cyan
    $minhookSln = Join-Path $minhookDir "build\VC16\MinHookVC16.sln"
    & $msbuild $minhookSln /p:Configuration=Release /p:Platform=x64 "/p:PlatformToolset=$toolset" /m
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to build MinHook library."
    }
} else {
    Write-Host "MinHook library already built." -ForegroundColor Green
}

Write-Host "Building cs2_swapping DLL..." -ForegroundColor Cyan
$solutionFile = Join-Path $PSScriptRoot "cs2_swapping.sln"
& $msbuild $solutionFile /p:Configuration=Release /p:Platform=x64 "/p:PlatformToolset=$toolset" /m
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to build cs2_swapping solution."
}

$outputDll = Join-Path $PSScriptRoot "x64\Release\cs2_swapping.dll"
if (Test-Path $outputDll) {
    Write-Host "`n==================================================" -ForegroundColor Green
    Write-Host "Success! The DLL has been built successfully." -ForegroundColor Green
    Write-Host "Output DLL path: $outputDll" -ForegroundColor Green
    Write-Host "==================================================" -ForegroundColor Green
} else {
    Write-Error "Build finished but output DLL was not found at $outputDll"
}
