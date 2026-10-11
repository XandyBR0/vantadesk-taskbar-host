param(
  [ValidateSet('Release', 'Debug')]
  [string]$Configuration = 'Release'
)

# Compila o VantaDeskTaskbarHost.exe em .\bin (mesmas opções usadas no VantaDesk).
$ErrorActionPreference = 'Stop'
$outputDirectory = Join-Path $PSScriptRoot 'bin'
$sourceFile = Join-Path $PSScriptRoot 'VantaDeskTaskbarHost.cpp'
$vsDevCmd = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat'

if (-not (Test-Path -LiteralPath $vsDevCmd)) {
  throw 'Visual Studio Build Tools 2022 com C++ não foi encontrado.'
}

New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

$compilerFlags = if ($Configuration -eq 'Debug') { '/Od /Zi' } else { '/O2 /GL' }
$command = 'call "' + $vsDevCmd + '" -arch=x64 -host_arch=x64 && ' +
  'cl.exe /nologo /std:c++20 /EHsc /utf-8 /DUNICODE /D_UNICODE ' + $compilerFlags + ' ' +
  '"' + $sourceFile + '" /link /SUBSYSTEM:WINDOWS /OUT:"' + (Join-Path $outputDirectory 'VantaDeskTaskbarHost.exe') + '" Ole32.lib Shell32.lib User32.lib'

& $env:ComSpec /d /s /c $command
if ($LASTEXITCODE -ne 0) {
  throw "Falha ao compilar VantaDeskTaskbarHost.exe (código $LASTEXITCODE)."
}

Write-Output (Join-Path $outputDirectory 'VantaDeskTaskbarHost.exe')
