# Start the public server on Windows, in its own window: powershell -ExecutionPolicy Bypass -File install\run-server.ps1 [port]
param([int]$Port = 7777)
Set-Location (Join-Path $PSScriptRoot "..")
Start-Process -FilePath "build\em_server.exe" -ArgumentList "--nogui","--port",$Port,"--unranked"
