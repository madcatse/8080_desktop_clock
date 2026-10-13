@echo off
setlocal
python "%~dp0tools\verify.py" %*
if errorlevel 1 exit /b 1
