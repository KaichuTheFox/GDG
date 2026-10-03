param(
    [Parameter(Mandatory = $true)][string]$Zlang,
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Expected,
    [Parameter(Mandatory = $true)][string]$Work,
    [Parameter(Mandatory = $true)][string]$Cxx
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $Work | Out-Null
$program = Join-Path $Work 'program.exe'
& $Zlang build $Source -o $program --cxx $Cxx --quiet
if ($LASTEXITCODE -ne 0) { throw "Z compilation failed with exit code $LASTEXITCODE" }
$actual = & $program | Out-String
if ($LASTEXITCODE -ne 0) { throw "Generated program failed with exit code $LASTEXITCODE" }
$expectedText = Get-Content -Raw -Encoding UTF8 $Expected
$actual = $actual -replace "`r`n", "`n"
$expectedText = $expectedText -replace "`r`n", "`n"
if ($actual -ne $expectedText) {
    throw "Output mismatch.`nExpected:`n$expectedText`nActual:`n$actual"
}
