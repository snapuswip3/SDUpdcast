@echo off
setlocal

REM ---- PATHS ----
set OUTDIR=%1
if "%OUTDIR%"=="" set OUTDIR=obj\Debug

REM Remove trailing backslash if present
if "%OUTDIR:~-1%"=="\" set OUTDIR=%OUTDIR:~0,-1%

set PROJECT_ROOT=%~dp0
set WFONT_BIN=%PROJECT_ROOT%wfont.bin

REM ---- ENSURE OUTPUT DIR ----
if not exist "%OUTDIR%" mkdir "%OUTDIR%" || exit /b 1

REM ---- CLEAN ----
del /q "%OUTDIR%\wfont.o" 2>nul

REM ---- VALIDATE INPUT ----
if not exist "%WFONT_BIN%" (
    echo ERROR: wfont.bin not found in %PROJECT_ROOT%
    exit /b 1
)

REM ---- CALL BIN2O.CMD ----
"C:\DreamSDK\opt\dreamsdk\ide\bin2o.cmd" "%PROJECT_ROOT%" "%OUTDIR%" wfont

if errorlevel 1 exit /b 1

echo wfont.o built successfully in %OUTDIR%
exit /b 0