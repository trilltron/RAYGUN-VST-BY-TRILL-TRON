@echo off
setlocal
title RAYGUN by TRILL TRON - installer
net session >nul 2>&1
if errorlevel 1 (
  echo.
  echo  Please RIGHT-CLICK this file and choose "Run as administrator".
  echo  RAYGUN installs into C:\Program Files\Common Files\VST3 so every DAW can see it.
  echo.
  pause
  exit /b 1
)
set "SRC=%~dp0"
set "DEST=%CommonProgramFiles%\VST3"
if not exist "%SRC%RAYGUN.vst3" (
  echo  RAYGUN.vst3 not found next to this installer.
  pause
  exit /b 1
)
echo  Installing RAYGUN.vst3 to "%DEST%" ...
if exist "%DEST%\RAYGUN.vst3" rmdir /S /Q "%DEST%\RAYGUN.vst3"
xcopy /E /I /Y /Q "%SRC%RAYGUN.vst3" "%DEST%\RAYGUN.vst3" >nul
if errorlevel 1 ( echo  Copy failed. & pause & exit /b 1 )
if not exist "%USERPROFILE%\Documents\Trill Tron\RAYGUN\User Presets" mkdir "%USERPROFILE%\Documents\Trill Tron\RAYGUN\User Presets"
if exist "%SRC%RAYGUN_VST2.dll" (
  if not exist "%ProgramFiles%\VSTPlugins" mkdir "%ProgramFiles%\VSTPlugins"
  copy /Y "%SRC%RAYGUN_VST2.dll" "%ProgramFiles%\VSTPlugins\RAYGUN.dll" >nul
  echo  VST2 version installed to "%ProgramFiles%\VSTPlugins\RAYGUN.dll"  ^(for FL Studio 20.0-20.6 and other older hosts^).
)
if exist "%SRC%RAYGUN.exe" (
  if not exist "%ProgramFiles%\Trill Tron\RAYGUN" mkdir "%ProgramFiles%\Trill Tron\RAYGUN"
  copy /Y "%SRC%RAYGUN.exe" "%ProgramFiles%\Trill Tron\RAYGUN\RAYGUN.exe" >nul
  echo  Standalone installed to "%ProgramFiles%\Trill Tron\RAYGUN".
)
echo.
echo  DONE. The 595 factory presets are built into the plugin - nothing else to copy.
echo.
echo  FL STUDIO:  Options ^> Manage plugins ^> click "Find installed plugins"  (or Start scan)
echo              then add  RAYGUN  from the Instruments list (Add ^> More plugins / Installed ^> Generators).
echo  Full steps are in README_FL_STUDIO.txt
echo.
pause
