@echo off
setlocal

REM ---- KOS ENV ----
set KOS_BASE=C:\DreamSDK\opt\toolchains\dc\kos
set KOS_ARCH=dreamcast
set KOS_PORTS=C:\DreamSDK\opt\toolchains\dc\kos-ports
set KOS_CC_BASE=C:\DreamSDK\opt\toolchains\dc\sh-elf
set KOS_CC_PREFIX=sh-elf

set KOS_CC=%KOS_CC_BASE%\bin\%KOS_CC_PREFIX%-gcc
set KOS_LIB_PATHS=-L%KOS_BASE%\lib\%KOS_ARCH% -L%KOS_BASE%\addons\lib\%KOS_ARCH% -L%KOS_PORTS%\lib

REM ---- PATHS ----
set OUTDIR=%1
if "%OUTDIR%"=="" set OUTDIR=obj\Debug

set ROMDIR=%~dp0romdisk

REM ---- ENSURE OUTPUT DIR ----
if not exist "%OUTDIR%" mkdir "%OUTDIR%" || exit /b 1

REM ---- CLEAN ----
del /q "%OUTDIR%\romdisk.img" 2>nul
del /q "%OUTDIR%\romdisk.o" 2>nul
del /q "%OUTDIR%\romdisk_tmp.c" 2>nul
del /q "%OUTDIR%\romdisk_tmp.o" 2>nul

REM ---- STEP 1: GENROMFS ----
"%KOS_BASE%\utils\genromfs\genromfs.exe" ^
 -f "%OUTDIR%\romdisk.img" ^
 -d "%ROMDIR%" ^
 -v -x .gitignore -x .DS_Store -x Thumbs.db

if errorlevel 1 exit /b 1

REM ---- STEP 2: BIN2C ----
"%KOS_BASE%\utils\bin2c\bin2c.exe" ^
 "%OUTDIR%\romdisk.img" ^
 "%OUTDIR%\romdisk_tmp.c" ^
 romdisk

if errorlevel 1 exit /b 1

REM ---- STEP 3: COMPILE ----
kos-cc -o "%OUTDIR%\romdisk_tmp.o" -c "%OUTDIR%\romdisk_tmp.c"

if errorlevel 1 exit /b 1

REM ---- STEP 4: LINK ----
%KOS_CC% -o "%OUTDIR%\romdisk.o" -r "%OUTDIR%\romdisk_tmp.o" %KOS_LIB_PATHS% -Wl,--whole-archive -lromdiskbase

if errorlevel 1 exit /b 1

REM ---- CLEAN TEMP ----
del /q "%OUTDIR%\romdisk_tmp.c" 2>nul
del /q "%OUTDIR%\romdisk_tmp.o" 2>nul

exit /b 0