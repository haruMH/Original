$vsWhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vsWhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
Write-Host "Using MSBuild at: $vsPath"
& $vsPath "GM31_0421.vcxproj" /p:Configuration=Debug /p:Platform=x64 /t:Build /m /v:minimal
