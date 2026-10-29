@echo off
rem Ventrik: abre en esta ventana el servidor de la carpeta "servidor" (al lado de este archivo). Para cerrarlo, Ctrl+C.
cd /d "%~dp0"
ventrik_servidor.exe --carpeta "%~dp0servidor"
pause
