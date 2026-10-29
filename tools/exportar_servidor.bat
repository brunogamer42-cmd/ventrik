@echo off
rem Ventrik: guarda un respaldo cifrado del servidor de la carpeta "servidor" (al lado de este archivo) en servidor.vkservidor.
rem Anda con el servidor prendido. Pide dos veces una contrasena para el respaldo, que no se ve al escribir.
cd /d "%~dp0"
ventrik_servidor.exe --carpeta "%~dp0servidor" --exportar "%~dp0servidor.vkservidor"
pause
