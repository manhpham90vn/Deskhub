<#
codestyle.ps1 — format/lint trên WINDOWS cho C++ (clang-format), Kotlin (ktlint)
và Swift (swiftformat). macOS/Ubuntu dùng scripts/codestyle.sh (cùng hành vi).

  scripts\codestyle.ps1                 # ÁP format tại chỗ cho cả 3 ngôn ngữ
  scripts\codestyle.ps1 -Check         # chỉ KIỂM TRA, exit != 0 nếu có file lệch (khớp CI)
  scripts\codestyle.ps1 -Only cpp      # giới hạn một ngôn ngữ: cpp | kotlin | swift
  scripts\codestyle.ps1 -Check -Only swift

Tool do `make bootstrap` cài (script này chỉ DÙNG, thiếu thì nhắc chạy bootstrap):
  clang-format — ưu tiên trên PATH; nếu không có thì lấy bản LLVM đóng gói trong
                 Visual Studio (qua vswhere). Cùng phiên bản 22.1.x với CI để không lệch.
  ktlint       — tools\ktlint.jar (thư mục tools\ đã gitignore).
  swiftformat  — ưu tiên trên PATH, không có thì tools\swiftformat.exe.
#>
param(
    [switch]$Check,
    [ValidateSet('all', 'cpp', 'kotlin', 'swift')][string]$Only = 'all'
)

$ErrorActionPreference = 'Stop'
# Chạy từ gốc repo dù gọi ở đâu.
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $root

$fail = 0

# --- C++ (clang-format) ----------------------------------------------------
if ($Only -in @('all', 'cpp')) {
    $clangFormat = (Get-Command clang-format -ErrorAction SilentlyContinue).Source
    if (-not $clangFormat) {
        $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $vswhere) {
            $vs = & $vswhere -latest -property installationPath
            $cand = Join-Path $vs 'VC\Tools\Llvm\x64\bin\clang-format.exe'
            if (Test-Path $cand) { $clangFormat = $cand }
        }
    }
    if (-not $clangFormat) { throw "clang-format not found - run 'make bootstrap' first." }

    $cpp = git ls-files 'core/*' 'platform/*' 'client/*' | Where-Object { $_ -match '\.(h|hpp|cpp|cc|c)$' }
    Write-Host "[clang-format] $($cpp.Count) files ($clangFormat)"
    if ($Check) {
        $bad = @()
        foreach ($f in $cpp) {
            & $clangFormat --dry-run --Werror $f 2>$null
            if ($LASTEXITCODE -ne 0) { $bad += $f }
        }
        if ($bad.Count) { $fail = 1; Write-Host "  NOT formatted:"; $bad | ForEach-Object { Write-Host "   - $_" } }
        else { Write-Host "  OK" }
    } else {
        & $clangFormat -i $cpp
        Write-Host "  formatted"
    }
}

# --- Kotlin (ktlint) -------------------------------------------------------
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
        if ($LASTEXITCODE -ne 0) { if ($Check) { $fail = 1 } }
        else { Write-Host "  OK" }
    } else {
        Write-Host "[ktlint] skipped (java not found)"
    }
}

# --- Swift (swiftformat) ---------------------------------------------------
if ($Only -in @('all', 'swift')) {
    $swiftformat = (Get-Command swiftformat -ErrorAction SilentlyContinue).Source
    if (-not $swiftformat) {
        $swiftformat = Join-Path $root 'tools\swiftformat.exe'
        if (-not (Test-Path $swiftformat)) { throw "tools\swiftformat.exe not found - run 'make bootstrap' first." }
    }

    $swift = git ls-files 'client/ios/*' | Where-Object { $_ -match '\.swift$' }
    # swiftformat tự quét thư mục theo .swiftformat ở gốc repo; danh sách file chỉ để in đếm.
    Write-Host "[swiftformat] $($swift.Count) files ($swiftformat)"
    $sfArgs = @('client/ios')
    if ($Check) { $sfArgs += '--lint' }
    & $swiftformat @sfArgs
    if ($LASTEXITCODE -ne 0) { if ($Check) { $fail = 1 } }
    else { Write-Host "  OK" }
}

if ($fail) { Write-Host "codestyle: FAILED"; exit 1 }
Write-Host "codestyle: OK"
