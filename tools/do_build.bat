@echo off
"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" GM31_0421.vcxproj /p:Configuration=Debug /p:Platform=x64 /t:Build /nologo /v:minimal > build_out.txt 2>&1
type build_out.txt | findstr "error C"
