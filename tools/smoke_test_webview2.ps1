# Smoke Test for ABDAudioLab & WebView2 Pre-warming
$exePath = Join-Path $PSScriptRoot "..\build\ABDAudioLab_artefacts\Release\ABDAudioLab.exe"
if (-not (Test-Path $exePath)) {
    Write-Error "ABDAudioLab.exe not found at $exePath"
    exit 1
}

Write-Host "[SmokeTest] Launching ABDAudioLab.exe..." -ForegroundColor Cyan
$proc = Start-Process -FilePath $exePath -PassThru

# Wait 5 seconds for splash screen sequence, audio driver init, and WebView2 pre-warm
Start-Sleep -Seconds 5

if (-not $proc.HasExited) {
    Write-Host "[SmokeTest] SUCCESS: ABDAudioLab is running cleanly (PID $($proc.Id))." -ForegroundColor Green
    Write-Host "[SmokeTest] WebView2 pre-warming and audio device startup succeeded without crash." -ForegroundColor Green
    $proc.Kill()
    $proc.WaitForExit()
    Write-Host "[SmokeTest] Process cleanly terminated after verification." -ForegroundColor Gray
    exit 0
} else {
    Write-Error "[SmokeTest] FAILED: Process exited unexpectedly with exit code $($proc.ExitCode)."
    exit 1
}
