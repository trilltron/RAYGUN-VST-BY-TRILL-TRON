@echo off
net session >nul 2>&1 || ( echo Run as administrator. & pause & exit /b 1 )
rmdir /S /Q "%CommonProgramFiles%\VST3\RAYGUN.vst3" 2>nul
rmdir /S /Q "%ProgramFiles%\Trill Tron\RAYGUN" 2>nul
del /Q "%ProgramFiles%\VSTPlugins\RAYGUN.dll" 2>nul
echo RAYGUN removed. Your user presets in Documents\Trill Tron\RAYGUN were left alone.
pause
