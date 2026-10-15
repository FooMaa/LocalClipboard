Unicode true
!include "MUI2.nsh"
!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!define MUI_ICON "..\..\assets\icons\localclipboard.ico"
!define MUI_UNICON "..\..\assets\icons\localclipboard.ico"
Name "LocalClipboard"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\LocalClipboard"
RequestExecutionLevel user
SetCompressor /SOLID lzma
Var MaintenanceRadio
Var ExistingDirectory
Var AppLanguage
Var EraseData
Var EraseRadio
Var KeepRadio
!define MUI_LANGDLL_REGISTRY_ROOT HKCU
!define MUI_LANGDLL_REGISTRY_KEY "Software\LocalClipboard\Installer"
!define MUI_LANGDLL_REGISTRY_VALUENAME "Language"
!define MUI_LANGDLL_ALWAYSSHOW
!define MUI_WELCOMEPAGE_TEXT "$(WelcomeText)"
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION StartApp
!define MUI_FINISHPAGE_RUN_TEXT "$(RunText)"
!insertmacro MUI_PAGE_WELCOME
Page custom MaintenancePage MaintenanceLeave
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
UninstPage custom un.DataPage un.DataPageLeave
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Russian"
!insertmacro MUI_RESERVEFILE_LANGDLL
LangString WelcomeText ${LANG_ENGLISH} "Send text, photos and files between your own devices on the same local network.$\r$\n$\r$\nAfter installation, register this device with a personal profile. Each person needs their own profile, even on the same Wi-Fi."
LangString WelcomeText ${LANG_RUSSIAN} "Передавайте текст, фото и файлы между вашими устройствами в одной локальной сети.$\r$\n$\r$\nПосле установки зарегистрируйте устройство в личном профиле. У каждого человека должен быть свой профиль, даже в одной сети Wi-Fi."
LangString RunText ${LANG_ENGLISH} "Register / open LocalClipboard"
LangString RunText ${LANG_RUSSIAN} "Зарегистрировать / открыть LocalClipboard"
LangString DataTitle ${LANG_ENGLISH} "Your local data"
LangString DataTitle ${LANG_RUSSIAN} "Ваши локальные данные"
LangString DataSubtitle ${LANG_ENGLISH} "Keep your profile and files, or erase them from this device."
LangString DataSubtitle ${LANG_RUSSIAN} "Сохраните профиль и файлы или удалите их с этого устройства."
LangString EraseText ${LANG_ENGLISH} "Erase my local data"
LangString EraseText ${LANG_RUSSIAN} "Стереть мои данные"
LangString KeepText ${LANG_ENGLISH} "Keep my profile and files"
LangString KeepText ${LANG_RUSSIAN} "Сохранить профиль и файлы"
LangString DataHelp ${LANG_ENGLISH} "Keep: registration and received files remain for reinstalling.$\r$\n$\r$\nErase: registration, received files, imported copies and language preference are erased on this device. Other devices are unaffected."
LangString DataHelp ${LANG_RUSSIAN} "Сохранить: регистрация и файлы останутся для повторной установки.$\r$\n$\r$\nСтереть: регистрация, полученные файлы, импортированные копии и выбранный язык будут удалены с этого устройства. Данные остальных устройств сохранятся."
LangString CloseApp ${LANG_ENGLISH} "Please quit LocalClipboard from its tray menu, then click Retry."
LangString CloseApp ${LANG_RUSSIAN} "Завершите LocalClipboard через меню в трее и нажмите «Повторить»."
LangString MaintenanceTitle ${LANG_ENGLISH} "LocalClipboard is already installed"
LangString MaintenanceTitle ${LANG_RUSSIAN} "LocalClipboard уже установлен"
LangString MaintenanceHelp ${LANG_ENGLISH} "Choose what to do with this installation."
LangString MaintenanceHelp ${LANG_RUSSIAN} "Выберите действие для установленного приложения."
LangString ReinstallText ${LANG_ENGLISH} "Reinstall (keep my profile and files)"
LangString ReinstallText ${LANG_RUSSIAN} "Переустановить (сохранить профиль и файлы)"
LangString RemoveText ${LANG_ENGLISH} "Uninstall LocalClipboard"
LangString RemoveText ${LANG_RUSSIAN} "Удалить LocalClipboard"
Function MaintenancePage
    IfFileExists "$ExistingDirectory\Uninstall.exe" 0 no_maintenance
    !insertmacro MUI_HEADER_TEXT "$(MaintenanceTitle)" "$(MaintenanceHelp)"
    nsDialogs::Create 1018
    Pop $0
    ${NSD_CreateRadioButton} 0 0 100% 30u "$(ReinstallText)"
    Pop $0
    ${NSD_Check} $0
    ${NSD_CreateRadioButton} 0 45u 100% 30u "$(RemoveText)"
    Pop $MaintenanceRadio
    nsDialogs::Show
    Return
    no_maintenance:
    Abort
FunctionEnd
Function MaintenanceLeave
    ${NSD_GetState} $MaintenanceRadio $0
    ${If} $0 == ${BST_CHECKED}
        HideWindow
        ExecWait '"$ExistingDirectory\Uninstall.exe"'
        Quit
    ${EndIf}
FunctionEnd
Function .onInit
    ReadRegStr $ExistingDirectory HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\LocalClipboard" "InstallLocation"
    ${If} $ExistingDirectory == ""
        StrCpy $ExistingDirectory $INSTDIR
    ${Else}
        StrCpy $INSTDIR $ExistingDirectory
    ${EndIf}
    !insertmacro MUI_LANGDLL_DISPLAY
    StrCpy $AppLanguage "en"
    ${If} $LANGUAGE == ${LANG_RUSSIAN}
        StrCpy $AppLanguage "ru"
    ${EndIf}
FunctionEnd
Function StartApp
    Exec '"$INSTDIR\LocalClipboard.exe" --language $AppLanguage'
FunctionEnd
Function un.onInit
    !insertmacro MUI_UNGETLANGUAGE
    StrCpy $EraseData ${BST_UNCHECKED}
FunctionEnd
Function un.DataPage
    !insertmacro MUI_HEADER_TEXT "$(DataTitle)" "$(DataSubtitle)"
    nsDialogs::Create 1018
    Pop $0
    ${If} $0 == error
        Abort
    ${EndIf}
    ${NSD_CreateRadioButton} 0 0 100% 20u "$(KeepText)"
    Pop $KeepRadio
    ${NSD_CreateRadioButton} 0 28u 100% 20u "$(EraseText)"
    Pop $EraseRadio
    ${If} $EraseData == ${BST_CHECKED}
        ${NSD_Check} $EraseRadio
    ${Else}
        ${NSD_Check} $KeepRadio
    ${EndIf}
    ${NSD_CreateLabel} 0 60u 100% 80u "$(DataHelp)"
    Pop $0
    nsDialogs::Show
FunctionEnd
Function un.DataPageLeave
    ${NSD_GetState} $EraseRadio $EraseData
FunctionEnd
Section "LocalClipboard"
    SetShellVarContext current
    IfFileExists "$INSTDIR\LocalClipboard.exe" 0 +3
        ExecWait '"$INSTDIR\LocalClipboard.exe" --quit'
        Sleep 500
    SetOutPath "$INSTDIR"
    File /r "${DEPLOY_DIR}\*"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    CreateDirectory "$SMPROGRAMS\LocalClipboard"
    CreateShortcut "$SMPROGRAMS\LocalClipboard\LocalClipboard.lnk" "$INSTDIR\LocalClipboard.exe"
    CreateShortcut "$SMPROGRAMS\LocalClipboard\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\LocalClipboard" "InstallLocation" "$INSTDIR"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\LocalClipboard" "DisplayName" "LocalClipboard"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\LocalClipboard" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\LocalClipboard" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\LocalClipboard" "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\LocalClipboard" "NoRepair" 1
SectionEnd
Section "Uninstall"
    SetShellVarContext current
    ExecWait '"$INSTDIR\LocalClipboard.exe" --quit'
    Sleep 500
    DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "LocalClipboard"
    uninstall_remove:
    RMDir /r "$INSTDIR"
    IfFileExists "$INSTDIR\LocalClipboard.exe" 0 uninstall_done
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "$(CloseApp)" IDRETRY uninstall_remove
    Abort
    uninstall_done:
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\LocalClipboard"
    RMDir /r "$SMPROGRAMS\LocalClipboard"
    ${If} $EraseData == ${BST_CHECKED}
        RMDir /r "$APPDATA\LocalClipboard\LocalClipboard"
        RMDir /r "$LOCALAPPDATA\LocalClipboard\LocalClipboard"
        DeleteRegKey HKCU "Software\LocalClipboard\Installer"
    ${EndIf}
SectionEnd
