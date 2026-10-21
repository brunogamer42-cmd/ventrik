/*
 * SQLite 3.54 usa GetTickCount64, que existe recién desde Windows Vista, y
 * ya no trae alternativa para XP: un .exe que la use no abre en XP.
 *
 * En la versión clásica este archivo se incluye antes que sqlite3.c (/FI,
 * ver third_party/CMakeLists.txt) y cambia ese nombre por una función
 * propia (ventrik_sqlite_xp.c) que usa la de Windows si existe y, en XP,
 * GetTickCount. SQLite la usa solo para mezclar la hora en su semilla de
 * números al azar (winRandomness), que no es para cifrar.
 */
#ifndef VENTRIK_SQLITE_XP_H
#define VENTRIK_SQLITE_XP_H

unsigned __int64 __stdcall ventrik_GetTickCount64(void);
#define GetTickCount64 ventrik_GetTickCount64

#endif
