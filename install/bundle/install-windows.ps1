# Elemental MOBA — install from this folder (pendrive) on Windows 10 / 11. Nothing else to install: the programs are ready.
# Double-click install-windows.cmd, or:  powershell -ExecutionPolicy Bypass -File install-windows.ps1 [folder]
# An older copy is updated in place: your settings, replays and own champions stay.
param([string]$Parent = $env:USERPROFILE)
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$game = Join-Path $Parent "elemental-moba"

Write-Host "== 1/3 game files -> $game"
New-Item -ItemType Directory -Force -Path $Parent | Out-Null
tar.exe -xzf (Join-Path $here "elemental-moba-source.tar.gz") -C $Parent
if ($LASTEXITCODE -ne 0) { throw "Could not unpack the game (tar.exe failed)." }

Write-Host "== 2/3 local champions (not on GitHub)"
$local = Join-Path $here "elemental-moba-local.tar.gz"
if (Test-Path $local) {
    tar.exe -xzf $local -C $game
    if ($LASTEXITCODE -ne 0) { throw "Could not unpack the local champions (tar.exe failed)." }
} else { Write-Host "   none here, skipped" }

Write-Host "== 3/3 programs and shortcut"
Copy-Item -Force (Join-Path $here "windows-programs\*") $game
$link = (New-Object -ComObject WScript.Shell).CreateShortcut((Join-Path ([Environment]::GetFolderPath("Desktop")) "Elemental MOBA.lnk"))
$link.TargetPath = Join-Path $game "elemental_moba.exe"
$link.WorkingDirectory = $game
$link.Save()

Write-Host ""
Write-Host "Done. Start: the 'Elemental MOBA' icon on the desktop, or $game\elemental_moba.exe"
Write-Host ("Public server: Multiplayer -> Direct connect -> " + (Get-Content (Join-Path $game "SERVER.txt")))
