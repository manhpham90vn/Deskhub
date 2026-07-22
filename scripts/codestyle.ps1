<#
codestyle.ps1 — format/lint dùng chung cho C++ (clang-format) và Kotlin (ktlint).

  scripts\codestyle.ps1           # ÁP format tại chỗ (clang-format -i, ktlint -F)
  scripts\codestyle.ps1 -Check    # chỉ KIỂM TRA, exit != 0 nếu có file lệch (dùng cho CI/pre-commit)

Tool tự dò:
  clang-format — ưu tiên trên PATH; nếu không có thì lấy bản LLVM đóng gói trong
                 Visual Studio (qua vswhere). Cùng phiên bản 22.1.x với CI để không lệch.
  ktlint       — jar tải về tools\ktlint.jar lần đầu (thư mục tools\ đã gitignore).
#>
param([switch]$Check)

$ErrorActionPreference = 'Stop'
# Chạy từ gốc repo dù gọi ở đâu.
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $root

# --- Dò clang-format -------------------------------------------------------
$clangFormat = (Get-Command clang-format -ErrorAction SilentlyContinue).Source
if (-not $clangFormat) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -property installationPath
        $cand = Join-Path $vs 'VC\Tools\Llvm\x64\bin\clang-format.exe'
        if (Test-Path $cand) { $clangFormat = $cand }
    }
}
if (-not $clangFormat) { throw "clang-format not found (PATH or Visual Studio LLVM)." }

# --- Dò/nạp ktlint ---------------------------------------------------------
$java = (Get-Command java -ErrorAction SilentlyContinue).Source
$ktlintJar = Join-Path $root 'tools\ktlint.jar'
if ($java -and -not (Test-Path $ktlintJar)) {
    New-Item -ItemType Directory -Force -Path (Split-Path $ktlintJar) | Out-Null
    Write-Host "Downloading ktlint 1.5.0..."
    Invoke-WebRequest -Uri 'https://github.com/pinterest/ktlint/releases/download/1.5.0/ktlint' -OutFile $ktlintJar
}

# --- Danh sách file --------------------------------------------------------
$cpp = git ls-files 'core/*' 'platform/*' 'client/*' | Where-Object { $_ -match '\.(h|hpp|cpp|cc|c)$' }
$kt  = git ls-files 'client/android/*' | Where-Object { $_ -match '\.kt$' }

$fail = 0

# --- C++ -------------------------------------------------------------------
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

# --- Kotlin ----------------------------------------------------------------
if ($java) {
    Write-Host "[ktlint] $($kt.Count) files"
    $args = @('-jar', $ktlintJar, '--relative')
    if (-not $Check) { $args += '-F' }
    $args += 'client/android/**/*.kt'
    & $java @args
    if ($LASTEXITCODE -ne 0) { if ($Check) { $fail = 1 } }
    else { Write-Host "  OK" }
} else {
    Write-Host "[ktlint] skipped (java not found)"
}

if ($fail) { Write-Host "codestyle: FAILED"; exit 1 }
Write-Host "codestyle: OK"
