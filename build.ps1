# Build script for "The Trial of Three Curses"
#
# IMPORTANT: this pins the MSYS2 UCRT64 compiler explicitly.
# The `g++` on PATH is MinGW.org 6.3.0 (32-bit) and CANNOT link the
# 64-bit GLFW/GLM in C:\msys64\ucrt64. Do not replace this with a bare `g++`.
#
#   .\build.ps1          build
#   .\build.ps1 -Run     build, then launch
#   .\build.ps1 -Clean    remove app.exe

param(
    [switch]$Run,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$Root     = $PSScriptRoot
$Msys     = "C:\msys64\ucrt64"
$Compiler = Join-Path $Msys "bin\g++.exe"
$Output   = Join-Path $Root "app.exe"

if ($Clean) {
    if (Test-Path $Output) { Remove-Item $Output -Force }
    Write-Host "Cleaned." -ForegroundColor Green
    exit 0
}

if (-not (Test-Path $Compiler)) {
    Write-Host "Compiler not found: $Compiler" -ForegroundColor Red
    Write-Host "Install with: pacman -S mingw-w64-ucrt-x86_64-gcc" -ForegroundColor Yellow
    exit 1
}

# The linker cannot overwrite app.exe while a previous run still has it open,
# which otherwise fails the build with "Permission denied" every single time
# you rebuild without closing the window first.
$running = Get-Process -Name "app" -ErrorAction SilentlyContinue |
           Where-Object { $_.Path -eq $Output }
if ($running) {
    Write-Host "Closing the running app.exe so the build can link..." -ForegroundColor Yellow
    $running | Stop-Process -Force
    Start-Sleep -Milliseconds 400
}

$sources = @()
$sources += (Get-ChildItem (Join-Path $Root "src") -Filter *.cpp | ForEach-Object { $_.FullName })
$sources += (Get-ChildItem (Join-Path $Root "src") -Filter *.c   | ForEach-Object { $_.FullName })

if ($sources.Count -eq 0) {
    Write-Host "No source files found in src\" -ForegroundColor Red
    exit 1
}

Write-Host "Compiling $($sources.Count) source file(s)..." -ForegroundColor Cyan

$flags = @(
    "-std=c++17"
    "-Wall"
    "-g"
    "-I$Root\include"
    "-I$Msys\include"
)

$links = @(
    "-L$Msys\lib"
    "-lglfw3"
    "-lopengl32"
    "-lgdi32"
    "-luser32"
    "-lshell32"
    "-static-libgcc"
    "-static-libstdc++"
)

& $Compiler @flags @sources @links -o $Output

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build FAILED." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "Build succeeded -> $Output" -ForegroundColor Green

if ($Run) {
    Write-Host "Launching..." -ForegroundColor Cyan
    Push-Location $Root
    try { & $Output } finally { Pop-Location }
}
