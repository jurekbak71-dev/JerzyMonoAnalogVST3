@echo off
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERROR] CMake nie jest dostepny w PATH.
  exit /b 1
)

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1

cmake --build build --config Release --parallel
if errorlevel 1 exit /b 1

echo.
echo =============================================
echo BUILD GOTOWY - szukam Jerzy Mono Analog.vst3
for /d /r "build" %%F in ("Jerzy Mono Analog.vst3") do echo %%F
echo =============================================
endlocal
