# Ventrik: huellas de las bibliotecas de third_party/.
#
# Sin opciones, compara cada archivo de las bibliotecas con
# third_party/HUELLAS.txt y termina con error si alguno cambió, falta o
# sobra: así nadie modifica código ajeno sin que se note.
#
# Con -Actualizar vuelve a escribir HUELLAS.txt. Se usa solo al agregar o
# actualizar una biblioteca, junto con third_party/VERSIONS.md.
#
# Uso:  powershell -NoProfile -ExecutionPolicy Bypass -File tools\huellas_terceros.ps1 [-Actualizar]

param([switch]$Actualizar)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$raiz = Split-Path -Parent $PSScriptRoot
$terceros = Join-Path $raiz 'third_party'
$registro = Join-Path $terceros 'HUELLAS.txt'

# Lo que es nuestro dentro de third_party/ no entra.
$propios = @('CMakeLists.txt', 'VERSIONS.md', 'HUELLAS.txt')

$actuales = @{}
Get-ChildItem $terceros -Recurse -File | ForEach-Object {
    $ruta = $_.FullName.Substring($terceros.Length + 1).Replace('\', '/')
    if ($propios -notcontains $ruta -and -not $ruta.StartsWith('integracion/')) {
        $actuales[$ruta] = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

if ($Actualizar) {
    $lineas = @(
        '# Huella SHA-256 de cada archivo de las bibliotecas de third_party/.'
        '# La revisa tools\check.ps1. Se regenera con tools\huellas_terceros.ps1 -Actualizar'
        '# solo al agregar o actualizar una biblioteca (ver VERSIONS.md).'
    )
    $lineas += $actuales.Keys | Sort-Object | ForEach-Object { $actuales[$_] + '  ' + $_ }
    [IO.File]::WriteAllLines($registro, [string[]]$lineas, (New-Object Text.UTF8Encoding $false))
    Write-Host ("HUELLAS.txt actualizado: {0} archivos" -f $actuales.Count)
    exit 0
}

if (-not (Test-Path $registro)) {
    Write-Host '         Falta third_party/HUELLAS.txt'
    exit 1
}
$guardadas = @{}
foreach ($linea in [IO.File]::ReadAllLines($registro)) {
    if ($linea -match '^([0-9a-f]{64})  (.+)$') { $guardadas[$Matches[2]] = $Matches[1] }
}

$problemas = @()
foreach ($ruta in $guardadas.Keys) {
    if (-not $actuales.ContainsKey($ruta)) { $problemas += "falta:   $ruta" }
    elseif ($actuales[$ruta] -ne $guardadas[$ruta]) { $problemas += "cambió:  $ruta" }
}
foreach ($ruta in $actuales.Keys) {
    if (-not $guardadas.ContainsKey($ruta)) { $problemas += "sobra:   $ruta" }
}

if ($problemas.Count -gt 0) {
    $problemas | Sort-Object | Select-Object -First 20 | ForEach-Object { Write-Host ('         ' + $_) }
    if ($problemas.Count -gt 20) { Write-Host ('         ... y {0} más' -f ($problemas.Count - 20)) }
    exit 1
}
exit 0
