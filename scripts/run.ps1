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
    # Headless: expect "Diskus" on COM1 within a few seconds
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $qemu
    $psi.Arguments = "-drive format=raw,file=`"$Img`" -boot order=c -display none -serial stdio -no-reboot"
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $p = [System.Diagnostics.Process]::Start($psi)
    $deadline = [DateTime]::UtcNow.AddSeconds(6)
    $buf = ''
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 200
        while (-not $p.StandardOutput.EndOfStream) {
            $buf += [char]$p.StandardOutput.Read()
            if ($buf -match 'Diskus PM') { break }
        }
        if ($buf -match 'Diskus PM') { break }
        if ($p.HasExited) { break }
    }
    if (-not $p.HasExited) {
        try { $p.Kill() } catch {}
    }
    Write-Host $buf
    if ($buf -match 'Diskus PM') {
        Write-Host 'SMOKE OK'
        exit 0
    }
    Write-Error "SMOKE FAIL — no 'Diskus PM' on serial. Got: $buf"
}

& $qemu -drive "format=raw,file=$Img" -boot order=c
