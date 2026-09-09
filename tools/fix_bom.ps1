$files = Get-ChildItem -Path "c:\Users\HARU17\Desktop\GM31\DX11_Original" -Include *.h,*.cpp -Recurse | Where-Object { $_.FullName -notmatch '\\(\.vs|x64|imgui)\\' }
foreach ($file in $files) {
    $f = $file.FullName
    $content = [System.IO.File]::ReadAllText($f, [System.Text.Encoding]::UTF8)
    $enc = New-Object System.Text.UTF8Encoding($true)
    [System.IO.File]::WriteAllText($f, $content, $enc)
    Write-Output "OK: $f"
}

