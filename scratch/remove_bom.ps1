# remove_bom.ps1
# instanced_vs.hlsl から BOM を除去するスクリプト

$path = "c:\Users\HARU17\Desktop\GM31\DX11_Original\instanced_vs.hlsl"
if (Test-Path $path) {
    $content = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
    # BOMなしUTF-8のエンコーディングオブジェクトを作成
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($path, $content, $utf8NoBom)
    Write-Output "Successfully removed BOM from $path"
} else {
    Write-Error "File not found: $path"
}
