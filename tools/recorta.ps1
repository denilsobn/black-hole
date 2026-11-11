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
