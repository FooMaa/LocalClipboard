param([string]$Serial)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (-not (Get-Command adb -ErrorAction SilentlyContinue)) { throw 'Install Android platform-tools (adb), enable USB debugging and connect your phone.' }
$AdbArgs = @()
if ($Serial) { $AdbArgs = @('-s', $Serial) }
& adb @AdbArgs get-state
if ($LASTEXITCODE -ne 0) { throw 'Device unavailable or unauthorized.' }
& adb @AdbArgs install -r LocalClipboard.apk
if ($LASTEXITCODE -ne 0) { throw 'APK installation failed.' }
& adb @AdbArgs shell am start -n org.localclipboard/.ClipboardActivity
if ($LASTEXITCODE -ne 0) { throw 'Could not open registration.' }
Write-Host 'Complete device registration on the phone.'
