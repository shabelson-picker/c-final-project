# solo-ran build helper: locate VS, import vcvars64, msbuild x64 Debug.
$ErrorActionPreference = "Stop"
$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
$proj = "C:\Users\shabe\Documents\cpp oop\c-course\fin-project\fin-project\fin-project.vcxproj"
# Run vcvars then msbuild in one cmd shell; surface warnings/errors.
cmd /c "`"$vcvars`" >nul 2>&1 && msbuild `"$proj`" /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal /clp:Summary"
exit $LASTEXITCODE
