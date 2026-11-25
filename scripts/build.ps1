# Build Diskus boot + C kernel image
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Out = Join-Path $Root 'build'
New-Item -ItemType Directory -Force -Path $Out | Out-Null

function Find-Tool([string[]]$names, [string[]]$extraDirs) {
    foreach ($n in $names) {
        $cmd = Get-Command $n -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    foreach ($dir in $extraDirs) {
        if (-not (Test-Path $dir)) { continue }
        foreach ($n in $names) {
            $p = Join-Path $dir $n
            if (Test-Path $p) { return $p }
            $p2 = Join-Path $dir "$n.exe"
            if (Test-Path $p2) { return $p2 }
        }
        # zig often lives in versioned folder
        $found = Get-ChildItem $dir -Filter ($names[0] + '.exe') -Recurse -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName
        if ($found) { return $found }
    }
    return $null
}

$nasm = Find-Tool @('nasm') @(
    "$env:LOCALAPPDATA\bin\NASM",
    "$env:ProgramFiles\NASM"
)
if (-not $nasm) { Write-Error 'nasm not found' }

$zig = Find-Tool @('zig') @(
    (Join-Path $Root 'tools\zig'),
    "$env:LOCALAPPDATA\Microsoft\WinGet\Packages",
    "$env:ProgramFiles\Zig",
    "$env:LOCALAPPDATA\Programs\Zig",
    "C:\Zig"
)
if (-not $zig) { Write-Error 'zig not found. Put portable zig in tools\zig or: winget install zig.zig' }

Write-Host "nasm: $nasm"
Write-Host "zig:  $zig"

$bootBin = Join-Path $Out 'boot.bin'
& $nasm -f bin (Join-Path $Root 'boot\boot.asm') -o $bootBin
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ((Get-Item $bootBin).Length -ne 512) {
    Write-Error "boot.bin must be 512 bytes"
}

$entryO = Join-Path $Out 'entry.o'
& $nasm -f elf32 (Join-Path $Root 'kernel\entry.asm') -o $entryO
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$idtO = Join-Path $Out 'idt.o'
& $nasm -f elf32 (Join-Path $Root 'kernel\idt.asm') -o $idtO
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$cflags = @('-target', 'x86-freestanding-none', '-ffreestanding', '-fno-stack-protector', '-fno-pic', '-O2', '-I', (Join-Path $Root 'kernel'))

$mainO = Join-Path $Out 'main.o'
& $zig cc @cflags -c (Join-Path $Root 'kernel\main.c') -o $mainO
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$ramfsO = Join-Path $Out 'ramfs.o'
& $zig cc @cflags -c (Join-Path $Root 'kernel\ramfs.c') -o $ramfsO
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$kernelElf = Join-Path $Out 'kernel.elf'
# Prefer zig's bundled lld
$lld = Join-Path (Split-Path $zig) 'lld.exe'
if (-not (Test-Path $lld)) {
    $lld = Get-ChildItem (Split-Path $zig) -Filter 'lld.exe' -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $lld) {
    & $zig ld.lld -m elf_i386 -T (Join-Path $Root 'kernel\linker.ld') -o $kernelElf --oformat=elf $entryO $idtO $mainO $ramfsO
} else {
    & $lld -m elf_i386 -T (Join-Path $Root 'kernel\linker.ld') -o $kernelElf $entryO $idtO $mainO $ramfsO
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$kernelBin = Join-Path $Out 'kernel.bin'
& $zig objcopy -O binary $kernelElf $kernelBin
if ($LASTEXITCODE -ne 0) {
    # fallback: llvm-objcopy naming
    & $zig objcopy -I elf32-i386 -O binary $kernelElf $kernelBin
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$KERNEL_SECS = 64
$pad = $KERNEL_SECS * 512
$kb = [System.IO.File]::ReadAllBytes($kernelBin)
if ($kb.Length -gt $pad) {
    Write-Error "kernel.bin too large: $($kb.Length) > $pad"
}
$padded = New-Object byte[] $pad
[Array]::Copy($kb, $padded, $kb.Length)

$img = Join-Path $Out 'diskus.img'
$boot = [System.IO.File]::ReadAllBytes($bootBin)
$outBytes = New-Object byte[] ($boot.Length + $padded.Length)
[Array]::Copy($boot, 0, $outBytes, 0, $boot.Length)
[Array]::Copy($padded, 0, $outBytes, $boot.Length, $padded.Length)
[System.IO.File]::WriteAllBytes($img, $outBytes)

Write-Host "OK $img ($($outBytes.Length) bytes; kernel $($kb.Length) raw)"
