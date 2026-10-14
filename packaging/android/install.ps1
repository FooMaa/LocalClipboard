param([string]$Serial, [ValidateSet("ru", "en")][string]$Language, [ValidateSet("install", "reinstall", "uninstall", "cancel")][string]$Action)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (-not $Language) {
    $Choice = Read-Host 'Language / Язык: 1 — Русский, 2 — English [2]'
    $Language = if ($Choice -eq '1' -or $Choice -eq 'ru') { 'ru' } else { 'en' }
}
if (-not (Get-Command adb -ErrorAction SilentlyContinue)) { throw 'Install Android platform-tools (adb), enable USB debugging and connect your phone.' }
$AdbArgs = @()
if ($Serial) { $AdbArgs = @('-s', $Serial) }
& adb @AdbArgs get-state
if ($LASTEXITCODE -ne 0) { throw 'Device unavailable or unauthorized.' }
$Installed = & adb @AdbArgs shell pm path org.localclipboard
if ($LASTEXITCODE -ne 0) { throw 'Could not query the selected device.' }
if ($Installed -match 'package:' -and -not $Action) {
    $Prompt = if ($Language -eq 'ru') { 'Уже установлено: 1 — переустановить, 2 — удалить, 3 — отмена [1]' } else { 'Already installed: 1 — reinstall, 2 — uninstall, 3 — cancel [1]' }
    $Choice = Read-Host $Prompt
    $Action = switch ($Choice) { '2' { 'uninstall' }; '3' { 'cancel' }; default { 'reinstall' } }
}
if ($Action -eq 'cancel') { exit 0 }
if ($Action -eq 'uninstall') {
    $Prompt = if ($Language -eq 'ru') { 'Приложение и его данные будут удалены. Введите DELETE' } else { 'The app and its data will be erased. Type DELETE' }
    if ((Read-Host $Prompt) -ne 'DELETE') { exit 0 }
    & adb @AdbArgs uninstall org.localclipboard
    if ($LASTEXITCODE -ne 0) { throw 'Removal failed.' }
    exit 0
}
& adb @AdbArgs install -r LocalClipboard.apk
if ($LASTEXITCODE -ne 0) { throw 'APK installation failed.' }
& adb @AdbArgs shell am start -n org.localclipboard/.ClipboardActivity --es localclipboard_language $Language
if ($LASTEXITCODE -ne 0) { throw 'Could not open registration.' }
if ($Language -eq 'ru') { Write-Host 'Завершите регистрацию устройства на телефоне.' } else { Write-Host 'Complete device registration on the phone.' }
