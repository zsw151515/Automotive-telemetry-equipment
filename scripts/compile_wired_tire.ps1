param(
    [Parameter(Mandatory=$true)][ValidateSet("channel","brake","display")][string]$Target,
    [ValidateSet("offline","hardware")][string]$Profile = "offline",
    [string]$ArduinoCli = "",
    [ValidateSet("prev3","postv3")][string]$ChipVariant = "prev3"
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
$sketchName = if ($Target -eq "brake") { "wired_brake_channel" } else { "wired_tire_$Target" }
$sketch = Join-Path $repoRoot "firmware/$sketchName"
if (-not (Test-Path -LiteralPath (Join-Path $sketch "$sketchName.ino"))) { throw "Sketch not implemented: $Target" }
$offlineFlag = 1
if ($Profile -eq "hardware") {
    $profileHeader = if ($Target -eq "channel") { "ApprovedChannelProfile.h" } elseif ($Target -eq "brake") { "ApprovedBrakeProfile.h" } else { "ApprovedDisplayProfile.h" }
    if (-not (Test-Path -LiteralPath (Join-Path $sketch $profileHeader))) { throw "Reviewed profile missing: $profileHeader. No hardware build made." }
    $offlineFlag = 0
}
$fqbn = if ($Target -eq "channel") { "esp32:esp32:esp32c3" } elseif ($Target -eq "brake") { "esp32:esp32:esp32c3:CDCOnBoot=cdc" } else { "esp32:esp32:esp32p4:ChipVariant=$ChipVariant,PSRAM=enabled" }
$buildRoot = Join-Path $repoRoot ".superpowers/sdd/2026-10-24-six-port-tire-brake-central/arduino-$Target-$Profile-$ChipVariant"
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$compileArgs = @("compile","--fqbn",$fqbn,"--libraries",(Join-Path $repoRoot "firmware\libraries"),
    "--build-path",$buildRoot,"--build-property","compiler.cpp.extra_flags=-DWIRED_TIRE_OFFLINE_BUILD=$offlineFlag",$sketch)
& $ArduinoCli @compileArgs
if ($LASTEXITCODE -ne 0) { throw "Compile failed: $Target / $Profile" }
if ($Profile -eq "offline") {
    $elfFiles = @(Get-ChildItem -LiteralPath $buildRoot -Filter "*.elf" -File)
    if ($elfFiles.Count -ne 1) { throw "Expected one target ELF for link inspection" }
    & (Join-Path $PSScriptRoot "check_wired_tire_link.ps1") -Target $Target -ElfPath $elfFiles[0].FullName -ArduinoCli $ArduinoCli
}
Write-Host "PASS wired tire $Target compile ($Profile). Compile success is not hardware validation."
