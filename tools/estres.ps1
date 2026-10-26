# Ventrik: estrés largo.
#
# Levanta el servidor de verdad (moderno, clásico y el de AddressSanitizer) y
# lo ataca más fuerte y por más tiempo que check.ps1: muchas conexiones a la
# vez, inundación, basura y entrar/salir, con un cliente "canario" que mide si
# los demás siguen atendidos. Se corre en segundo plano cuando un paso toca la
# red, como el fuzzing largo, nunca durante una prueba a mano (usa mucho).
#
# Uso:  powershell -NoProfile -ExecutionPolicy Bypass -File tools\estres.ps1 [-Conexiones 400] [-Churn 1000]
#       Antes hay que compilar con tools\check.ps1.

param([int]$Conexiones = 400, [int]$Churn = 1000)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$raiz = Split-Path -Parent $PSScriptRoot
$registros = Join-Path $raiz 'build\registros'
New-Item -ItemType Directory -Force -Path $registros | Out-Null
$env:VENTRIK_ESTRES_CONEXIONES = "$Conexiones"
$env:VENTRIK_ESTRES_CHURN = "$Churn"
$fallas = 0

$servidores = @(
    @{ nombre = 'moderno';            servidor = 'moderna\Release\ventrik_servidor.exe'; pruebas = 'moderna\Release\ventrik_pruebas.exe'; asan = $false },
    @{ nombre = 'clásico';            servidor = 'clasica\Release\ventrik_servidor.exe'; pruebas = 'clasica\Release\ventrik_pruebas.exe'; asan = $false },
    @{ nombre = 'AddressSanitizer';   servidor = 'asan64\Release\ventrik_servidor.exe';  pruebas = 'moderna\Release\ventrik_pruebas.exe'; asan = $true }
)

foreach ($caso in $servidores) {
    $servidor = Join-Path $raiz "build\$($caso.servidor)"
    $pruebas = Join-Path $raiz "build\$($caso.pruebas)"
    if (-not (Test-Path $servidor) -or -not (Test-Path $pruebas)) {
        Write-Host ("  SALTeado  servidor $($caso.nombre) (falta compilar con check.ps1)") -ForegroundColor Yellow
        continue
    }
    $carpeta = Join-Path $raiz "build\estres\largo-$($caso.nombre)"
    if (Test-Path $carpeta) { Remove-Item -Recurse -Force $carpeta }
    New-Item -ItemType Directory -Force -Path $carpeta | Out-Null
    $env:VENTRIK_SERVIDOR = $servidor
    $env:VENTRIK_CARPETA = $carpeta
    $env:ASAN_OPTIONS = if ($caso.asan) { 'abort_on_error=0' } else { '' }
    $registro = Join-Path $registros "estres-largo-$($caso.nombre).txt"
    Write-Host ("Servidor $($caso.nombre): $Conexiones conexiones, $Churn idas y vueltas")
    & $pruebas '-tc=estres*' '--no-skip' *> $registro
    $ok = $LASTEXITCODE -eq 0
    Get-Content $registro | Where-Object { $_ -match 'MESSAGE: ' } |
        ForEach-Object { Write-Host ('    ' + ($_ -replace '.*MESSAGE: ', '')) }
    if ($ok) {
        Write-Host ("  OK     servidor $($caso.nombre)") -ForegroundColor Green
    } else {
        $fallas++
        Write-Host ("  FALLÓ  servidor $($caso.nombre). Registro: $registro") -ForegroundColor Red
        Get-Content $registro -Tail 20 | ForEach-Object { Write-Host ('         | ' + $_) }
    }
}
$env:ASAN_OPTIONS = ''

Write-Host ''
if ($fallas -eq 0) {
    Write-Host 'ESTRÉS OK' -ForegroundColor Green
    exit 0
}
Write-Host ("$fallas con fallas") -ForegroundColor Red
exit 1
