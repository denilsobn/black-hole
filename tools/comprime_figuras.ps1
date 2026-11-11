param([string]$Dir = "docs\artigo\figs")

Add-Type -AssemblyName System.Drawing

$total0 = 0; $total1 = 0
Get-ChildItem $Dir -Filter *.png | ForEach-Object {
    $antes = $_.Length
    $tmp = [System.IO.Path]::ChangeExtension($_.FullName, ".tmp.png")

    $img = [System.Drawing.Image]::FromFile($_.FullName)
    $bmp = New-Object System.Drawing.Bitmap $img
    $img.Dispose()
    $bmp.Save($tmp, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()

    Move-Item -Force $tmp $_.FullName
    $depois = (Get-Item $_.FullName).Length
    $total0 += $antes; $total1 += $depois
    "{0,-34} {1,8:N0} -> {2,8:N0} KB" -f $_.Name, ($antes/1KB), ($depois/1KB)
}
""
"total: {0:N0} KB -> {1:N0} KB  ({2:N1}x menor)" -f ($total0/1KB), ($total1/1KB), ($total0/[math]::Max($total1,1))
