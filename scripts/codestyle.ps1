param(
    [switch]$Check,
    [ValidateSet('all', 'cpp', 'kotlin', 'swift')][string]$Only = 'all'
)

$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
. (Join-Path $PSScriptRoot 'pinned-tools.ps1')
Set-Location $root

function Get-VisualStudioLlvmDir {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return @() }
    $vs = & $vswhere -latest -products * -property installationPath
    if (-not $vs) { return @() }
    @(Join-Path $vs 'VC\Tools\Llvm\x64\bin')
}

function Get-PinnedClangFormat([string]$Version) {
    $extraDirs = Get-VisualStudioLlvmDir
    $found = Find-PinnedTool 'clang-format' $Version $extraDirs
    if ($found) { return $found }
    $others = Get-ToolCandidates 'clang-format' $extraDirs |
        ForEach-Object { "$_ is $(((& $_ --version) -join ' ') -replace '^clang-format version ', '')" }
    if (-not $others) { throw "clang-format not found - run 'make bootstrap' first." }
    throw ("clang-format $Version not found, and reformatting with another version churns files CI then rejects " +
        "($($others -join '; ')). Run 'make bootstrap' and put the clang-format it installs ahead of those on PATH.")
}

$fail = 0

if ($Only -in @('all', 'cpp')) {
    $clangFormatVersion = Get-PinnedToolVersion 'CLANG_FORMAT_VERSION'
    $clangFormat = Get-PinnedClangFormat $clangFormatVersion

    $cpp = git ls-files 'core/*' 'platform/*' 'client/*' 'tests/*' | Where-Object { $_ -match '\.(h|hpp|cpp|cc|c)$' }
    Write-Host "[clang-format] $($cpp.Count) files ($clangFormat $clangFormatVersion)"
    if ($Check) {
        $bad = @()
        $prevEap = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        foreach ($f in $cpp) {
            & $clangFormat --dry-run --Werror $f 2>$null
            if ($LASTEXITCODE -ne 0) { $bad += $f }
        }
        $ErrorActionPreference = $prevEap
        if ($bad.Count) { $fail = 1; Write-Host "  NOT formatted:"; $bad | ForEach-Object { Write-Host "   - $_" } }
        else { Write-Host "  OK" }
    } else {
        & $clangFormat -i $cpp
        Write-Host "  formatted"
    }
}

if ($Only -in @('all', 'kotlin')) {
    $java = (Get-Command java -ErrorAction SilentlyContinue).Source
    if ($java) {
        $ktlintJar = Join-Path $root 'tools\ktlint.jar'
        if (-not (Test-Path $ktlintJar)) { throw "tools\ktlint.jar not found - run 'make bootstrap' first." }

        $kt = git ls-files 'client/android/*' | Where-Object { $_ -match '\.kt$' }
        Write-Host "[ktlint] $($kt.Count) files"
        $ktArgs = @('-jar', $ktlintJar, '--relative')
        if (-not $Check) { $ktArgs += '-F' }
        $ktArgs += 'client/android/**/*.kt'
        & $java @ktArgs
        if ($LASTEXITCODE -ne 0) { $fail = 1 }
        else { Write-Host "  OK" }
    } else {
        Write-Host "[ktlint] skipped (java not found)"
    }
}

if ($Only -in @('all', 'swift')) {
    $swiftformat = (Get-Command swiftformat -ErrorAction SilentlyContinue).Source
    if (-not $swiftformat) {
        $swiftformat = Join-Path $root 'tools\swiftformat.exe'
        if (-not (Test-Path $swiftformat)) { throw "tools\swiftformat.exe not found - run 'make bootstrap' first." }
    }

    $swift = git ls-files 'client/apple/*' 'client/ios/*' 'client/macos/*' | Where-Object { $_ -match '\.swift$' }
    Write-Host "[swiftformat] $($swift.Count) files ($swiftformat)"
    $sfArgs = @('client/apple', 'client/ios', 'client/macos')
    if ($Check) { $sfArgs += '--lint' }
    & $swiftformat @sfArgs
    if ($LASTEXITCODE -ne 0) { $fail = 1 }
    else { Write-Host "  OK" }

    $swiftlint = (Get-Command swiftlint -ErrorAction SilentlyContinue).Source
    if (-not $swiftlint) {
        Write-Host "[swiftlint] skipped (swiftlint not found)"
    } else {
        $slDirs = @('client/apple/swift', 'client/ios/app/swift', 'client/ios/broadcast/swift', 'client/ios/shared', 'client/macos/app/swift')
        Write-Host "[swiftlint] $($slDirs -join ' ') ($swiftlint)"
        if (-not $Check) { & $swiftlint lint --fix --quiet @slDirs | Out-Null }
        & $swiftlint lint --strict --quiet @slDirs
        if ($LASTEXITCODE -ne 0) { if ($Check) { $fail = 1 } }
        else { Write-Host "  OK" }
    }
}

if ($fail) { Write-Host "codestyle: FAILED"; exit 1 }
Write-Host "codestyle: OK"
