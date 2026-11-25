# Build Diskus boot image
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Out = Join-Path $Root 'build'
New-Item -ItemType Directory -Force -Path $Out | Out-Null

function Find-Nasm {
    $cmd = Get-Command nasm -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $candidates = @(
        "$env:LOCALAPPDATA\bin\NASM\nasm.exe",
        "$env:ProgramFiles\NASM\nasm.exe",
        "${env:ProgramFiles(x86)}\NASM\nasm.exe"
    )
    foreach ($p in $candidates) {
        if (Test-Path $p) { return $p }
    }
    return $null
}

$nasm = Find-Nasm
if (-not $nasm) {
    Write-Error "nasm not found. Install: winget install NASM.NASM"
}

$bin = Join-Path $Out 'boot.bin'
& $nasm -f bin (Join-Path $Root 'boot\boot.asm') -o $bin
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$size = (Get-Item $bin).Length
if ($size -ne 512) {
    Write-Error "boot.bin must be 512 bytes, got $size"
}

Copy-Item $bin (Join-Path $Out 'diskus.img') -Force
Write-Host "OK build\diskus.img ($size bytes) via $nasm"
