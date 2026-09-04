# construir-android.ps1 — arma el APK para el telefono.
#
# Todo lo que hace falta ya esta instalado en este equipo. Las rutas van escritas
# arriba para que el guion no dependa de variables de entorno del sistema: la
# unica forma de que un guion de compilacion siga funcionando en tres meses es
# que no suponga nada del entorno.
#
#   .\construir-android.ps1              compila e imprime donde quedo el APK
#   .\construir-android.ps1 -Limpiar     borra build\android y empieza de cero
#
# El APK sale firmado con la clave de depuracion, que es lo correcto para
# instalarlo a mano en el telefono propio. Para publicarlo en Play haria falta
# una clave de firma de verdad, que es otra conversacion.

param(
    [switch]$Limpiar
)

$ErrorActionPreference = "Stop"

$Jdk    = "C:\Program Files\Microsoft\jdk-17.0.20.101-hotspot"
$Sdk    = "C:\Android\Sdk"
$Ndk    = "$Sdk\ndk\26.1.10909125"
$Tools  = "$Sdk\build-tools\35.0.1"
$QtHost = "C:\Qt\6.8.3\msvc2022_64"
$QtAndr = "C:\Qt\6.8.3\android_arm64_v8a"
$Ninja  = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
# qt-cmake.bat llama a `cmake` a secas, asi que tiene que estar en el PATH. En
# este equipo el unico cmake instalado es el que trae Visual Studio.
$CMake  = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"

foreach ($ruta in @($Jdk, $Sdk, $Ndk, $QtHost, $QtAndr, $CMake, $Tools)) {
    if (-not (Test-Path $ruta)) {
        Write-Error "No esta: $ruta"
    }
}

$env:JAVA_HOME        = $Jdk
$env:ANDROID_SDK_ROOT = $Sdk
$env:ANDROID_NDK_ROOT = $Ndk
$env:PATH             = "$CMake;$Ninja;$Jdk\bin;$env:PATH"

$raiz  = $PSScriptRoot
$build = Join-Path $raiz "build\android"

if ($Limpiar -and (Test-Path $build)) {
    Write-Output "Borrando $build"
    Remove-Item $build -Recurse -Force
}

Write-Output "== Configurando =="
& "$QtAndr\bin\qt-cmake.bat" `
    -S $raiz `
    -B $build `
    -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DQT_HOST_PATH="$QtHost" `
    -DQT_ANDROID_ABIS=arm64-v8a
if ($LASTEXITCODE -ne 0) { Write-Error "Fallo la configuracion" }

Write-Output "== Compilando el APK =="
& cmake --build $build --target apk
if ($LASTEXITCODE -ne 0) { Write-Error "Fallo la compilacion" }

# androiddeployqt deja el APK dentro de la carpeta de gradle del target.
$apk = Get-ChildItem -Path $build -Recurse -Filter "*.apk" -ErrorAction SilentlyContinue |
       Where-Object { $_.FullName -like "*debug*" -or $_.FullName -like "*release*" } |
       Sort-Object LastWriteTime -Descending |
       Select-Object -First 1

if ($null -eq $apk) {
    Write-Error "Compilo pero no encontre el APK."
}

$destino = Join-Path $raiz "apk"
New-Item -ItemType Directory -Force -Path $destino | Out-Null
$final = Join-Path $destino "FinanzasDakeLabs.apk"
# androiddeployqt entrega el APK SIN FIRMAR, y Android no instala un paquete sin
# firma: el telefono lo rechaza con "problema al analizar el paquete" y no dice
# por que. Se firma con la clave de depuracion, que es la correcta para
# instalarlo a mano en el telefono propio.
$almacen = Join-Path $env:USERPROFILE ".android\debug.keystore"

if (-not (Test-Path $almacen)) {
    Write-Output "== Creando la clave de depuracion =="
    New-Item -ItemType Directory -Force -Path (Split-Path $almacen) | Out-Null
    & "$Jdk\bin\keytool.exe" -genkeypair -keystore $almacen -storepass android `
        -keypass android -alias androiddebugkey -keyalg RSA -keysize 2048 `
        -validity 10950 -dname "CN=Android Debug,O=Android,C=US"
    if ($LASTEXITCODE -ne 0) { Write-Error "No se pudo crear la clave" }
}

Write-Output "== Alineando y firmando =="
# zipalign va ANTES de firmar: alinear despues invalida la firma.
& "$Tools\zipalign.exe" -f -p 4 $apk.FullName $final
if ($LASTEXITCODE -ne 0) { Write-Error "Fallo zipalign" }

& "$Tools\apksigner.bat" sign --ks $almacen --ks-pass pass:android `
    --key-pass pass:android --ks-key-alias androiddebugkey $final
if ($LASTEXITCODE -ne 0) { Write-Error "Fallo la firma" }

& "$Tools\apksigner.bat" verify $final
if ($LASTEXITCODE -ne 0) { Write-Error "El APK quedo sin firma valida" }

Write-Output ""
Write-Output "APK listo: $final"
Write-Output "Tamaño: $([math]::Round((Get-Item $final).Length / 1MB, 1)) MB"
