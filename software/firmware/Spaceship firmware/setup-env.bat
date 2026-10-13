@echo off
setlocal
python "%~dp0tools\setup.py" %*
if errorlevel 1 exit /b 1
