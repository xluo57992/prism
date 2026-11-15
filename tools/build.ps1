# Prism 构建脚本（Windows / MSVC / Ninja）
#
# 用法：
#   powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
#
# 依赖：
#   - Visual Studio 2022 Build Tools（含 C++ 桌面开发）
#   - 或任意带 C++ 工具链的 VS 安装（脚本用 vswhere 自动定位）
#   - CMake 与 Ninja 随 VS 一起装（CommonExtensions\Microsoft\CMake）

param(
    [ValidateSet('Release', 'Debug')]
    [string]$Config = 'Release',
    [switch]$Clean,
    [string]$JuceDir = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot      # 仓库根目录

# --- 1. 定位 Visual Studio ---------------------------------------------------
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found. Install Visual Studio Build Tools with the C++ workload."
}

$vsPath = & $vswhere -products '*' -property installationPath |
          Where-Object { $_ -and (Test-Path (Join-Path $_ 'VC\Auxiliary\Build\vcvars64.bat')) } |
          Select-Object -First 1
if (-not $vsPath) { throw "No Visual Studio installation with the C++ toolchain found." }

$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
$cmake  = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja  = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
if (-not (Test-Path $cmake)) { throw "CMake not found at $cmake" }

Write-Host "VS    : $vsPath"
Write-Host "CMake : $cmake"

# --- 2. 生成 bat（让 vcvars 环境能被 CMake 继承）----------------------------
$buildDir = Join-Path $root 'build'
if ($Clean -and (Test-Path $buildDir)) { Remove-Item $buildDir -Recurse -Force }

$juceArg = if ($JuceDir) { "-DJUCE_DIR=`"$JuceDir`"" } else { '' }

$bat = Join-Path $env:TEMP 'prism_build.bat'
@"
@echo off
call "$vcvars" >nul 2>&1
if errorlevel 1 exit /b 1
"$cmake" -S "$root" -B "$buildDir" -G Ninja -DCMAKE_BUILD_TYPE=$Config -DCMAKE_MAKE_PROGRAM="$ninja" $juceArg
if errorlevel 1 exit /b 1
"$cmake" --build "$buildDir" --config $Config
exit /b %errorlevel%
"@ | Set-Content -Encoding ASCII $bat

# --- 3. 编译 -----------------------------------------------------------------
& cmd /c "`"$bat`""
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }

# --- 4. 产物 ----------------------------------------------------------------
Write-Host ""
Write-Host "=== artefacts ===" -ForegroundColor Green
Get-ChildItem $buildDir -Recurse -Filter '*.vst3' -ErrorAction SilentlyContinue |
    ForEach-Object { Write-Host ("VST3 : " + $_.FullName) }
Get-ChildItem $buildDir -Recurse -Filter 'Prism.exe' -ErrorAction SilentlyContinue |
    ForEach-Object { Write-Host ("Standalone : " + $_.FullName) }
Get-ChildItem $buildDir -Recurse -Filter 'PrismSelfTest.exe' -ErrorAction SilentlyContinue |
    ForEach-Object { Write-Host ("SelfTest : " + $_.FullName) }
