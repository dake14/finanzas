# generar-icono.ps1 — arma packaging\finanzas.ico a partir del logo ya
# incrustado en la app (ui\recursos\logo\DAke.png).
#
# Un .ico moderno es una lista de imagenes PNG de distinto tamano metidas en
# un contenedor ICO; Windows las usa desde Vista para arriba. No hace falta
# ninguna herramienta aparte: System.Drawing alcanza para redimensionar y
# codificar cada tamano.

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$raiz = Split-Path $PSScriptRoot -Parent
$origen = Join-Path $raiz "ui\recursos\logo\DAke.png"
$destino = Join-Path $PSScriptRoot "finanzas.ico"
$tamanos = @(16, 32, 48, 64, 128, 256)

$fuente = [System.Drawing.Image]::FromFile($origen)
$paginas = New-Object System.Collections.Generic.List[byte[]]
foreach ($lado in $tamanos) {
    $lienzo = New-Object System.Drawing.Bitmap $lado, $lado
    $g = [System.Drawing.Graphics]::FromImage($lienzo)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.DrawImage($fuente, 0, 0, $lado, $lado)
    $g.Dispose()
    $mem = New-Object System.IO.MemoryStream
    $lienzo.Save($mem, [System.Drawing.Imaging.ImageFormat]::Png)
    $paginas.Add($mem.ToArray())
    $mem.Dispose()
    $lienzo.Dispose()
}
$fuente.Dispose()

$out = New-Object System.IO.MemoryStream
$w = New-Object System.IO.BinaryWriter $out
$w.Write([uint16]0)          # reservado
$w.Write([uint16]1)          # tipo: icono
$w.Write([uint16]$paginas.Count)

$offset = 6 + 16 * $paginas.Count
for ($i = 0; $i -lt $paginas.Count; $i++) {
    $lado = $tamanos[$i]
    $byteLado = if ($lado -ge 256) { 0 } else { $lado }
    $w.Write([byte]$byteLado)   # ancho
    $w.Write([byte]$byteLado)   # alto
    $w.Write([byte]0)           # paleta
    $w.Write([byte]0)           # reservado
    $w.Write([uint16]1)         # planos de color
    $w.Write([uint16]32)        # bits por pixel
    $w.Write([uint32]$paginas[$i].Length)
    $w.Write([uint32]$offset)
    $offset += $paginas[$i].Length
}
foreach ($pagina in $paginas) {
    $w.Write($pagina)
}
$w.Flush()
[System.IO.File]::WriteAllBytes($destino, $out.ToArray())
$w.Dispose()
$out.Dispose()

Write-Output "Listo: $destino"
