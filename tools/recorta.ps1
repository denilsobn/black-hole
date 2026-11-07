# =====================================================================
#  recorta.ps1 -- recorta parte de uma imagem para as figuras.
#
#  Sem -W/-H: recorta a regiao CENTRAL, com a mesma proporcao, ocupando
#  a fracao -Fracao da largura e da altura (figuras do artigo: mostram a
#  sombra e os aneis legiveis mesmo com quatro paineis lado a lado).
#
#  Com -X -Y -W -H: recorta esse retangulo e o amplia -Escala vezes, sem
#  interpolar (detalhe da costura do disco, no README).
#
#  Exemplos:
#    powershell -ExecutionPolicy Bypass -File tools\recorta.ps1 `
#        -In figs\r_rk4.png -Out figs\r_rk4_c.png -Fracao 0.55
#    powershell -ExecutionPolicy Bypass -File tools\recorta.ps1 `
#        -In a.png -Out b.png -X 600 -Y 300 -W 210 -H 170 -Escala 2
# =====================================================================
param([Parameter(Mandatory=$true)][string]$In,
      [Parameter(Mandatory=$true)][string]$Out,
      [double]$Fracao = 0.6,
      [int]$X = -1, [int]$Y = -1, [int]$W = 0, [int]$H = 0,
      [int]$Escala = 1)

Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $In))

if ($W -le 0 -or $H -le 0) {
    $W = [int]($src.Width * $Fracao)
    $H = [int]($src.Height * $Fracao)
    $X = [int](($src.Width - $W) / 2)
    $Y = [int](($src.Height - $H) / 2)
}

$dst = New-Object System.Drawing.Bitmap ($W * $Escala), ($H * $Escala)
$g = [System.Drawing.Graphics]::FromImage($dst)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
$g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, ($W * $Escala), ($H * $Escala)),
             (New-Object System.Drawing.Rectangle $X, $Y, $W, $H),
             [System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose(); $src.Dispose()
$dst.Save([System.IO.Path]::GetFullPath($Out), [System.Drawing.Imaging.ImageFormat]::Png)
$dst.Dispose()
"ok: $Out ($($W * $Escala)x$($H * $Escala))"
