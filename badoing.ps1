param(
    [Parameter(Position = 0)]
    [ValidateSet('help', 'pull', 'build', 'test', 'install', 'update', 'run', 'login')]
    [string]$Command = 'help',
    [Parameter(Position = 1)]
    [ValidateSet('on', 'off', 'status')]
    [string]$LoginAction = 'status',
    [switch]$Login,
    [string]$Prefix = $env:BADOING_PREFIX,
    [string]$BuildDir = $env:BADOING_BUILD_DIR,
    [ValidateSet('ON', 'OFF')]
    [string]$Sound = $(if ($env:BADOING_SOUND) { $env:BADOING_SOUND } else { 'ON' }),
    [string[]]$AppArgs = @()
)

$ErrorActionPreference = 'Stop'
$repo = $PSScriptRoot
if (-not $Prefix) { $Prefix = Join-Path $env:LOCALAPPDATA 'Programs/BADOING' }
if (-not $BuildDir) { $BuildDir = Join-Path $repo 'build' }
$Prefix = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Prefix)
$BuildDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildDir)
$app = Join-Path $Prefix 'bin/badoing-pet.exe'

function Invoke-Checked([string]$Program, [string[]]$CommandArgs) {
    & $Program @CommandArgs
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE." }
}

function Require-App {
    if (-not (Test-Path -LiteralPath $app -PathType Leaf)) { throw "Install first: $app was not found." }
}

function Build-App {
    Invoke-Checked 'cmake' @('-S', $repo, '-B', $BuildDir, '-DCMAKE_BUILD_TYPE=Release', "-DBADOING_ENABLE_SOUND=$Sound")
    Invoke-Checked 'cmake' @('--build', $BuildDir, '--config', 'Release', '--parallel')
}

function Join-AppArguments([string[]]$Values) {
    # Windows command-line quoting, including quotes and trailing backslashes.
    ($Values | ForEach-Object {
        '"' + ($_ -replace '(\\*)"', '$1$1\"' -replace '(\\+)$', '$1$1') + '"'
    }) -join ' '
}

function Invoke-Login([string]$Action, [string[]]$Values = @()) {
    Require-App
    $arguments = Join-AppArguments (@('--login', $Action) + $Values)
    $process = Start-Process -FilePath $app -ArgumentList $arguments -Wait -PassThru
    if ($Action -eq 'status') {
        if ($process.ExitCode -eq 0) { Write-Output 'Enabled'; return }
        if ($process.ExitCode -eq 3) { Write-Output 'Disabled'; return }
    }
    if ($process.ExitCode -ne 0) {
        throw "Could not change login startup (exit $($process.ExitCode)). Check startup folder permissions."
    }
}

function Enable-Login {
    Invoke-Login 'on' $AppArgs
    Write-Output 'Enabled for the next login.'
}

function Install-App {
    Build-App
    Invoke-Checked 'cmake' @('--install', $BuildDir, '--config', 'Release', '--prefix', $Prefix)
    if ($Login) { Enable-Login }
}

switch ($Command) {
    'help' {
        @'
Usage: .\badoing.ps1 COMMAND
  pull                    Pull this checkout with git pull --ff-only
  build                   Configure and compile
  test                    Build and run the tests
  install [-Login]        Build and install; optionally enable login startup
  update [-Login]         Pull, build, and install
  run                     Run the installed pet once
  login on|off|status     Enable, disable, or inspect future login startup

Options: -Prefix PATH, -BuildDir PATH, -Sound ON|OFF, -AppArgs @('--mute')
Environment: BADOING_PREFIX, BADOING_BUILD_DIR, BADOING_SOUND, CMAKE_PREFIX_PATH.
Defaults install for your user with sound enabled. Login startup is opt-in.
'@
    }
    'pull' { Invoke-Checked 'git' @('-C', $repo, 'pull', '--ff-only') }
    'build' { Build-App }
    'test' {
        Build-App
        Push-Location $BuildDir
        try { Invoke-Checked 'ctest' @('-C', 'Release', '--output-on-failure') }
        finally { Pop-Location }
    }
    'install' { Install-App }
    'update' {
        Invoke-Checked 'git' @('-C', $repo, 'pull', '--ff-only')
        Install-App
    }
    'run' {
        Require-App
        if ($AppArgs.Count) {
            $process = Start-Process -FilePath $app -ArgumentList (Join-AppArguments $AppArgs) -Wait -PassThru
        } else {
            $process = Start-Process -FilePath $app -Wait -PassThru
        }
        exit $process.ExitCode
    }
    'login' {
        switch ($LoginAction) {
            'on' { Enable-Login }
            'off' {
                Invoke-Login 'off'
                Write-Output 'Disabled future login startup.'
            }
            'status' {
                Invoke-Login 'status'
            }
        }
    }
}
