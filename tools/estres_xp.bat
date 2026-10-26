@echo off
rem Ventrik: estres y ataques en esta PC (pensado para Windows XP).
rem Copiar la carpeta build\para_xp entera y hacer doble clic en este archivo.
rem El resultado queda en resultado_estres.txt, al lado de este archivo.
cd /d "%~dp0"
set VENTRIK_SERVIDOR=%~dp0ventrik_servidor.exe
set VENTRIK_CARPETA=%~dp0estres
set VENTRIK_ESTRES_CONEXIONES=40
set VENTRIK_ESTRES_CHURN=12
if exist "%VENTRIK_CARPETA%" rmdir /s /q "%VENTRIK_CARPETA%"
mkdir "%VENTRIK_CARPETA%"
echo Estresando el servidor en esta PC (puede tardar unos minutos)...
echo.
ventrik_pruebas.exe -tc=estres* --no-skip > resultado_estres.txt 2>&1
type resultado_estres.txt
echo.
echo Si la ultima linea dice "Status: SUCCESS!", aguanto todo.
echo Si no, mandar el archivo resultado_estres.txt.
echo.
pause