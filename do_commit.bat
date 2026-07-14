@echo off
set GIT=C:\Users\HARU17\AppData\Local\Programs\Git\cmd\git.exe
set REPO=c:\Users\HARU17\Desktop\GM31\DX11_Original
"%GIT%" -C "%REPO%" add collision_system.cpp attacking_enemy.cpp spatial_grid.h spatial_grid.cpp GM31_0421.vcxproj GM31_0421.vcxproj.filters do_build.bat do_commit.bat
"%GIT%" -C "%REPO%" commit -m "feat(phase6): SpatialGrid shared utility complete"
