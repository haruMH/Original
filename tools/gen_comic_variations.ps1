Add-Type -AssemblyName System.Drawing

$srcPath = 'C:\Users\HARU17\.gemini\antigravity-ide\brain\24e6e578-346c-4e14-b296-f2d456143b23\wall_comic_block_1789385788595.jpg'
$bmpOriginal = [System.Drawing.Bitmap]::FromFile($srcPath)
$width = $bmpOriginal.Width
$height = $bmpOriginal.Height

# バリエーション1: シアン発光ルーン・目地（サイバーネオン×トゥーン石積み）
$bmpCyan = New-Object System.Drawing.Bitmap($width, $height)
# バリエーション2: アメジスト・オブシディアン（深紫・黒曜石ハイコントラスト）
$bmpAmethyst = New-Object System.Drawing.Bitmap($width, $height)
# バリエーション3: クリムゾン・ブラッド（赤・真紅のエネルギー目地）
$bmpCrimson = New-Object System.Drawing.Bitmap($width, $height)

# ロックして高速にバイト配列を処理
$rect = New-Object System.Drawing.Rectangle(0, 0, $width, $height)
$bmpDataOrig = $bmpOriginal.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$bmpDataCyan = $bmpCyan.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$bmpDataAme  = $bmpAmethyst.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$bmpDataCrim = $bmpCrimson.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)

$bytes = $width * $height * 4
$bufOrig = New-Object byte[] $bytes
$bufCyan = New-Object byte[] $bytes
$bufAme  = New-Object byte[] $bytes
$bufCrim = New-Object byte[] $bytes

[System.Runtime.InteropServices.Marshal]::Copy($bmpDataOrig.Scan0, $bufOrig, 0, $bytes)

for ($i = 0; $i -lt $bytes; $i += 4) {
    $b = [float]$bufOrig[$i]
    $g = [float]$bufOrig[$i+1]
    $r = [float]$bufOrig[$i+2]
    $a = 255

    # 輝度 (Luminance)
    $lum = 0.299 * $r + 0.587 * $g + 0.114 * $b

    # 目地・境界線の判定（暗いライン部分ほど値が大きくなる）
    $edge = [Math]::Max(0.0, (70.0 - $lum) / 70.0)
    # ハイライト部分（明るい紫・ハイライト部分）
    $hi = [Math]::Max(0.0, ($lum - 80.0) / 100.0)

    # ----------------------------------------------------
    # 1. シアン・エナジーストーン (Cyan Rune)
    # 石自体はクールなブルースレート、目地やひび割れにシアン発光
    # ----------------------------------------------------
    $r1 = [Math]::Min(255.0, [Math]::Max(0.0, $r * 0.45 + $edge * 30.0 + $hi * 20.0))
    $g1 = [Math]::Min(255.0, [Math]::Max(0.0, $g * 0.85 + $edge * 190.0 + $hi * 120.0))
    $b1 = [Math]::Min(255.0, [Math]::Max(0.0, $b * 1.05 + $edge * 255.0 + $hi * 200.0))
    $bufCyan[$i]   = [byte]$b1
    $bufCyan[$i+1] = [byte]$g1
    $bufCyan[$i+2] = [byte]$r1
    $bufCyan[$i+3] = 255

    # ----------------------------------------------------
    # 2. アメジスト・オブシディアン (Amethyst Obsidian)
    # 黒曜石のような漆黒＋ビビッドな紫のハイライト強調
    # ----------------------------------------------------
    $contrast = [Math]::Pow($lum / 255.0, 1.4) * 255.0
    $r2 = [Math]::Min(255.0, [Math]::Max(0.0, $r * 1.1 + $hi * 90.0))
    $g2 = [Math]::Min(255.0, [Math]::Max(0.0, $g * 0.6))
    $b2 = [Math]::Min(255.0, [Math]::Max(0.0, $b * 1.35 + $hi * 120.0))
    $bufAme[$i]   = [byte]$b2
    $bufAme[$i+1] = [byte]$g2
    $bufAme[$i+2] = [byte]$r2
    $bufAme[$i+3] = 255

    # ----------------------------------------------------
    # 3. クリムゾン・ブラッドルーン (Crimson Blood)
    # 目地や割れ目が灼熱のレッド・マゼンタに脈動発光
    # ----------------------------------------------------
    $r3 = [Math]::Min(255.0, [Math]::Max(0.0, $r * 1.25 + $edge * 240.0 + $hi * 140.0))
    $g3 = [Math]::Min(255.0, [Math]::Max(0.0, $g * 0.4 + $edge * 25.0))
    $b3 = [Math]::Min(255.0, [Math]::Max(0.0, $b * 0.5 + $edge * 60.0))
    $bufCrim[$i]   = [byte]$b3
    $bufCrim[$i+1] = [byte]$g3
    $bufCrim[$i+2] = [byte]$r3
    $bufCrim[$i+3] = 255
}

[System.Runtime.InteropServices.Marshal]::Copy($bufCyan, 0, $bmpDataCyan.Scan0, $bytes)
[System.Runtime.InteropServices.Marshal]::Copy($bufAme,  0, $bmpDataAme.Scan0,  $bytes)
[System.Runtime.InteropServices.Marshal]::Copy($bufCrim, 0, $bmpDataCrim.Scan0, $bytes)

$bmpOriginal.UnlockBits($bmpDataOrig)
$bmpCyan.UnlockBits($bmpDataCyan)
$bmpAmethyst.UnlockBits($bmpDataAme)
$bmpCrimson.UnlockBits($bmpDataCrim)

$bmpOriginal.Dispose()

# 保存
$outDir = 'Assets\texture'
$bmpCyan.Save((Join-Path $outDir 'wall_stone_cyan.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$bmpAmethyst.Save((Join-Path $outDir 'wall_stone_amethyst.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$bmpCrimson.Save((Join-Path $outDir 'wall_stone_crimson.png'), [System.Drawing.Imaging.ImageFormat]::Png)

# x64/Debug と x64/Release にもコピー
foreach ($n in @('wall_stone_cyan.png', 'wall_stone_amethyst.png', 'wall_stone_crimson.png')) {
    Copy-Item (Join-Path $outDir $n) -Destination 'x64\Debug\' -Force
    Copy-Item (Join-Path $outDir $n) -Destination 'x64\Release\Assets\texture\' -Force
}

$bmpCyan.Dispose()
$bmpAmethyst.Dispose()
$bmpCrimson.Dispose()

Write-Host "Successfully generated 3 comic stone wall variations!"
