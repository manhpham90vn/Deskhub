function Get-PythonToolDirs {
    $dirs = @(Join-Path $env:USERPROFILE '.local\bin')
    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($py) {
        $userScripts = & $py.Source -c "import sysconfig; print(sysconfig.get_path('scripts', 'nt_user'))" |
            Select-Object -First 1
        if ($userScripts) { $dirs += $userScripts }
    }
    $dirs
}

function Get-ToolCandidates([string]$Cmd, [string[]]$ExtraDirs = @()) {
    $paths = @((Get-Command $Cmd -ErrorAction SilentlyContinue).Source)
    $paths += ((Get-PythonToolDirs) + $ExtraDirs | ForEach-Object { Join-Path $_ "$Cmd.exe" })
    @($paths | Where-Object { $_ -and (Test-Path $_) } | Select-Object -Unique)
}

function Find-PinnedTool([string]$Cmd, [string]$Version, [string[]]$ExtraDirs = @()) {
    foreach ($candidate in Get-ToolCandidates $Cmd $ExtraDirs) {
        if (((& $candidate --version) -join ' ') -match [regex]::Escape($Version)) { return $candidate }
    }
    return $null
}

function Get-PinnedToolVersion([string]$Variable) {
    $pins = Join-Path $PSScriptRoot 'pinned-versions.txt'
    $match = Select-String -Path $pins -Pattern "^$Variable=(.+)$" | Select-Object -First 1
    if (-not $match) {
        throw "no $Variable in $pins - the pinned tool versions live there, and every script reads them from it so the two cannot drift."
    }
    $match.Matches[0].Groups[1].Value.Trim()
}

function Get-LocalToolsDir {
    Join-Path (Split-Path $PSScriptRoot -Parent) 'tools'
}

function Get-LocalVenvDir {
    Join-Path (Get-LocalToolsDir) 'venv'
}

function Get-LocalVenvBin {
    $venv = Get-LocalVenvDir
    $scripts = Join-Path $venv 'Scripts'
    if (Test-Path $scripts) { return $scripts }
    $bin = Join-Path $venv 'bin'
    if (Test-Path $bin) { return $bin }
    return $scripts
}

function Get-LocalHostPython {
    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($py) {
        $prevEap = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            & $py.Source -3 --version >$null 2>$null
            $pyOk = $LASTEXITCODE -eq 0
        } catch {
            $pyOk = $false
        }
        $ErrorActionPreference = $prevEap
        if ($pyOk) { return $py.Source }
    }
    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python) { return $python.Source }
    return $null
}

function Get-VisualStudioLlvmDir {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return @() }
    $vs = & $vswhere -latest -products * -property installationPath
    if (-not $vs) { return @() }
    @(Join-Path $vs 'VC\Tools\Llvm\x64\bin')
}

function Resolve-LocalClangTool([string]$Cmd, [string]$Version) {
    $venvExe = Join-Path (Get-LocalVenvBin) "$Cmd.exe"
    if ((Test-Path $venvExe) -and (((& $venvExe --version) -join ' ') -match [regex]::Escape($Version))) {
        return $venvExe
    }
    return (Find-PinnedTool $Cmd $Version (Get-VisualStudioLlvmDir))
}

function Resolve-LocalClangFormat {
    Resolve-LocalClangTool 'clang-format' (Get-PinnedToolVersion 'CLANG_FORMAT_VERSION')
}

function Resolve-LocalClangTidy {
    Resolve-LocalClangTool 'clang-tidy' (Get-PinnedToolVersion 'CLANG_TIDY_VERSION')
}

function Resolve-LocalSwiftformat {
    $onPath = (Get-Command swiftformat -ErrorAction SilentlyContinue).Source
    if ($onPath) { return $onPath }
    $exe = Join-Path (Get-LocalToolsDir) 'swiftformat.exe'
    if (Test-Path $exe) { return $exe }
    return $null
}

function Ensure-LocalKtlint {
    $version = Get-PinnedToolVersion 'KTLINT_VERSION'
    $sha = Get-PinnedToolVersion 'KTLINT_SHA256'
    $toolsDir = Get-LocalToolsDir
    New-Item -ItemType Directory -Force -Path $toolsDir | Out-Null
    $jar = Join-Path $toolsDir 'ktlint.jar'
    $verFile = Join-Path $toolsDir 'ktlint.jar.version'
    $current = if (Test-Path $verFile) { (Get-Content $verFile -Raw).Trim() } else { '' }
    if ((Test-Path $jar) -and ($current -eq $version)) {
        Write-Host "[ok]      ktlint $version ($jar)"
        return
    }
    Write-Host "[install] ktlint $version..."
    Invoke-WebRequest -Uri "https://github.com/pinterest/ktlint/releases/download/$version/ktlint" -OutFile $jar
    if ((Get-FileHash $jar -Algorithm SHA256).Hash -ne $sha) {
        Remove-Item $jar -Force
        throw "ktlint download failed the checksum check."
    }
    Set-Content -Path $verFile -Value $version
    Write-Host "[ok]      ktlint $version ($jar)"
}

function Ensure-LocalSwiftFormat {
    $version = Get-PinnedToolVersion 'SWIFTFORMAT_VERSION'
    $onPath = (Get-Command swiftformat -ErrorAction SilentlyContinue).Source
    if ($onPath) {
        Write-Host "[ok]      swiftformat ($onPath)"
        return
    }
    $toolsDir = Get-LocalToolsDir
    New-Item -ItemType Directory -Force -Path $toolsDir | Out-Null
    $exe = Join-Path $toolsDir 'swiftformat.exe'
    if (Test-Path $exe) {
        Write-Host "[ok]      swiftformat ($exe)"
        return
    }
    Write-Host "[install] SwiftFormat $version..."
    $sha = Get-PinnedToolVersion 'SWIFTFORMAT_MSI_SHA256'
    $msi = Join-Path $env:TEMP 'SwiftFormat.amd64.msi'
    $ext = Join-Path $env:TEMP 'SwiftFormatMsiExtract'
    Invoke-WebRequest -Uri "https://github.com/nicklockwood/SwiftFormat/releases/download/$version/SwiftFormat.amd64.msi" -OutFile $msi
    if ((Get-FileHash $msi -Algorithm SHA256).Hash -ne $sha) {
        Remove-Item $msi -Force
        throw "SwiftFormat download failed the checksum check."
    }
    Start-Process msiexec -ArgumentList "/a `"$msi`" /qn TARGETDIR=`"$ext`"" -Wait
    Copy-Item (Join-Path $ext 'PFiles64\nicklockwood\SwiftFormat\swiftformat.exe') $exe
    Remove-Item $msi -Force
    Remove-Item $ext -Recurse -Force
    Write-Host "[ok]      SwiftFormat $version ($exe)"
}

function Ensure-LocalClangTools {
    $formatVersion = Get-PinnedToolVersion 'CLANG_FORMAT_VERSION'
    $tidyVersion = Get-PinnedToolVersion 'CLANG_TIDY_VERSION'
    $format = Resolve-LocalClangFormat
    $tidy = Resolve-LocalClangTidy
    if ($format -and $tidy) {
        Write-Host "[ok]      clang-format $formatVersion ($format)"
        Write-Host "[ok]      clang-tidy $tidyVersion ($tidy)"
        return
    }
    Write-Host "[install] clang-format $formatVersion + clang-tidy $tidyVersion (tools/venv)..."
    $venv = Get-LocalVenvDir
    New-Item -ItemType Directory -Force -Path (Get-LocalToolsDir) | Out-Null
    $hostPython = Get-LocalHostPython
    if (-not $hostPython) {
        throw "Python 3 not found - install it (winget install Python.Python.3.12), reopen the terminal, then re-run."
    }
    if (-not (Test-Path $venv)) {
        if ($hostPython -match 'py(\.exe)?$') {
            & $hostPython -3 -m venv $venv
        } else {
            & $hostPython -m venv $venv
        }
        if ($LASTEXITCODE -ne 0) { throw "python -m venv failed (exit $LASTEXITCODE)" }
    }
    $venvPython = Join-Path (Get-LocalVenvBin) 'python.exe'
    & $venvPython -m pip install "clang-format==$formatVersion" "clang-tidy==$tidyVersion"
    if ($LASTEXITCODE -ne 0) { throw "pip failed installing the pinned clang tools (exit $LASTEXITCODE)" }
    $format = Resolve-LocalClangFormat
    $tidy = Resolve-LocalClangTidy
    if (-not $format -or -not $tidy) {
        throw "clang-format $formatVersion + clang-tidy $tidyVersion are still not runnable after their install."
    }
    Write-Host "[ok]      clang-format $formatVersion ($format)"
    Write-Host "[ok]      clang-tidy $tidyVersion ($tidy)"
}

function Ensure-LocalStyleTools {
    Ensure-LocalClangTools
    Ensure-LocalKtlint
    Ensure-LocalSwiftFormat
}
