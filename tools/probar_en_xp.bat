@echo off
rem Ventrik: corre las pruebas automaticas en esta PC (pensado para Windows XP).
rem Copiar la carpeta build\para_xp entera y hacer doble clic en este archivo.
rem El resultado tambien queda en resultado.txt, al lado de este archivo.
cd /d "%~dp0"
echo Corriendo las pruebas de Ventrik en esta PC...
echo.
ventrik_pruebas.exe > resultado.txt 2>&1
echo Probando el servidor y el programa juntos: se abre y se cierra una ventana...
set VENTRIK_SERVIDOR=%~dp0ventrik_servidor.exe
set VENTRIK_PROGRAMA=%~dp0ventrik.exe
set VENTRIK_CARPETA=%~dp0combinacion
if exist "%VENTRIK_CARPETA%" rmdir /s /q "%VENTRIK_CARPETA%"
mkdir "%VENTRIK_CARPETA%"
ventrik_pruebas.exe -tc=combinaciones* --no-skip >> resultado.txt 2>&1
type resultado.txt
echo.
echo Si las dos lineas "Status:" dicen "SUCCESS!", anda todo.
echo Si no, mandar el archivo resultado.txt.
echo.
pause
