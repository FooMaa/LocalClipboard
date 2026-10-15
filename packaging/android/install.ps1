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
if ($LASTEXITCODE -ne 0 -and $Installed) { throw 'Could not query the selected device.' }
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
$PreviousErrorAction = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$InstallOutput = & adb @AdbArgs install -r LocalClipboard.apk 2>&1
$InstallExitCode = $LASTEXITCODE
$ErrorActionPreference = $PreviousErrorAction
$InstallOutput | ForEach-Object { Write-Host $_ }
if ($InstallExitCode -ne 0) {
    if ("$InstallOutput" -match 'INSTALL_FAILED_UPDATE_INCOMPATIBLE|INSTALL_PARSE_FAILED_INCONSISTENT_CERTIFICATES') {
        if ($Language -eq 'ru') { throw 'Подпись APK отличается. Регистрация сохранена. Нужен APK с прежним ключом. Если ключ потерян, сохраните код подключения, затем выберите удаление в установщике и установите заново. Удаление стирает данные приложения.' }
        throw 'APK signing key differs. Registration preserved. Use the original signing key. If it is lost, save your connection code, select removal in this installer and install again. Removal erases app data.'
    }
    if ("$InstallOutput" -match 'INSTALL_FAILED_VERSION_DOWNGRADE') {
        if ($Language -eq 'ru') { throw 'Установлена более новая версия. Используйте новый APK; регистрация сохранена.' }
        throw 'A newer version is installed. Use a newer APK; registration preserved.'
    }
    throw 'APK installation failed. See the Android error above.'
}
& adb @AdbArgs shell am start -n org.localclipboard/.ClipboardActivity --es localclipboard_language $Language
if ($LASTEXITCODE -ne 0) { throw 'Could not open registration.' }
if ($Language -eq 'ru') { Write-Host 'Завершите регистрацию устройства на телефоне.' } else { Write-Host 'Complete device registration on the phone.' }
