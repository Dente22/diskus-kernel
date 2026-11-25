# Run Diskus in QEMU (VGA window). Use -Smoke for serial-only check.
param(
    [switch]$Smoke
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Img = Join-Path $Root 'build\diskus.img'

if (-not (Test-Path $Img)) {
    & (Join-Path $PSScriptRoot 'build.ps1')
}

function Find-Qemu {
    $names = @('qemu-system-i386', 'qemu-system-x86_64', 'qemu-system-i386w', 'qemu-system-x86_64w')
    foreach ($n in $names) {
        $cmd = Get-Command $n -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    $dir = 'C:\Program Files\qemu'
    foreach ($n in $names) {
        $p = Join-Path $dir "$n.exe"
        if (Test-Path $p) { return $p }
    }
    return $null
}

$qemu = Find-Qemu
if (-not $qemu) {
    Write-Error "qemu not found. Install: winget install SoftwareFreedomConservancy.QEMU"
}

Write-Host "QEMU: $qemu"
Write-Host "Image: $Img"

if ($Smoke) {
    # Headless: serial to file (Windows pipe DataAvailable is flaky with QEMU)
    $OutDir = Join-Path $Root 'build'
    $log = Join-Path $OutDir 'serial.log'
    if (Test-Path $log) { Remove-Item $log -Force }
    $args = "-drive format=raw,file=`"$Img`" -boot order=c -display none -serial file:$log -no-reboot"
    $p = Start-Process -FilePath $qemu -ArgumentList $args -PassThru -WindowStyle Hidden
    Start-Sleep -Seconds 2
    if (-not $p.HasExited) {
        try { Stop-Process -Id $p.Id -Force } catch {}
    }
    Start-Sleep -Milliseconds 400
    $text = ''
    if (Test-Path $log) {
        $text = [IO.File]::ReadAllText($log)
    }
    Write-Host $text
        if ($text -match 'P6' -and $text -match 'ticks' -and $text -match '>') {
        Write-Host 'SMOKE OK'
        exit 0
    }
    Write-Error "SMOKE FAIL - no P6 on serial. Got: $text"
}

& $qemu -drive "format=raw,file=$Img" -boot order=c
