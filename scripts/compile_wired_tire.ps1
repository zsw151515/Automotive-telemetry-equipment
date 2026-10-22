param(
    [Parameter(Mandatory=$true)][ValidateSet("channel","display")][string]$Target,
    [ValidateSet("offline","hardware")][string]$Profile = "offline",
    [string]$ArduinoCli = ""
)
$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
if (-not $ArduinoCli) {
    $found = Get-Command arduino-cli -ErrorAction SilentlyContinue
    if ($found) { $ArduinoCli = $found.Source }
    elseif (Test-Path -LiteralPath "E:\arduino\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe") {
        $ArduinoCli = "E:\arduino\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
    } else { throw "Arduino CLI unavailable: pass -ArduinoCli" }
}
$sketch = Join-Path $repoRoot "firmware\wired_tire_$Target"
if (-not (Test-Path -LiteralPath (Join-Path $sketch "wired_tire_$Target.ino"))) { throw "Sketch not implemented: $Target" }
$offlineFlag = 1
if ($Profile -eq "hardware") {
    $profileHeader = if ($Target -eq "channel") { "ApprovedChannelProfile.h" } else { "ApprovedDisplayProfile.h" }
    if (-not (Test-Path -LiteralPath (Join-Path $sketch $profileHeader))) { throw "Reviewed profile missing: $profileHeader. No hardware build made." }
    $offlineFlag = 0
}
$fqbn = if ($Target -eq "channel") { "esp32:esp32:esp32c3" } else { "esp32:esp32:esp32p4" }
$buildRoot = Join-Path $repoRoot ".superpowers\sdd\2026-10-22-wired-tire-thermal-central\arduino-$Target-$Profile"
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$compileArgs = @("compile","--fqbn",$fqbn,"--libraries",(Join-Path $repoRoot "firmware\libraries"),
    "--build-path",$buildRoot,"--build-property","compiler.cpp.extra_flags=-DWIRED_TIRE_OFFLINE_BUILD=$offlineFlag",$sketch)
& $ArduinoCli @compileArgs
if ($LASTEXITCODE -ne 0) { throw "Compile failed: $Target / $Profile" }
Write-Host "PASS wired tire $Target compile ($Profile). Compile success is not hardware validation."
