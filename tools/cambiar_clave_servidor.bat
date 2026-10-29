@echo off
rem Ventrik: hace una clave nueva para el servidor de la carpeta "servidor", firmada con la actual.
rem Antes, cerrar el servidor; despues, volver a abrirlo con abrir_servidor.bat. Los programas que ya lo conocian entran sin aviso.
cd /d "%~dp0"
ventrik_servidor.exe --carpeta "%~dp0servidor" --cambiar-clave
pause
