param([ValidateSet('x64','x86')][string]$Architecture = 'x64')
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$platform = if ($Architecture -eq 'x64') { 'x64' } else { 'Win32' }
$buildDir = Join-Path $PSScriptRoot "build\windows-$Architecture"
cmake -S . -B $buildDir -A $platform -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $buildDir --config Release
exit $LASTEXITCODE
