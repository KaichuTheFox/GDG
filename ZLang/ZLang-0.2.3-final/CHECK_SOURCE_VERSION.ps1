$ErrorActionPreference = 'Stop'
$main = Join-Path $PSScriptRoot 'src\main.cpp'
$cmake = Join-Path $PSScriptRoot 'CMakeLists.txt'
Write-Host 'ZLang source check'
Write-Host ('main.cpp: ' + ((Select-String -Path $main -Pattern 'Z Language Compiler' | Select-Object -First 1).Line))
Write-Host ('CMakeLists: ' + ((Select-String -Path $cmake -Pattern '^project\(' | Select-Object -First 1).Line))
