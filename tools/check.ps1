# Ventrik: comprobación completa.
#
# Compila las dos versiones (moderna y clásica), corre las pruebas en las
# dos y revisa que cada .exe tenga lo que pide el diseño. Muestra OK o
# FALLÓ en cada punto y, si algo falla, termina con código de error 1.
# Al final deja en build\para_xp lo que hay que copiar a una PC con XP.
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

# DLL de Windows que existen en XP SP3. La versión clásica no puede pedir
# ninguna otra: si lo hace, no abre en XP.
$dllDeXP = @(
    'kernel32.dll', 'user32.dll', 'gdi32.dll', 'advapi32.dll', 'shell32.dll',
    'ole32.dll', 'oleaut32.dll', 'comctl32.dll', 'comdlg32.dll', 'shlwapi.dll',
    'ws2_32.dll', 'wsock32.dll', 'mswsock.dll', 'iphlpapi.dll', 'winmm.dll',
    'dsound.dll', 'd3d9.dll', 'imm32.dll', 'version.dll', 'crypt32.dll',
    'secur32.dll', 'rpcrt4.dll', 'setupapi.dll', 'winspool.drv', 'msimg32.dll',
    'uxtheme.dll', 'userenv.dll', 'psapi.dll', 'netapi32.dll', 'winhttp.dll',
    'wininet.dll', 'usp10.dll'
)

# Funciones de Windows que no existen en XP de 32 bits y que suelen colarse.
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
    'SetDefaultDllDirectories', 'AddDllDirectory', 'SetThreadStackGuarantee',
    'GetLogicalProcessorInformationEx', 'GetActiveProcessorCount',
    'QueryUnbiasedInterruptTime', 'K32GetProcessMemoryInfo', 'K32EnumProcessModules',
    'SetThreadDescription', 'GetThreadDescription', 'WaitOnAddress',
    'WakeByAddressSingle', 'CreateFile2', 'GetOverlappedResultEx',
    'BCryptGenRandom', 'WSAPoll', 'inet_pton', 'inet_ntop', 'if_nametoindex',
    'InetPtonA', 'InetPtonW', 'InetNtopA', 'InetNtopW'
)

# Revisa las protecciones de un .exe y, en la clásica, que abra en XP.
function Revisar-Exe([string]$exe, [string]$version) {
    $nombre = Split-Path $exe -Leaf
    if (-not (Test-Path $exe)) {
        Informar "Existe $nombre" $false $exe
        return
    }
    $cabeceras = Texto-Dumpbin @('/headers', $exe)
    $dependencias = Texto-Dumpbin @('/dependents', $exe)

    Informar "${nombre}: un solo .exe, sin DLL de Visual C++ (/MT)" `
        ($dependencias -notmatch '(?i)vcruntime|msvcp|ucrtbase|api-ms-win-crt')
    Informar "${nombre}: protección DEP (NX compatible)" ($cabeceras -match 'NX compatible')
    Informar "${nombre}: direcciones al azar (Dynamic base)" ($cabeceras -match 'Dynamic base')
    Informar "${nombre}: Control Flow Guard" ($cabeceras -match 'Control Flow Guard')

    if ($version -eq 'moderna') {
        Informar "${nombre}: es de 64 bits" ($cabeceras -match 'machine \(x64\)')
        Informar "${nombre}: direcciones al azar ampliadas (High Entropy)" `
            ($cabeceras -match 'High Entropy Virtual Addresses')
        Informar "${nombre}: DLL solo desde la carpeta de sistema" `
            ((Texto-Dumpbin @('/loadconfig', $exe)) -match '0800 Dependent Load Flag')
        return
    }

    Informar "${nombre}: es de 32 bits" ($cabeceras -match 'machine \(x86\)')
    Informar "${nombre}: marcado para Windows XP (5.01)" ($cabeceras -match '5\.01 subsystem version')
    # dumpbin muestra la cantidad en hexadecimal.
    $seh = [regex]::Match((Texto-Dumpbin @('/loadconfig', $exe)), '([0-9A-Fa-f]+) Safe Exception Handler Count')
    Informar "${nombre}: SafeSEH" ($seh.Success -and [Convert]::ToInt32($seh.Groups[1].Value, 16) -gt 0)

    $dlls = [regex]::Matches($dependencias, '(?im)^\s+(\S+\.(dll|drv))\s*$') |
        ForEach-Object { $_.Groups[1].Value.ToLowerInvariant() }
    $dllRaras = @($dlls | Where-Object { $dllDeXP -notcontains $_ })
    Informar "${nombre}: solo usa DLL que XP tiene" ($dllRaras.Count -eq 0) ('Usa: ' + ($dllRaras -join ', '))

    $importadas = (Texto-Dumpbin @('/imports', $exe)) -split "`n" |
        ForEach-Object { if ($_ -match '^\s+[0-9A-F]+\s+(\w+)\s*$') { $Matches[1] } }
    $faltan = @($importadas | Where-Object { $soloDesdeVista -contains $_ } | Sort-Object -Unique)
    Informar "${nombre}: sin funciones de Windows que XP no tiene" ($faltan.Count -eq 0) `
        ('Usa: ' + ($faltan -join ', '))
}

# Abre el programa, espera su ventana y la cierra como lo haría la X.
function Probar-Ventana([string]$exe) {
    if (-not (Test-Path $exe)) { return }
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

    & (Join-Path $PSScriptRoot 'huellas_terceros.ps1')
    Informar 'Bibliotecas de terceros sin cambios (third_party\HUELLAS.txt)' ($LASTEXITCODE -eq 0)

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

        $carpeta = Join-Path $raiz "build\$version\Release"
        Revisar-Exe (Join-Path $carpeta 'ventrik.exe') $version
        Revisar-Exe (Join-Path $carpeta 'ventrik_pruebas.exe') $version
        Probar-Ventana (Join-Path $carpeta 'ventrik.exe')
    }

    # Lo que hay que copiar a una PC con XP para probar ahí.
    $paraXP = Join-Path $raiz 'build\para_xp'
    $clasica = Join-Path $raiz 'build\clasica\Release'
    New-Item -ItemType Directory -Force -Path $paraXP | Out-Null
    $copiado = $true
    foreach ($archivo in (Join-Path $clasica 'ventrik.exe'), (Join-Path $clasica 'ventrik_pruebas.exe'),
                         (Join-Path $raiz 'tools\probar_en_xp.bat')) {
        if (-not (Test-Path $archivo)) { $copiado = $false; continue }
        # El antivirus de Windows a veces bloquea unos segundos un .exe recién hecho.
        $listo = $false
        for ($intento = 1; $intento -le 5 -and -not $listo; $intento++) {
            try {
                Copy-Item $archivo $paraXP -Force
                $listo = $true
            } catch {
                Start-Sleep -Seconds 2
            }
        }
        if (-not $listo) { $copiado = $false }
    }
    Write-Host ''
    Informar 'Carpeta para probar en XP (build\para_xp)' $copiado
} finally {
    Pop-Location
}

Write-Host ''
if ($script:fallas -eq 0) {
    Write-Host 'TODO OK' -ForegroundColor Green
    Write-Host ('Para probar en XP, copiá la carpeta ' + (Join-Path $raiz 'build\para_xp'))
    exit 0
}
$plural = if ($script:fallas -eq 1) { 'FALLA' } else { 'FALLAS' }
Write-Host ("$script:fallas $plural") -ForegroundColor Red
exit 1
