# Enter DiskusUbuntu WSL in the repo directory
$Root = Split-Path -Parent $PSScriptRoot
$wslPath = ($Root -replace '\\', '/') -replace '^([A-Za-z]):', { '/mnt/' + $args[0].Groups[1].Value.ToLower() }
# simpler: use -cd with Windows path (WSL accepts it on recent builds)
wsl -d DiskusUbuntu --cd $Root
