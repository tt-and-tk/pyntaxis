@REM 未完成

@REM @echo off

@REM setlocal

@REM for %%f in (%CD%\asm\*.pt) do (
@REM     set sv=!%%f:\asm\=\sv\!
@REM     @REM ..\asm2sv.exe %%f -o %sv:~0,-3%
@REM     echo %sv%
@REM     @REM ..\asm2sv.exe %%f -o %sv:.pt=.sav%
@REM     ..\asm2sv.exe %%f -o %sv%
@REM )

@REM endlocal

python test.py
