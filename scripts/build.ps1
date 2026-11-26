# Build Diskus boot + C kernel image
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Out = Join-Path $Root 'build'
$Kernel = Join-Path $Root 'kernel'
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

function Build-UserBin([string]$AsmName, [string]$BinName) {
    $binPath = Join-Path $Out $BinName
    & $nasm -f bin (Join-Path $Kernel $AsmName) -o $binPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    return [System.IO.File]::ReadAllBytes($binPath)
}

# User programs live on the disk image (DKFS), not embedded in the kernel
$userBytes = Build-UserBin 'user.asm' 'user.bin'
$user2Bytes = Build-UserBin 'user2.asm' 'user2.bin'
$user3Bytes = Build-UserBin 'user_cat.asm' 'user_cat.bin'
$userLen = $userBytes.Length
$user2Len = $user2Bytes.Length
$user3Len = $user3Bytes.Length

$entryO = Join-Path $Out 'entry.o'
& $nasm -f elf32 (Join-Path $Kernel 'entry.asm') -o $entryO
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$idtO = Join-Path $Out 'idt.o'
& $nasm -f elf32 (Join-Path $Kernel 'idt.asm') -o $idtO
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$userlandO = Join-Path $Out 'userland.o'
& $nasm -f elf32 (Join-Path $Kernel 'userland.asm') -o $userlandO
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$cflags = @('-target', 'x86-freestanding-none', '-ffreestanding', '-fno-stack-protector', '-fno-pic', '-O2', '-I', $Kernel)

$mainO = Join-Path $Out 'main.o'
& $zig cc @cflags -c (Join-Path $Kernel 'main.c') -o $mainO
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$ramfsO = Join-Path $Out 'ramfs.o'
& $zig cc @cflags -c (Join-Path $Kernel 'ramfs.c') -o $ramfsO
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$diskO = Join-Path $Out 'disk.o'
& $zig cc @cflags -c (Join-Path $Kernel 'disk.c') -o $diskO
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$pagingO = Join-Path $Out 'paging.o'
& $zig cc @cflags -c (Join-Path $Kernel 'paging.c') -o $pagingO
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$gfxO = Join-Path $Out 'gfx.o'
& $zig cc @cflags -c (Join-Path $Kernel 'gfx.c') -o $gfxO
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$uiO = Join-Path $Out 'ui.o'
& $zig cc @cflags -c (Join-Path $Kernel 'ui.c') -o $uiO
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$mouseO = Join-Path $Out 'mouse.o'
& $zig cc @cflags -c (Join-Path $Kernel 'mouse.c') -o $mouseO
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$kernelElf = Join-Path $Out 'kernel.elf'
$objs = @($entryO, $idtO, $userlandO, $mainO, $ramfsO, $diskO, $pagingO, $gfxO, $uiO, $mouseO)
$lld = Join-Path (Split-Path $zig) 'lld.exe'
if (-not (Test-Path $lld)) {
    $lld = Get-ChildItem (Split-Path $zig) -Filter 'lld.exe' -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $lld) {
    & $zig ld.lld -m elf_i386 -T (Join-Path $Kernel 'linker.ld') -o $kernelElf --oformat=elf @objs
} else {
    & $lld -m elf_i386 -T (Join-Path $Kernel 'linker.ld') -o $kernelElf @objs
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$kernelBin = Join-Path $Out 'kernel.bin'
& $zig objcopy -O binary $kernelElf $kernelBin
if ($LASTEXITCODE -ne 0) {
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

# DKFS tail at LBA 65 (after boot + 64 kernel sectors): magic, count, files
function Append-DkfsFile([System.Collections.Generic.List[byte]]$dst, [string]$name, [byte[]]$data) {
    $nameBytes = New-Object byte[] 16
    $nb = [Text.Encoding]::ASCII.GetBytes($name)
    [Array]::Copy($nb, $nameBytes, [Math]::Min($nb.Length, 15))
    $dst.AddRange($nameBytes)
    $dst.Add([byte]($data.Length -band 0xFF))
    $dst.Add([byte](($data.Length -shr 8) -band 0xFF))
    $dst.Add([byte](($data.Length -shr 16) -band 0xFF))
    $dst.Add([byte](($data.Length -shr 24) -band 0xFF))
    $dst.AddRange($data)
}

$dkfs = New-Object 'System.Collections.Generic.List[byte]'
$dkfs.AddRange([byte[]][char[]]@('D','K','F','S'))
$dkfs.Add(3)   # file count
$dkfs.Add(0); $dkfs.Add(0); $dkfs.Add(0)
Append-DkfsFile $dkfs 'hello' $userBytes
Append-DkfsFile $dkfs 'bye' $user2Bytes
Append-DkfsFile $dkfs 'cat' $user3Bytes
# pad to 2 sectors (kernel reads DKFS_SECS=2)
while ($dkfs.Count % 512 -ne 0) { $dkfs.Add(0) }
while ($dkfs.Count -lt 1024) { $dkfs.Add(0) }

$img = Join-Path $Out 'diskus.img'
$boot = [System.IO.File]::ReadAllBytes($bootBin)
$total = $boot.Length + $padded.Length + $dkfs.Count
$outBytes = New-Object byte[] $total
[Array]::Copy($boot, 0, $outBytes, 0, $boot.Length)
[Array]::Copy($padded, 0, $outBytes, $boot.Length, $padded.Length)
[Array]::Copy($dkfs.ToArray(), 0, $outBytes, $boot.Length + $padded.Length, $dkfs.Count)
[System.IO.File]::WriteAllBytes($img, $outBytes)

Write-Host "OK $img ($($outBytes.Length) bytes; kernel $($kb.Length) raw; DKFS user $userLen+$user2Len+$user3Len)"
