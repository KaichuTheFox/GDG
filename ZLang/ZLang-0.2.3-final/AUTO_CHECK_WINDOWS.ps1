param([ValidateSet('x64','x86')][string]$Architecture = 'x64')
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$platform = if ($Architecture -eq 'x64') { 'x64' } else { 'Win32' }
$buildDir = Join-Path $PSScriptRoot "build\windows-$Architecture"
$zlang = Join-Path $buildDir 'bin\zlang.exe'

function Step([string]$message) { Write-Host "`n== $message ==" -ForegroundColor Cyan }
function Require([bool]$condition, [string]$message) { if (-not $condition) { throw $message } }

Step '1/8 Configure'
cmake -S . -B $buildDir -A $platform -DBUILD_TESTING=ON
Require ($LASTEXITCODE -eq 0) 'CMake configure failed.'

Step '2/8 Build'
cmake --build $buildDir --config Release
Require ($LASTEXITCODE -eq 0) 'C++ build failed.'
Require (Test-Path $zlang) "zlang.exe missing: $zlang"

Step '3/8 CLI'
$version = & $zlang --version | Out-String
Require ($LASTEXITCODE -eq 0) '--version failed.'
Require ($version -match '0\.2\.0') 'Unexpected version output.'
& $zlang --help | Out-Null
Require ($LASTEXITCODE -eq 0) '--help failed.'

Step '4/8 CTest'
ctest --test-dir $buildDir -C Release --output-on-failure
Require ($LASTEXITCODE -eq 0) 'CTest failed.'

Step '5/8 Hello World native EXE'
$helloExe = Join-Path $buildDir 'auto-hello.exe'
Remove-Item $helloExe -Force -ErrorAction SilentlyContinue
& $zlang (Join-Path $PSScriptRoot 'examples\hello.z') -o $helloExe --quiet
Require ($LASTEXITCODE -eq 0) 'Hello compilation failed.'
Require (Test-Path $helloExe) 'Hello executable was not created.'
$helloOutput = & $helloExe | Out-String
Require ($LASTEXITCODE -eq 0) 'Hello executable failed.'
Require (($helloOutput.Trim()) -eq 'Hello World!') "Hello output mismatch: $helloOutput"

Step '6/8 Compatibility native EXE'
$compatExe = Join-Path $buildDir 'auto-compatibility.exe'
Remove-Item $compatExe -Force -ErrorAction SilentlyContinue
& $zlang (Join-Path $PSScriptRoot 'examples\compatibility.z') -o $compatExe --quiet
Require ($LASTEXITCODE -eq 0) 'Compatibility compilation failed.'
$actual = (& $compatExe | Out-String) -replace "`r`n", "`n"
$expected = (Get-Content -Raw -Encoding UTF8 (Join-Path $PSScriptRoot 'examples\compatibility.stdout')) -replace "`r`n", "`n"
Require ($actual -eq $expected) 'Compatibility output mismatch.'

Step '7/8 Error diagnostics'
$badExe = Join-Path $buildDir 'must-not-exist.exe'
Remove-Item $badExe -Force -ErrorAction SilentlyContinue
$diagnostic = & $zlang (Join-Path $PSScriptRoot 'tests\errors\undeclared.z') -o $badExe 2>&1 | Out-String
Require ($LASTEXITCODE -eq 1) 'Invalid Z source did not return exit code 1.'
Require (-not (Test-Path $badExe)) 'Compiler incorrectly created EXE for invalid source.'
Require ($diagnostic -match 'undeclared variable') 'Expected diagnostic was not emitted.'

Step '8/8 Greater-equal, UTF-8, while, cpx and semicolon smoke test'
$smokeSource = Join-Path $buildDir 'windows-smoke.z'
$smokeExe = Join-Path $buildDir 'windows-smoke.exe'
@'
main() {
    box n = 2;
    while (n > 0) {
        print "반복 ", n, "\n"
        n = n - 1
    }
    if (n >= 0) {
        cpx value = cpx(3, 4)
        print(value, "\n")
    }
}
'@ | Set-Content -Path $smokeSource -Encoding utf8
& $zlang $smokeSource -o $smokeExe --quiet
Require ($LASTEXITCODE -eq 0) 'Windows feature smoke compilation failed.'
$smokeOutput = & $smokeExe | Out-String
Require ($LASTEXITCODE -eq 0) 'Windows feature smoke executable failed.'
Require ($smokeOutput -match '반복 2') 'UTF-8/while output check failed.'
Require ($smokeOutput -match '3\+4i') 'cpx output check failed.'

Write-Host "`nALL WINDOWS CHECKS PASSED ($Architecture)" -ForegroundColor Green
