@echo off
REM 临时将 OpenCV DLL 目录添加到 PATH，并运行 gui_app.exe（不会永久修改系统 PATH）
setlocal

set "OPENCV_BIN=C:\Program Files\Opencv\opencv\build\x64\vc16\bin"
set "PATH=%OPENCV_BIN%;%PATH%"

pushd "%~dp0"
if exist "build\bin\Debug\gui_app.exe" (
  build\bin\Debug\gui_app.exe %*
) else (
  echo 找不到 build\bin\Debug\gui_app.exe，请先编译项目或调整路径。
  pause
)
popd

endlocal