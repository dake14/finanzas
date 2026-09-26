# compilar.ps1 — compila para Windows sin tener que abrir una consola de MSVC.
#
# `cmake --build --preset debug` necesita cl.exe en el PATH, y cl.exe solo
# aparece despues de correr vcvars64.bat. Ese guion exporta variables en un
# `cmd`, no en PowerShell, asi que la unica forma de encadenarlos es lanzarlos
# juntos dentro del mismo `cmd`.
#
#   .\compilar.ps1              compila la version de depuracion
#   .\compilar.ps1 release      compila la de entrega
#   .\compilar.ps1 debug -Probar  compila y ademas corre las verificaciones

param(
    [ValidateSet("debug", "release")]
    [string]$Config = "debug",
    [switch]$Probar
)

$ErrorActionPreference = "Stop"

$vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    Write-Error "No esta el entorno de MSVC: $vcvars"
}

$raiz = $PSScriptRoot
$orden = "`"$vcvars`" >nul 2>&1 && cmake --build --preset $Config"
if ($Probar) {
    $orden += " && ctest --test-dir `"$raiz\build\$Config`" --output-on-failure"
}

cmd /c $orden
if ($LASTEXITCODE -ne 0) { Write-Error "Fallo la compilacion" }

Write-Output "Listo: build\$Config\bin"
