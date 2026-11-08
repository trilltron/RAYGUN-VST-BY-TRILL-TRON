@echo off
setlocal EnableDelayedExpansion
title RAYGUN (VST2) by TRILL TRON - setup
net session >nul 2>&1
if errorlevel 1 (
  echo.
  echo  Please RIGHT-CLICK this file and choose "Run as administrator".
  echo.
  pause
  exit /b 1
)
set "SRC=%~dp0RAYGUN.dll"
if not exist "%SRC%" ( echo  RAYGUN.dll not found next to this installer. & pause & exit /b 1 )

set COUNT=0
rem 1) the standard VST plugin folder
if not exist "%ProgramFiles%\VSTPlugins" mkdir "%ProgramFiles%\VSTPlugins"
copy /Y "%SRC%" "%ProgramFiles%\VSTPlugins\RAYGUN.dll" >nul && ( echo  Installed: %ProgramFiles%\VSTPlugins\RAYGUN.dll & set /a COUNT+=1 )

rem 2) FL Studio's own VST folder(s), so FL finds it with no extra settings
for %%R in ("%ProgramFiles%" "%ProgramFiles(x86)%") do (
  for /d %%D in ("%%~R\Image-Line\FL Studio*") do (
    if exist "%%~D\Plugins\VST" (
      copy /Y "%SRC%" "%%~D\Plugins\VST\RAYGUN.dll" >nul && ( echo  Installed: %%~D\Plugins\VST\RAYGUN.dll & set /a COUNT+=1 )
    )
  )
)
if not exist "%USERPROFILE%\Documents\Trill Tron\RAYGUN\User Presets" mkdir "%USERPROFILE%\Documents\Trill Tron\RAYGUN\User Presets"

echo.
echo  DONE - copied to !COUNT! location^(s^). The 595 factory presets are built in.
echo.
echo  NEXT, IN FL STUDIO:
echo    1. Options ^> Manage plugins
echo    2. Click "Find installed plugins" ^(Start scan^)
echo    3. RAYGUN appears in the list - click it, then add it from Channel rack ^> + ^> RAYGUN
echo.
echo  If it does not appear: Options ^> File settings ^> add  C:\Program Files\VSTPlugins  to the plugin search paths, then scan again.
echo.
pause
