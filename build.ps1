# Configures and builds the project with the Visual Studio toolchain
$ErrorActionPreference = 'Stop'

$vcpkgRoot = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg'
$toolchain = Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
$cmake = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

& $cmake -S . -B build -G "Visual Studio 18 2026" -A x64 `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
    "-DVCPKG_TARGET_TRIPLET=x64-windows" `
    "-DVCPKG_INSTALLED_DIR=$root\vcpkg_installed"
if ($LASTEXITCODE -ne 0) { throw "configure failed" }

& $cmake --build build --config Release
if ($LASTEXITCODE -ne 0) { throw "build failed" }

Write-Host ""
Write-Host "Binaries are in build\Release"
