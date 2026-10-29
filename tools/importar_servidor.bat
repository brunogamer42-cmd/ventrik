@echo off
rem Ventrik: arma el servidor desde servidor.vkservidor (al lado de este archivo) en la carpeta "servidor", que tiene que estar vacia.
rem Pide la contrasena del respaldo, que no se ve al escribir. Despues se arranca con abrir_servidor.bat.
cd /d "%~dp0"
ventrik_servidor.exe --carpeta "%~dp0servidor" --importar "%~dp0servidor.vkservidor"
pause
