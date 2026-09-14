Add-Type -AssemblyName System.Drawing

$images = @{
    'wall_cyber_neon.png'  = 'C:\Users\HARU17\.gemini\antigravity-ide\brain\24e6e578-346c-4e14-b296-f2d456143b23\wall_cyber_neon_1789385746475.jpg';
    'wall_honeycomb.png'   = 'C:\Users\HARU17\.gemini\antigravity-ide\brain\24e6e578-346c-4e14-b296-f2d456143b23\wall_honeycomb_1789385765719.jpg';
    'wall_comic_block.png' = 'C:\Users\HARU17\.gemini\antigravity-ide\brain\24e6e578-346c-4e14-b296-f2d456143b23\wall_comic_block_1789385788595.jpg';
}

$destDirs = @('Assets\texture', 'x64\Debug', 'x64\Release\Assets\texture')
foreach ($dir in $destDirs) {
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force }
}

foreach ($name in $images.Keys) {
    $srcPath = $images[$name]
    $bmp = [System.Drawing.Bitmap]::FromFile($srcPath)
    $outPath = Join-Path 'Assets\texture' $name
    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "Saved $outPath"

    Copy-Item $outPath -Destination 'x64\Debug\' -Force
    Copy-Item $outPath -Destination 'x64\Release\Assets\texture\' -Force
}

# Default to wall_cyber_neon.png as wall.png
Copy-Item 'Assets\texture\wall_cyber_neon.png' -Destination 'Assets\texture\wall.png' -Force
Copy-Item 'Assets\texture\wall_cyber_neon.png' -Destination 'x64\Debug\wall.png' -Force
Copy-Item 'Assets\texture\wall_cyber_neon.png' -Destination 'x64\Release\Assets\texture\wall.png' -Force
Write-Host "Copied default wall.png successfully"
