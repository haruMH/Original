Add-Type -AssemblyName System.Drawing
foreach ($name in @('wall.png', 'wall_cyber_neon.png', 'wall_honeycomb.png', 'wall_comic_block.png')) {
    $path = (Resolve-Path "Assets\texture\$name").Path
    $bmp = [System.Drawing.Bitmap]::FromFile($path)
    Write-Host "$($name) Width=$($bmp.Width) Height=$($bmp.Height)"
    $bmp.Dispose()
}
