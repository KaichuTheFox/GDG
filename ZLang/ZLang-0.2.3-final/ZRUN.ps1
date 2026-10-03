param([Parameter(Mandatory=$true, Position=0)][string]$Source)
$ErrorActionPreference = 'Stop'
$zlang = Join-Path $PSScriptRoot 'build\windows-x64\bin\zlang.exe'
if (-not (Test-Path $zlang)) { throw 'zlang.exe not found. Run BUILD_WINDOWS.bat first.' }
& $zlang run (Resolve-Path $Source).Path
exit $LASTEXITCODE
