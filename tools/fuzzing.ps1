# Ventrik: fuzzing.
#
# Corre los fuzzers (compilados con AddressSanitizer por check.ps1) durante
# un tiempo y avisa si alguno encontró una falla. Lo aprendido en la corrida larga
# queda en build\fuzzing\corpus para la próxima vez, y cada falla en build\fuzzing\fallas.
#
# Uso:  powershell -NoProfile -ExecutionPolicy Bypass -File tools\fuzzing.ps1 [-Segundos 600]
#       Antes hay que compilarlos: tools\check.ps1 lo hace.

param([int]$Segundos = 600)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$raiz = Split-Path -Parent $PSScriptRoot
$carpeta = Join-Path $raiz 'build\fuzzing'
$fallas = Join-Path $carpeta 'fallas'
New-Item -ItemType Directory -Force -Path $fallas | Out-Null
$huboFallas = $false

function Informar([string]$nombre, [bool]$ok, [string]$detalle = '') {
    if ($ok) {
        Write-Host ('  OK     ' + $nombre) -ForegroundColor Green
    } else {
        $script:huboFallas = $true
        Write-Host ('  FALLÓ  ' + $nombre) -ForegroundColor Red
        if ($detalle) { Write-Host ('         ' + $detalle) -ForegroundColor Red }
    }
}

# Start-Process no pone comillas, y la carpeta del proyecto puede tener espacios.
function Entre-Comillas([string]$texto) { return '"' + $texto + '"' }

$corridas = @()
foreach ($nombre in 'fuzz_base', 'fuzz_guardados', 'fuzz_identidad', 'fuzz_nombres', 'fuzz_protocolo', 'fuzz_sala', 'fuzz_textos', 'fuzz_utf', 'fuzz_voz', 'fuzz_voz_servidor') {
    $semillas = Join-Path $raiz ('fuzz\semillas\' + $nombre.Substring(5))
    foreach ($bits in '64', '32') {
        $otros = if ($bits -eq '64') { '32' } else { '64' }
        $exe = Join-Path $raiz "build\fuzzing$bits\Release\$nombre.exe"
        $titulo = "$nombre ($bits bits)"
        if (-not (Test-Path $exe)) {
            Informar $titulo $false "No está compilado: $exe (correr tools\check.ps1)"
            continue
        }
        # La corrida larga usa el corpus guardado (el suyo y el del otro). La corta arranca de las
        # semillas en una carpeta vacía: el corpus guardado crece y cargarlo no entra en check.ps1.
        $corpus = Join-Path $carpeta "corpus\$nombre$bits"
        $corpusOtro = Join-Path $carpeta "corpus\$nombre$otros"
        if ($Segundos -lt 300) {
            $corpus = Join-Path $carpeta "corto\$nombre$bits"
            if (Test-Path $corpus) { Remove-Item -Recurse -Force $corpus }
        }
        New-Item -ItemType Directory -Force -Path $corpus, $corpusOtro | Out-Null
        $registro = Join-Path $carpeta "$nombre$bits.txt"
        # Tiene que caber lo más grande que se acepta: un mensaje de 64 KiB o una clave de 16 KiB.
        $largo = switch ($nombre) { 'fuzz_protocolo' { 70000 } 'fuzz_base' { 65536 } 'fuzz_identidad' { 17000 } default { 4096 } }
        $argumentos = @("-max_total_time=$Segundos", "-max_len=$largo", '-timeout=10', '-rss_limit_mb=1536', '-print_final_stats=1',
                        (Entre-Comillas "-artifact_prefix=$fallas\$nombre$bits-"),
                        (Entre-Comillas $corpus), (Entre-Comillas $semillas))
        if ($Segundos -ge 300) { $argumentos += Entre-Comillas $corpusOtro }
        $antes = @(Get-ChildItem $fallas -Filter "$nombre$bits-*" | ForEach-Object { $_.Name })
        $proceso = Start-Process -FilePath $exe -ArgumentList $argumentos -NoNewWindow -PassThru `
            -RedirectStandardError $registro -RedirectStandardOutput "$registro.salida"
        $null = $proceso.Handle  # sin esto, PowerShell 5.1 pierde el código de salida
        $corridas += [pscustomobject]@{ Titulo = $titulo; Proceso = $proceso; Registro = $registro
                                        Prefijo = "$nombre$bits-"; Antes = $antes }
    }
}

Write-Host ("Fuzzing: {0} fuzzers durante {1} segundos, a la vez" -f $corridas.Count, $Segundos)
$limite = (Get-Date).AddSeconds($Segundos + 120)
foreach ($corrida in $corridas) {
    $restante = [math]::Max(1, ($limite - (Get-Date)).TotalMilliseconds)
    if (-not $corrida.Proceso.WaitForExit([int]$restante)) {
        $corrida.Proceso.Kill()
        Informar $corrida.Titulo $false "No terminó a tiempo. Registro: $($corrida.Registro)"
        continue
    }
    $nuevas = @(Get-ChildItem $fallas -Filter "$($corrida.Prefijo)*" |
        Where-Object { $corrida.Antes -notcontains $_.Name })
    $texto = Get-Content $corrida.Registro
    $casos = $texto | Select-String 'stat::number_of_executed_units:\s+(\d+)' | Select-Object -Last 1
    $casos = if ($casos) { '{0:N0} casos' -f [long]$casos.Matches[0].Groups[1].Value } else { '' }
    if ($corrida.Proceso.ExitCode -eq 0 -and $nuevas.Count -eq 0) {
        Informar ("{0}: {1}" -f $corrida.Titulo, $casos) $true
    } else {
        Informar $corrida.Titulo $false ("Código {0}. Registro: {1}" -f $corrida.Proceso.ExitCode, $corrida.Registro)
        $texto | Select-String 'ERROR|SUMMARY|ALARM|deadly signal|out-of-memory|==ABORTING' |
            Select-Object -First 8 | ForEach-Object { Write-Host ('         | ' + $_.Line) }
        foreach ($falla in $nuevas) { Write-Host ('         Caso que falla: ' + $falla.FullName) -ForegroundColor Red }
    }
}

if ($huboFallas) { exit 1 }
exit 0
