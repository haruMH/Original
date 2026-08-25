@echo off
set GIT=C:\Users\HARU17\AppData\Local\Programs\Git\cmd\git.exe
set REPO=c:\Users\HARU17\Desktop\GM31\DX11_Original
"%GIT%" -C "%REPO%" add .agents/AGENTS.md save_utf8_bom.ps1
"%GIT%" -C "%REPO%" commit -m "docs(agents): add rule to suppress thinking process outputs globally in AGENTS.md"
