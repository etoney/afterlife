; Afterlife setup for Windows (Inno Setup 6). CI builds it with:  ISCC /DAppVer=<version> installer\windows\afterlife.iss
;   VST3          -> C:\Program Files\Common Files\VST3\Afterlife.vst3
;   quick guides  -> C:\Program Files\Castle Danger Music Company\Guides
; Uninstall from Settings > Apps, or the Start menu.
#ifndef AppVer
  #define AppVer "dev"
#endif

[Setup]
AppId={{6B0E3C55-8F7A-4D2B-9C6E-AF7E2D1B4A01}
AppName=Afterlife
AppVersion={#AppVer}
AppVerName=Afterlife {#AppVer}
AppPublisher=Castle Danger Music Company
AppPublisherURL=https://castledangermusic.com
DefaultDirName={autopf}\Castle Danger Music Company
DisableDirPage=yes
DefaultGroupName=Castle Danger Music Company
DisableProgramGroupPage=yes
LicenseFile=..\..\LICENSE.txt
OutputDir=..\..\build-win
OutputBaseFilename=Afterlife-v{#AppVer}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
UninstallDisplayName=Afterlife (Castle Danger Music Company)
VersionInfoCompany=Castle Danger Music Company
VersionInfoDescription=Afterlife setup

[InstallDelete]
; a fresh copy every time, so no files from an older version are left inside the bundle
Type: filesandordirs; Name: "{commoncf64}\VST3\Afterlife.vst3"

[Files]
Source: "..\..\build\Afterlife_artefacts\Release\VST3\Afterlife.vst3\*"; DestDir: "{commoncf64}\VST3\Afterlife.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\..\controls.txt"; DestDir: "{app}\Guides"; DestName: "Afterlife - controls.txt"; Flags: ignoreversion
Source: "..\..\THIRD-PARTY-NOTICES.txt"; DestDir: "{app}\Guides"; DestName: "Afterlife - third-party notices.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\Afterlife controls"; Filename: "{app}\Guides\Afterlife - controls.txt"
Name: "{group}\Uninstall Afterlife"; Filename: "{uninstallexe}"

[Messages]
WelcomeLabel2=This installs Afterlife {#AppVer}, a convolution reverb and delay with section lanes, as a VST3 plug-in for every DAW on this PC.%n%nThe Echoes Never Die.
FinishedLabel=Afterlife is installed. Open Fender Studio Pro (or your DAW), rescan plug-ins, and find it under Castle Danger.
