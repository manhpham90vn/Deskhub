$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
. (Join-Path $PSScriptRoot 'pinned-tools.ps1')

if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    throw "winget not found. Install 'App Installer' from Microsoft Store, then re-run."
}

function Install-IfMissing([string]$Cmd, [string]$WingetId, [string]$Label) {
    if (Get-Command $Cmd -ErrorAction SilentlyContinue) {
        Write-Host "[ok]      $Label ($((Get-Command $Cmd).Source))"
        return $false
    }
    Write-Host "[install] $Label ($WingetId)..."
    & winget install --id $WingetId --exact --accept-source-agreements --accept-package-agreements
    if ($LASTEXITCODE -ne 0) { throw "winget failed installing $WingetId (exit $LASTEXITCODE)" }
    return $true
}

$restartNote = $false

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsOk = $false
if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vs) {
        $vsOk = $true; Write-Host "[ok]      Visual Studio C++ toolchain ($vs)"
        if (Test-Path (Join-Path $vs 'VC\Tools\Llvm\x64\bin\clang++.exe')) {
            Write-Host "[ok]      LLVM in VS (clang++ + llvm-cov)"
        } else {
            Write-Host "[action]  VS is missing the LLVM component (needed by 'make coverage')."
            Write-Host "          Open Visual Studio Installer -> Modify -> add 'C++ Clang tools for Windows'."
        }
    }
}
if (-not $vsOk) {
    Write-Host "[install] Visual Studio 2026 Build Tools (C++ workload + Clang)..."
    winget install --id Microsoft.VisualStudio.BuildTools --exact --accept-source-agreements --accept-package-agreements `
        --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Llvm.Clang --includeRecommended"
    if ($LASTEXITCODE -ne 0) { throw "winget failed installing VS Build Tools (exit $LASTEXITCODE)" }
    $restartNote = $true
}

if (Install-IfMissing 'rustc' 'Rustlang.Rustup' 'Rust (builds quiche, the QUIC transport)') { $restartNote = $true }
$cargoBin = Join-Path $env:USERPROFILE '.cargo\bin'
if (Test-Path $cargoBin) { $env:PATH = "$cargoBin;$env:PATH" }
$rustHost = ''
$rustc = Get-Command rustc -ErrorAction SilentlyContinue
if ($rustc) {
    $hostLine = & $rustc.Source -vV | Select-String '^host:'
    if ($hostLine) { $rustHost = ($hostLine.ToString() -replace '^host:\s*', '') }
}
if ($rustHost -and $rustHost -ne 'x86_64-pc-windows-msvc') {
    Write-Host "[action]  Rust host is '$rustHost' - quiche needs the MSVC toolchain."
    Write-Host "          rustup default stable-x86_64-pc-windows-msvc"
}

$nasmDir = Join-Path $env:LOCALAPPDATA 'bin\NASM'
$nasmExe = Join-Path $nasmDir 'nasm.exe'
$nasmOnPath = [bool](Get-Command nasm -ErrorAction SilentlyContinue)
if (-not $nasmOnPath -and -not (Test-Path $nasmExe)) {
    if (Install-IfMissing 'nasm' 'NASM.NASM' 'NASM (assembles BoringSSL)') { $restartNote = $true }
}
if ($nasmOnPath) {
    Write-Host "[ok]      NASM (assembles BoringSSL) ($((Get-Command nasm).Source))"
} elseif (Test-Path $nasmExe) {
    $env:PATH = "$nasmDir;$env:PATH"
    Write-Host "[ok]      NASM (assembles BoringSSL) ($nasmExe)"
    Write-Host "[action]  Add '$nasmDir' to your PATH - the NASM installer does not."
} else {
    Write-Host "[action]  nasm not found after install - 'build-quiche.sh windows' will fail without it."
}

if (Install-IfMissing 'make' 'GnuWin32.Make' 'GNU make') { $restartNote = $true }
if (Install-IfMissing 'java' 'EclipseAdoptium.Temurin.17.JDK' 'JDK 17 (Temurin)') { $restartNote = $true }

Ensure-LocalClangTools

if (Test-Path (Join-Path $root '.git')) {
    $subStatus = git -C $root submodule status
    if ($subStatus -match '(?m)^-') {
        Write-Host "[install] git submodules (nvenc headers)..."
        git -C $root submodule update --init
        if ($LASTEXITCODE -ne 0) { throw "git submodule update failed (exit $LASTEXITCODE)" }
    } else {
        Write-Host "[ok]      git submodules"
    }
}

Ensure-LocalKtlint
Ensure-LocalSwiftFormat

$cppcheckVersion = Get-PinnedToolVersion 'CPPCHECK_VERSION'
$cppcheckExe = 'C:\Program Files\Cppcheck\cppcheck.exe'
if ((Test-Path $cppcheckExe) -and (((& $cppcheckExe --version) -join ' ') -eq "Cppcheck $cppcheckVersion")) {
    Write-Host "[ok]      cppcheck $cppcheckVersion ($cppcheckExe)"
} else {
    Write-Host "[install] cppcheck $cppcheckVersion (Cppcheck.Cppcheck)..."
    & winget install --id Cppcheck.Cppcheck --exact --version $cppcheckVersion --accept-source-agreements --accept-package-agreements
    if ($LASTEXITCODE -ne 0) { throw "winget failed installing cppcheck $cppcheckVersion (exit $LASTEXITCODE)" }
}

$sdkPackages = @('platform-tools', 'platforms;android-37.0', 'ndk;26.1.10909125', 'cmake;3.22.1')

function Find-CmdlineTool([string]$SdkRoot, [string]$Name) {
    Get-ChildItem -Path (Join-Path $SdkRoot "cmdline-tools\*\bin\$Name") -ErrorAction SilentlyContinue |
        Select-Object -First 1
}

function Get-MissingSdkPackages([string]$SdkRoot) {
    @($sdkPackages | Where-Object { -not (Test-Path (Join-Path $SdkRoot ($_ -replace ';', '\'))) })
}

$sdkRoot = $env:ANDROID_HOME
if (-not $sdkRoot) { $sdkRoot = Join-Path $env:LOCALAPPDATA 'Android\Sdk' }
$androidCli = Find-CmdlineTool $sdkRoot 'android.exe'
$sdkmanager = Find-CmdlineTool $sdkRoot 'sdkmanager.bat'
if (-not $androidCli -and -not $sdkmanager) {
    Write-Host "[install] Android Studio (bootstrap for SDK)..."
    winget install --id Google.AndroidStudio --exact --accept-source-agreements --accept-package-agreements
    if ($LASTEXITCODE -ne 0) { throw "winget failed installing Android Studio (exit $LASTEXITCODE)" }
    Write-Host "  NOTE: open Android Studio once (installs SDK + cmdline-tools), then re-run bootstrap."
} else {
    Write-Host "[ok]      Android SDK ($sdkRoot)"
    $missing = Get-MissingSdkPackages $sdkRoot
    if (-not $missing) {
        Write-Host "[ok]      Android SDK packages ($($sdkPackages -join ', '))"
    } else {
        Write-Host "[install] SDK packages ($($missing -join ', '))..."
        foreach ($package in $missing) {
            if ($androidCli) {
                & $androidCli.FullName sdk install ($package -replace ';', '/')
            } else {
                & $sdkmanager.FullName --install $package
            }
        }
        $stillMissing = Get-MissingSdkPackages $sdkRoot
        if ($stillMissing) {
            throw ("Android SDK packages still absent after the install ran: $($stillMissing -join ', '). " +
                "cmdline-tools 23 replaced sdkmanager with 'android sdk install <group>/<version>', which returns a non-zero " +
                "exit code even when it succeeds, so bootstrap judges the install by the directories under '$sdkRoot' " +
                "rather than by that code. Install them by hand and re-run.")
        }
    }
}

Write-Host ""
Write-Host "bootstrap: DONE"
Write-Host "  Next: 'make' (list every target), 'make test', 'make lint', 'make build-windows'"
if ($restartNote) { Write-Host "  NOTE: open a NEW terminal so PATH changes take effect." }
