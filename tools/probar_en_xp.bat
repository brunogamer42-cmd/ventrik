@echo off
rem Ventrik: corre las pruebas automaticas en esta PC (pensado para Windows XP).
rem Copiar la carpeta build\para_xp entera y hacer doble clic en este archivo.
rem El resultado tambien queda en resultado.txt, al lado de este archivo.
cd /d "%~dp0"
echo Corriendo las pruebas de Ventrik en esta PC...
echo.
ventrik_pruebas.exe > resultado.txt 2>&1
type resultado.txt
echo.
echo Si la ultima linea dice "Status: SUCCESS!", anda todo.
echo Si no, mandar el archivo resultado.txt.
echo.
pause
