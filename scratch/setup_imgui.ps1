# setup_imgui.ps1
# GitHubからImGuiをクローンし、必要なファイルをプロジェクトのimguiフォルダにコピーします。

$tempDir = "imgui_temp"
$destDir = "imgui"

# 一時ディレクトリがあれば削除
if (Test-Path $tempDir) {
    Remove-Item -Path $tempDir -Recurse -Force
}

# 宛先ディレクトリがあれば削除して再作成
if (Test-Path $destDir) {
    Remove-Item -Path $destDir -Recurse -Force
}
New-Item -ItemType Directory -Path $destDir

# クローン
Write-Output "Cloning ImGui from GitHub..."
git clone --depth 1 https://github.com/ocornut/imgui.git $tempDir

if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to clone ImGui."
    exit 1
}

# コアファイルをコピー
Write-Output "Copying core ImGui files..."
Copy-Item "$tempDir\*.h" $destDir
Copy-Item "$tempDir\*.cpp" $destDir

# バックエンドファイルをコピー
Write-Output "Copying backend files..."
Copy-Item "$tempDir\backends\imgui_impl_dx11.h" $destDir
Copy-Item "$tempDir\backends\imgui_impl_dx11.cpp" $destDir
Copy-Item "$tempDir\backends\imgui_impl_win32.h" $destDir
Copy-Item "$tempDir\backends\imgui_impl_win32.cpp" $destDir

# 一時ディレクトリを削除
Remove-Item -Path $tempDir -Recurse -Force

Write-Output "ImGui files setup successfully in $destDir/"
