; packaging\finanzas.iss — instalador de Finanzas DakeLabs para Windows.
;
; Lo compila armar-instalador.ps1 (en esta misma carpeta), que le pasa la
; version tomada de CMakeLists.txt con /DAppVersion=X.Y.Z. Si lo corres a mano
; con ISCC.exe sin ese parametro, usa el valor de reserva de aca abajo.
;
; Empaqueta lo que dejo windeployqt en build\release\bin: hace falta haber
; corrido ".\compilar.ps1 release" antes. No copia los binarios de prueba
; (dake_*test.exe, dake_uipreview.exe, dake_movil.exe) ni el .exe viejo que
; queda a veces en esa carpeta: solo dake_pruebas.exe y lo que necesita para
; correr.

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

#define AppName "Finanzas DakeLabs"
#define AppPublisher "DakeLabs"
#define AppExeName "dake_pruebas.exe"
#define ReleaseDir "..\build\release\bin"

[Setup]
AppId={{6D96E832-0555-4D6C-93CD-62F5A710DEFA}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppPublisher}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=FinanzasDakeLabs-{#AppVersion}-instalador
SetupIconFile=finanzas.ico
UninstallDisplayIcon={app}\{#AppExeName}
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
; Instala para el usuario actual, sin pedir permisos de administrador: es lo
; que corresponde para una app de un taller de una sola persona.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline dialog

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "Crear un acceso directo en el escritorio"; GroupDescription: "Accesos directos:"; Flags: unchecked

[Files]
Source: "{#ReleaseDir}\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleaseDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleaseDir}\generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\qml\*"; DestDir: "{app}\qml"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\qmltooling\*"; DestDir: "{app}\qmltooling"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\sqldrivers\*"; DestDir: "{app}\sqldrivers"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\Desinstalar {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Abrir {#AppName}"; Flags: nowait postinstall skipifsilent
