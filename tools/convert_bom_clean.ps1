$enc = New-Object System.Text.UTF8Encoding($true)
$files = @(
    "Source\Core\progression_types.h",
    "Source\Core\gameobject.h",
    "Source\Core\save_system.h",
    "Source\Core\save_system.cpp",
    "Source\Core\progression_manager.h",
    "Source\Core\progression_manager.cpp",
    "Source\Stage\material_drop.h",
    "Source\Stage\material_drop.cpp",
    "Source\Scene\title_scene.cpp",
    "Source\Scene\hub_scene.h",
    "Source\Scene\hub_scene.cpp",
    "Source\Scene\tutorial_scene.cpp",
    "Source\Scene\gameplay_scene.cpp",
    "Source\Core\manager.h",
    "Source\Scene\clear_scene.h",
    "Source\Scene\clear_scene.cpp",
    "Source\Scene\gameover_scene.h",
    "Source\Scene\gameover_scene.cpp",
    "Source\UI\item_notification_hud.h",
    "Source\UI\item_notification_hud.cpp",
    "Source\Stage\collision_system.cpp",
    "Source\Core\manager.cpp",
    "Source\Graphics\renderer.h",
    "Source\Graphics\renderer.cpp",
    "Source\Core\game_constants.h",
    "Source\Stage\collision.cpp",
    "Source\UI\option_ui.h",
    "Source\UI\option_ui.cpp",
    "Source\Graphics\obj_model.h",
    "Source\Graphics\obj_model.cpp",
    "Source\Stage\prop_model.h",
    "Source\Stage\prop_model.cpp"
)
foreach ($f in $files) {
    if (Test-Path $f) {
        $text = [System.IO.File]::ReadAllText($f, [System.Text.Encoding]::UTF8)
        [System.IO.File]::WriteAllText($f, $text, $enc)
        Write-Output "Converted: $f"
    }
}
