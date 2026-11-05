param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = 'Stop'

# The app's requireAdministrator manifest needs a shell launch to show UAC.
$options = @{
    FilePath = (Resolve-Path -LiteralPath $Executable).Path
    WorkingDirectory = (Get-Location).Path
    Verb = 'RunAs'
    Wait = $true
    PassThru = $true
}
if ($env:DESKHUB_RUN_ARGS) {
    $options.ArgumentList = $env:DESKHUB_RUN_ARGS
}

$process = Start-Process @options
exit $process.ExitCode
