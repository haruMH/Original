$files = @(
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\manager.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\manager.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\collision_system.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\camera.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\camera.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\gameplay_scene.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\sound_manager.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\sound_manager.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\game_constants.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\player_combat.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\player_movement.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\explosion_system.cpp",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\main.h",
    "c:\Users\HARU17\Desktop\GM31\DX11_Original\resource_manager.cpp"
)
foreach ($f in $files) {
    if (Test-Path $f) {
        $content = [System.IO.File]::ReadAllText($f, [System.Text.Encoding]::UTF8)
        $enc = New-Object System.Text.UTF8Encoding($true)
        [System.IO.File]::WriteAllText($f, $content, $enc)
        Write-Output "OK: $f"
    }
}
