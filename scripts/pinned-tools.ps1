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
    $bootstrap = Join-Path $PSScriptRoot 'bootstrap.sh'
    $match = Select-String -Path $bootstrap -Pattern "^$Variable=(.+)$" | Select-Object -First 1
    if (-not $match) {
        throw "no $Variable in $bootstrap - the pinned tool versions live there, and the Windows scripts read them from it so the two cannot drift."
    }
    $match.Matches[0].Groups[1].Value.Trim()
}
