@echo off
setlocal
python "%~dp0tools\build.py" %*
if errorlevel 1 exit /b 1
