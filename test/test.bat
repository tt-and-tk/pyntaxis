@REM 未完成

@REM @echo off

@REM setlocal

@REM for %%f in (%CD%\asm\*.asm) do (
@REM     set sv=!%%f:\asm\=\bin\!
@REM     @REM ..\asm2bin.exe %%f -o %sv:~0,-3%
@REM     echo %sv%
@REM     @REM ..\asm2bin.exe %%f -o %sv:.asm=.sav%
@REM     ..\asm2bin.exe %%f -o %sv%
@REM )

@REM endlocal

python test.py
