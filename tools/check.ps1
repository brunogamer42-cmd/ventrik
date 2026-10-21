# Ventrik: comprobación completa.
#
# Compila las dos versiones (moderna y clásica), corre las pruebas en las
# dos y revisa que cada .exe tenga lo que pide el diseño. Muestra OK o
# FALLÓ en cada punto y, si algo falla, termina con código de error 1.
#
# Uso:  powershell -NoProfile -ExecutionPolicy Bypass -File tools\check.ps1

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$raiz = Split-Path -Parent $PSScriptRoot
$registros = Join-Path $raiz 'build\registros'
New-Item -ItemType Directory -Force -Path $registros | Out-Null
$script:fallas = 0

function Informar([string]$nombre, [bool]$ok, [string]$detalle = '') {
    if ($ok) {
        Write-Host ('  OK     ' + $nombre) -ForegroundColor Green
    } else {
        $script:fallas++
        Write-Host ('  FALLÓ  ' + $nombre) -ForegroundColor Red
        if ($detalle) { Write-Host ('         ' + $detalle) -ForegroundColor Red }
    }
}

# Corre un programa, guarda todo lo que muestra en un registro y devuelve
# su código de salida. Si falla, muestra las últimas líneas del registro.
function Ejecutar([string]$programa, [string[]]$argumentos, [string]$registro) {
    $anterior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $programa @argumentos 2>&1 | ForEach-Object { "$_" } | Out-File -FilePath $registro -Encoding utf8
        $codigo = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $anterior
    }
    if ($codigo -ne 0) {
        Get-Content $registro -Tail 20 | ForEach-Object { Write-Host ('         | ' + $_) }
        Write-Host ('         Registro completo: ' + $registro)
    }
    return $codigo
}

function Texto-Dumpbin([string[]]$argumentos) {
    return (& $script:dumpbin /nologo @argumentos) -join "`n"
}

# Funciones de Windows que no existen en XP y que suelen colarse sin querer.
$soloDesdeVista = @(
    'GetTickCount64', 'InitializeSRWLock', 'AcquireSRWLockExclusive',
    'AcquireSRWLockShared', 'ReleaseSRWLockExclusive', 'ReleaseSRWLockShared',
    'TryAcquireSRWLockExclusive', 'TryAcquireSRWLockShared',
    'InitializeConditionVariable', 'SleepConditionVariableCS',
    'SleepConditionVariableSRW', 'WakeConditionVariable',
    'WakeAllConditionVariable', 'InitOnceExecuteOnce', 'InitOnceBeginInitialize',
    'InitOnceComplete', 'FlsAlloc', 'FlsFree', 'FlsGetValue', 'FlsSetValue',
    'InitializeCriticalSectionEx', 'CreateEventExW', 'CreateMutexExW',
    'CreateSemaphoreExW', 'CreateWaitableTimerExW', 'GetSystemTimePreciseAsFileTime',
    'LCMapStringEx', 'CompareStringEx', 'GetLocaleInfoEx', 'GetUserDefaultLocaleName',
    'LocaleNameToLCID', 'LCIDToLocaleName', 'GetCurrentProcessorNumber',
    'GetFileInformationByHandleEx', 'SetFileInformationByHandle',
    'GetFinalPathNameByHandleW', 'CreateSymbolicLinkW', 'QueryFullProcessImageNameW',
    'CancelIoEx', 'GetErrorMode', 'GetProductInfo', 'GetDynamicTimeZoneInformation',
    'InitializeProcThreadAttributeList', 'UpdateProcThreadAttribute',
    'CreateThreadpoolWork', 'SubmitThreadpoolWork', 'CreateThreadpoolTimer',
    'SetThreadpoolTimer', 'RaiseFailFastException', 'GetEnabledXStateFeatures',
    'SetDefaultDllDirectories', 'AddDllDirectory', 'BCryptGenRandom', 'WSAPoll', 'inet_pton', 'inet_ntop', 'if_nametoindex'
)

Push-Location $raiz
try {
    Write-Host 'Herramientas'
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs = $null
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -products * -version '[17.0,18.0)' `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.WinXP `
            -property installationPath | Select-Object -First 1
    }
    Informar 'Visual Studio 2022 con C++ y soporte para XP' ([bool]$vs) `
        'Falta Visual Studio 2022 con "Desarrollo para el escritorio con C++" y la compatibilidad con Windows XP (v141)'
    if (-not $vs) { exit 1 }

    $cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (-not (Test-Path $cmake)) {
        $encontrado = Get-Command cmake.exe -ErrorAction SilentlyContinue
        $cmake = if ($encontrado) { $encontrado.Source } else { $null }
    }
    Informar 'CMake' ([bool]$cmake) 'No se encontró CMake (viene con "Herramientas de CMake de C++ para Windows")'
    if (-not $cmake) { exit 1 }
    $ctest = Join-Path (Split-Path $cmake) 'ctest.exe'

    $msvc = Get-ChildItem (Join-Path $vs 'VC\Tools\MSVC') -Directory |
        Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
    $script:dumpbin = Join-Path $msvc.FullName 'bin\Hostx64\x64\dumpbin.exe'
    Informar 'dumpbin (para revisar los .exe)' (Test-Path $script:dumpbin) $script:dumpbin
    if (-not (Test-Path $script:dumpbin)) { exit 1 }

    foreach ($version in 'moderna', 'clasica') {
        Write-Host ''
        Write-Host ('Versión ' + $version)

        $r = Ejecutar $cmake @('--preset', $version, "-DCMAKE_GENERATOR_INSTANCE=$vs") `
            (Join-Path $registros "$version-configurar.txt")
        Informar 'Configurar' ($r -eq 0)
        if ($r -ne 0) { continue }

        $r = Ejecutar $cmake @('--build', '--preset', $version) (Join-Path $registros "$version-compilar.txt")
        Informar 'Compilar' ($r -eq 0)
        if ($r -ne 0) { continue }

        $r = Ejecutar $ctest @('--preset', $version) (Join-Path $registros "$version-pruebas.txt")
        Informar 'Pruebas automáticas' ($r -eq 0)

        $exe = Join-Path $raiz "build\$version\Release\ventrik.exe"
        if (-not (Test-Path $exe)) {
            Informar 'Existe ventrik.exe' $false $exe
            continue
        }

        $cabeceras = Texto-Dumpbin @('/headers', $exe)
        $dependencias = Texto-Dumpbin @('/dependents', $exe)
        $config = Texto-Dumpbin @('/loadconfig', $exe)

        Informar 'Un solo .exe, sin DLL de Visual C++ (/MT)' `
            ($dependencias -notmatch '(?i)vcruntime|msvcp|ucrtbase|api-ms-win-crt')
        Informar 'Protección DEP (NX compatible)' ($cabeceras -match 'NX compatible')
        Informar 'Direcciones al azar (Dynamic base)' ($cabeceras -match 'Dynamic base')
        Informar 'Control Flow Guard' ($cabeceras -match 'Control Flow Guard')

        if ($version -eq 'moderna') {
            Informar 'Es de 64 bits' ($cabeceras -match 'machine \(x64\)')
            Informar 'Direcciones al azar ampliadas (High Entropy)' ($cabeceras -match 'High Entropy Virtual Addresses')
        } else {
            Informar 'Es de 32 bits' ($cabeceras -match 'machine \(x86\)')
            Informar 'Marcado para Windows XP (5.01)' ($cabeceras -match '5\.01 subsystem version')
            $seh = [regex]::Match($config, '(\d+) Safe Exception Handler Count')
            Informar 'SafeSEH' ($seh.Success -and [int]$seh.Groups[1].Value -gt 0)
            Informar 'Sin DLL que XP no tiene' ($dependencias -notmatch '(?i)bcrypt\.dll|api-ms-win-')
            $importadas = (Texto-Dumpbin @('/imports', $exe)) -split "`n" |
                ForEach-Object { if ($_ -match '^\s+[0-9A-F]+\s+(\w+)\s*$') { $Matches[1] } }
            $faltan = @($importadas | Where-Object { $soloDesdeVista -contains $_ } | Sort-Object -Unique)
            Informar 'Sin funciones de Windows que XP no tiene' ($faltan.Count -eq 0) `
                ('Usa: ' + ($faltan -join ', '))
        }

        # Abre el programa, espera su ventana y la cierra como lo haría la X.
        $proceso = Start-Process -FilePath $exe -PassThru
        $null = $proceso.Handle   # para poder leer el código de salida después
        try {
            $titulo = ''
            $limite = (Get-Date).AddSeconds(15)
            while ((Get-Date) -lt $limite -and -not $proceso.HasExited) {
                $proceso.Refresh()
                if ($proceso.MainWindowHandle -ne [IntPtr]::Zero) {
                    $titulo = $proceso.MainWindowTitle
                    break
                }
                Start-Sleep -Milliseconds 200
            }
            Informar 'Abre la ventana "Ventrik"' ($titulo -eq 'Ventrik') ('Título: "' + $titulo + '"')
            $cerro = $false
            if (-not $proceso.HasExited) {
                $null = $proceso.CloseMainWindow()
                $cerro = $proceso.WaitForExit(10000)
            }
            Informar 'Se cierra bien' ($cerro -and $proceso.ExitCode -eq 0)
        } finally {
            if (-not $proceso.HasExited) { $proceso.Kill() }
        }
    }
} finally {
    Pop-Location
}

Write-Host ''
if ($script:fallas -eq 0) {
    Write-Host 'TODO OK' -ForegroundColor Green
    exit 0
}
$plural = if ($script:fallas -eq 1) { 'FALLA' } else { 'FALLAS' }
Write-Host ("$script:fallas $plural") -ForegroundColor Red
exit 1
