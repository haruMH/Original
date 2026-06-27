param (
    [string]$FilePath
)
if (Test-Path $FilePath) {
    $content = [System.IO.File]::ReadAllText($FilePath, [System.Text.Encoding]::UTF8)
    $utf8WithBom = New-Object System.Text.UTF8Encoding($true)
    [System.IO.File]::WriteAllText($FilePath, $content, $utf8WithBom)
    Write-Output "Converted $FilePath to UTF-8 with BOM"
} else {
    Write-Error "File not found: $FilePath"
}
