; Copyright 2016 Christoph Reiter
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; Taken from Quod Libet and modified for Parlatype by Gabor Karsay.


Unicode true

!define PT_NAME "Parlatype"
!define PT_ID "parlatype"
!define PT_DESC "Music Player"
!define PT_WEBSITE "https://www.parlatype.org"

!define PT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PT_NAME}"
!define PT_INSTDIR_KEY "Software\${PT_NAME}"
!define PT_INSTDIR_VALUENAME "InstDir"

!define MUI_CUSTOMFUNCTION_GUIINIT custom_gui_init
!include "MUI2.nsh"
!include "FileFunc.nsh"

Name "${PT_NAME} (${VERSION})"
OutFile "parlatype-LATEST.exe"
SetCompressor /SOLID /FINAL lzma
SetCompressorDictSize 32
InstallDir "$PROGRAMFILES\${PT_NAME}"
RequestExecutionLevel admin

Var PT_INST_BIN
Var UNINST_BIN

!define MUI_ABORTWARNING
!define MUI_ICON "..\parlatype.ico"

!insertmacro MUI_PAGE_LICENSE "..\parlatype\COPYING"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Arabic"
!insertmacro MUI_LANGUAGE "Catalan"
!insertmacro MUI_LANGUAGE "Czech"
!insertmacro MUI_LANGUAGE "Dutch"
!insertmacro MUI_LANGUAGE "Estonian"
!insertmacro MUI_LANGUAGE "Finnish"
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "Hungarian"
!insertmacro MUI_LANGUAGE "Indonesian"
!insertmacro MUI_LANGUAGE "Italian"
!insertmacro MUI_LANGUAGE "Japanese"
!insertmacro MUI_LANGUAGE "Kurdish"
!insertmacro MUI_LANGUAGE "Latvian"
!insertmacro MUI_LANGUAGE "Lithuanian"
!insertmacro MUI_LANGUAGE "Malay"
!insertmacro MUI_LANGUAGE "Polish"
!insertmacro MUI_LANGUAGE "PortugueseBR"
!insertmacro MUI_LANGUAGE "Portuguese"
!insertmacro MUI_LANGUAGE "SerbianLatin"
!insertmacro MUI_LANGUAGE "Serbian"
!insertmacro MUI_LANGUAGE "Slovak"
!insertmacro MUI_LANGUAGE "Spanish"
!insertmacro MUI_LANGUAGE "Swedish"


Section "Install"
    SetShellVarContext all

    ; Use this to make things faster for testing installer changes
    ;~ SetOutPath "$INSTDIR\bin"
    ;~ File /r "mingw32\bin\*.exe"

    SetOutPath "$INSTDIR"
    File /r "*.*"

    StrCpy $PT_INST_BIN "$INSTDIR\bin\parlatype.exe"
    StrCpy $UNINST_BIN "$INSTDIR\uninstall.exe"

    ; Store installation folder
    WriteRegStr HKLM "${PT_INSTDIR_KEY}" "${PT_INSTDIR_VALUENAME}" $INSTDIR

    ; Set up an entry for the uninstaller
    WriteRegStr HKLM "${PT_UNINST_KEY}" \
        "DisplayName" "${PT_NAME} - ${PT_DESC}"
    WriteRegStr HKLM "${PT_UNINST_KEY}" "DisplayIcon" "$\"$PT_INST_BIN$\""
    WriteRegStr HKLM "${PT_UNINST_KEY}" "UninstallString" \
        "$\"$UNINST_BIN$\""
    WriteRegStr HKLM "${PT_UNINST_KEY}" "QuietUninstallString" \
    "$\"$UNINST_BIN$\" /S"
    WriteRegStr HKLM "${PT_UNINST_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "${PT_UNINST_KEY}" "HelpLink" "${PT_WEBSITE}"
    WriteRegStr HKLM "${PT_UNINST_KEY}" "Publisher" "The ${PT_NAME} Development Community"
    WriteRegStr HKLM "${PT_UNINST_KEY}" "DisplayVersion" "${VERSION}"
    WriteRegDWORD HKLM "${PT_UNINST_KEY}" "NoModify" 0x1
    WriteRegDWORD HKLM "${PT_UNINST_KEY}" "NoRepair" 0x1
    ; Installation size
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "${PT_UNINST_KEY}" "EstimatedSize" "$0"

    ; Register a default entry for file extensions
    WriteRegStr HKLM "Software\Classes\${PT_ID}.assoc.ANY\shell\play\command" "" "$\"$PT_INST_BIN$\" $\"%1$\""
    WriteRegStr HKLM "Software\Classes\${PT_ID}.assoc.ANY\DefaultIcon" "" "$\"$PT_INST_BIN$\""
    WriteRegStr HKLM "Software\Classes\${PT_ID}.assoc.ANY\shell\play" "FriendlyAppName" "${PT_NAME}"

    ; Add application entry
    WriteRegStr HKLM "Software\${PT_NAME}\${PT_ID}\Capabilities" "ApplicationDescription" "${PT_DESC}"
    WriteRegStr HKLM "Software\${PT_NAME}\${PT_ID}\Capabilities" "ApplicationName" "${PT_NAME}"

    ; Register supported file extensions
    ; (generated using gen_supported_types.py)
    !define PT_ASSOC_KEY "Software\${PT_NAME}\${PT_ID}\Capabilities\FileAssociations"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".3g2" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".3gp" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".3gp2" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".669" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".aac" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".adif" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".adts" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".aif" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".aifc" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".aiff" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".amf" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".ams" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".ape" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".asf" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".dsf" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".dsm" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".far" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".flac" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".gdm" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".it" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".m4a" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".m4v" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".med" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".mid" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".mod" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".mp+" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".mp1" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".mp2" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".mp3" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".mp4" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".mpc" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".mpeg" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".mpg" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".mt2" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".mtm" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".oga" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".ogg" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".oggflac" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".ogv" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".okt" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".opus" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".s3m" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".spc" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".spx" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".stm" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".tta" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".ult" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".vgm" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".wav" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".wma" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".wmv" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".wv" "${PT_ID}.assoc.ANY"
    WriteRegStr HKLM "${PT_ASSOC_KEY}" ".xm" "${PT_ID}.assoc.ANY"

    ; Register application entry
    WriteRegStr HKLM "Software\RegisteredApplications" "${PT_NAME}" "Software\${PT_NAME}\${PT_ID}\Capabilities"

    ; Register app paths
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\parlatype.exe" "" "$PT_INST_BIN"

    ; Create uninstaller
    WriteUninstaller "$UNINST_BIN"

    ; Create start menu shortcuts
    CreateDirectory "$SMPROGRAMS\${PT_NAME}"
    CreateShortCut "$SMPROGRAMS\${PT_NAME}\${PT_NAME}.lnk" "$PT_INST_BIN"
SectionEnd

Function custom_gui_init
    BringToFront

    ; Read the install dir and set it
    Var /GLOBAL instdir_temp
    Var /GLOBAL uninst_bin_temp

    SetRegView 32
    ReadRegStr $instdir_temp HKLM "${PT_INSTDIR_KEY}" "${PT_INSTDIR_VALUENAME}"
    SetRegView lastused
    StrCmp $instdir_temp "" skip 0
        StrCpy $INSTDIR $instdir_temp
    skip:

    SetRegView 64
    ReadRegStr $instdir_temp HKLM "${PT_INSTDIR_KEY}" "${PT_INSTDIR_VALUENAME}"
    SetRegView lastused
    StrCmp $instdir_temp "" skip2 0
        StrCpy $INSTDIR $instdir_temp
    skip2:

    StrCpy $uninst_bin_temp "$INSTDIR\uninstall.exe"

    ; try to un-install existing installations first
    IfFileExists "$INSTDIR" do_uninst do_continue
    do_uninst:
        ; instdir exists
        IfFileExists "$uninst_bin_temp" exec_uninst rm_instdir
        exec_uninst:
            ; uninstall.exe exists, execute it and
            ; if it returns success proceed, otherwise abort the
            ; installer (uninstall aborted by user for example)
            ExecWait '"$uninst_bin_temp" _?=$INSTDIR' $R1
            ; uninstall succeeded, since the uninstall.exe is still there
            ; goto rm_instdir as well
            StrCmp $R1 0 rm_instdir
            ; uninstall failed
            Abort
        rm_instdir:
            ; either the uninstaller was successfull or
            ; the uninstaller.exe wasn't found
            RMDir /r "$INSTDIR"
    do_continue:
        ; the instdir shouldn't exist from here on

    BringToFront
FunctionEnd

Section "Uninstall"
    SetShellVarContext all
    SetAutoClose true

    ; Remove start menu entries
    Delete "$SMPROGRAMS\${PT_NAME}\${PT_NAME}.lnk"
    RMDir "$SMPROGRAMS\${PT_NAME}"

    ; Remove application registration and file assocs
    DeleteRegKey HKLM "Software\Classes\${PT_ID}.assoc.ANY"
    DeleteRegKey HKLM "Software\${PT_NAME}"
    DeleteRegValue HKLM "Software\RegisteredApplications" "${PT_NAME}"

    ; Remove app paths
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\parlatype.exe"

    ; Delete installation related keys
    DeleteRegKey HKLM "${PT_UNINST_KEY}"
    DeleteRegKey HKLM "${PT_INSTDIR_KEY}"

    ; Delete files
    RMDir /r "$INSTDIR"
SectionEnd
