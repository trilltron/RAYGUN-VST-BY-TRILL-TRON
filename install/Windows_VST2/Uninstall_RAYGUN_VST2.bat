@echo off
net session >nul 2>&1 || ( echo Run as administrator. & pause & exit /b 1 )
del /Q "%ProgramFiles%\VSTPlugins\RAYGUN.dll" 2>nul
for %%R in ("%ProgramFiles%" "%ProgramFiles(x86)%") do for /d %%D in ("%%~R\Image-Line\FL Studio*") do del /Q "%%~D\Plugins\VST\RAYGUN.dll" 2>nul
echo RAYGUN VST2 removed. Your user presets were left alone.
pause
