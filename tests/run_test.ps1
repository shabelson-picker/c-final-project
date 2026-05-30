# solo-ran test runner: compile a test harness against the app sources (minus
# main.c) and run it. Usage: run_test.ps1 <test_file.c>
# Output goes to tests\bin\; exit code is the test's exit code.
param([Parameter(Mandatory=$true)][string]$Test)
$ErrorActionPreference = "Stop"
$root   = Split-Path $PSScriptRoot -Parent              # fin-project\
$src    = Join-Path $root "fin-project\src"
$bin    = Join-Path $PSScriptRoot "bin"
New-Item -ItemType Directory -Force -Path $bin | Out-Null

$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"

# All app sources except main.c, plus the test file.
$srcs = Get-ChildItem -Path $src -Filter *.c | Where-Object { $_.Name -ne "main.c" } | ForEach-Object { "`"$($_.FullName)`"" }
$testPath = Join-Path $PSScriptRoot $Test
$exeName  = [System.IO.Path]::GetFileNameWithoutExtension($Test) + ".exe"
$exePath  = Join-Path $bin $exeName

$clArgs = "/nologo /W4 /D_CRT_SECURE_NO_WARNINGS /I `"$src`" `"$testPath`" " + ($srcs -join " ") + " /Fe`"$exePath`" /Fo`"$bin\\`""
cmd /c "`"$vcvars`" >nul 2>&1 && cl $clArgs"
if ($LASTEXITCODE -ne 0) { Write-Output "COMPILE FAILED ($LASTEXITCODE)"; exit $LASTEXITCODE }

Write-Output "--- running $exeName ---"
& $exePath
$rc = $LASTEXITCODE
Write-Output "--- exit code: $rc ---"
exit $rc
