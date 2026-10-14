Unicode true
!include "MUI2.nsh"
Name "LocalClipboard"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\LocalClipboard"
RequestExecutionLevel user
SetCompressor /SOLID lzma
!define MUI_WELCOMEPAGE_TEXT "Synchronize text, photos and files between your own devices. After installation, register this device with a personal profile. Each person should create a separate profile."
!define MUI_FINISHPAGE_RUN "$INSTDIR\LocalClipboard.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Register / open LocalClipboard"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Russian"
Section "LocalClipboard"
    SetShellVarContext current
    ; Stop the installed copy before replacing its Qt DLLs during an upgrade.
    nsExec::ExecToLog 'taskkill /IM LocalClipboard.exe /F'
    SetOutPath "$INSTDIR"
    File /r "${DEPLOY_DIR}\*"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    CreateDirectory "$SMPROGRAMS\LocalClipboard"
    CreateShortcut "$SMPROGRAMS\LocalClipboard\LocalClipboard.lnk" "$INSTDIR\LocalClipboard.exe"
    CreateShortcut "$SMPROGRAMS\LocalClipboard\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\LocalClipboard" "DisplayName" "LocalClipboard"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\LocalClipboard" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\LocalClipboard" "UninstallString" '$"$INSTDIR\Uninstall.exe$"'
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\LocalClipboard" "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\LocalClipboard" "NoRepair" 1
SectionEnd
Section "Uninstall"
    SetShellVarContext current
    nsExec::ExecToLog 'taskkill /IM LocalClipboard.exe /F'
    DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "LocalClipboard"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\LocalClipboard"
    RMDir /r "$SMPROGRAMS\LocalClipboard"
    RMDir /r "$INSTDIR"
    ; Keep the personal profile and received files so reinstalling does not lose registration.
SectionEnd
