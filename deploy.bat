@echo off
REM ========================================
REM TouchDataViewer 打包脚本
REM ========================================

echo 正在创建发布目录...
if not exist "release_package" mkdir release_package
cd release_package
if exist "*" del /Q *

echo.
echo 正在复制可执行文件...
copy ..\build\Desktop_Qt_6_9_0_MSVC2022_64bit-Release\RGD_FAE.exe . /Y

echo.
echo 正在运行 windeployqt...
REM 使用 Qt 的 windeployqt 工具部署必要的 DLL
REM 请根据您的 Qt 安装路径修改下面的路径
"D:\tools\Qt\software\6.9.0\msvc2022_64\bin\windeployqt.exe" ^
    --release ^
    --no-translations ^
    --no-system-d3d-compiler ^
    --no-opengl-sw ^
    --no-compiler-runtime ^
    RGD_FAE.exe

echo.
echo 正在清理不需要的文件...
REM 删除不需要的图像格式插件（保留常用的）
if exist "imageformats\qgif.dll" del imageformats\qgif.dll
if exist "imageformats\qicns.dll" del imageformats\qicns.dll
if exist "imageformats\qtga.dll" del imageformats\qtga.dll
if exist "imageformats\qtiff.dll" del imageformats\qtiff.dll
if exist "imageformats\qwbmp.dll" del imageformats\qwbmp.dll
if exist "imageformats\qwebp.dll" del imageformats\qwebp.dll

echo.
echo 正在复制资源文件...
REM 如果有额外的资源文件，在这里复制
if exist "..\resources\icons\app_icon.ico" copy "..\resources\icons\app_icon.ico" . /Y

echo.
echo ========================================
echo 打包完成！
echo 发布文件位于: release_package 目录
echo.
echo 预估体积:
dir /s
echo ========================================

pause
