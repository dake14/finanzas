# armar-instalador.ps1 — empaqueta el instalador de Finanzas DakeLabs.
#
# Toma la version de CMakeLists.txt (una sola fuente de verdad: no hay que
# tocar dos archivos cuando cambia) y compila packaging\finanzas.iss con
# Inno Setup. Antes hay que haber compilado la version de entrega:
#
#   .\compilar.ps1 release
#   .\packaging\armar-instalador.ps1
#
# El instalador sale en dist\FinanzasDakeLabs-<version>-instalador.exe.

$ErrorActionPreference = "Stop"

$raiz = Split-Path $PSScriptRoot -Parent
$exeVersion = Join-Path $raiz "build\release\bin\dake_pruebas.exe"
if (-not (Test-Path $exeVersion)) {
    Write-Error "No esta compilada la version de entrega: corre primero '.\compilar.ps1 release' desde la raiz del proyecto."
}

$cmake = Get-Content (Join-Path $raiz "CMakeLists.txt") -Raw
$match = [regex]::Match($cmake, "VERSION\s+(\d+\.\d+\.\d+)")
if (-not $match.Success) {
    Write-Error "No pude leer la version desde CMakeLists.txt."
}
$version = $match.Groups[1].Value

# Segun como lo instale winget, ISCC.exe puede quedar en Program Files o en
# el AppData del usuario (Inno Setup no pide administrador por defecto).
$candidatos = @(
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
)
$iscc = $candidatos | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) {
    $comando = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($null -eq $comando) {
        Write-Error "No encuentro ISCC.exe (Inno Setup 6). Instalalo con: winget install -e --id JRSoftware.InnoSetup"
    }
    $iscc = $comando.Source
}

& $iscc "/DAppVersion=$version" (Join-Path $PSScriptRoot "finanzas.iss")
if ($LASTEXITCODE -ne 0) { Write-Error "Fallo la compilacion del instalador" }

Write-Output "Listo: dist\FinanzasDakeLabs-$version-instalador.exe"
