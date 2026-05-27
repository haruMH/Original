# save_utf8_bom.ps1
# すべての新規追加ファイルをBOM付きUTF-8に変換して、MSVCのコンパイルエラー（C4819、C2065等）を解消します。

$files = @(
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\collision.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\collision.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\camera.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\camera.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\math_helper.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\player_controller.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\player_controller.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\resource_manager.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\resource_manager.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\manager.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\manager.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\player.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\player.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\enemy.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\enemy.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\game_rule.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\game_rule.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\collision_system.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\collision_system.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\explosion_system.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\explosion_system.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\render_component.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\render_system.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\render_system.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\renderer.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\renderer.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\item.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\item.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\input.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\input.cpp"
)

foreach ($file in $files) {
    if (Test-Path $file) {
        $content = [System.IO.File]::ReadAllText($file, [System.Text.Encoding]::UTF8)
        # 明示的にBOM付きUTF-8のエンコーディングオブジェクトを作成
        $utf8WithBom = New-Object System.Text.UTF8Encoding($true)
        [System.IO.File]::WriteAllText($file, $content, $utf8WithBom)
        Write-Output "Saved $file with UTF-8 BOM"
    }
}
