param(
    [Parameter(Mandatory=$true, Position=0)][string]$Source,
    [Parameter(Position=1)][string]$Output
)
$ErrorActionPreference = 'Stop'
$zlang = Join-Path $PSScriptRoot 'build\windows-x64\bin\zlang.exe'
if (-not (Test-Path $zlang)) { throw 'zlang.exe not found. Run BUILD_WINDOWS.bat first.' }
$sourcePath = (Resolve-Path $Source).Path
if (-not $Output) { $Output = [IO.Path]::ChangeExtension($sourcePath, '.exe') }
& $zlang $sourcePath -o $Output
exit $LASTEXITCODE
