param(
    [ValidateSet("all","yf90640","zones","schedule","racechrono","snapshot","view","heatmap","probe_driver","ble_driver","brake_modbus","brake_model","brake_buzzer","brake_settings","brake_wire","brake_view","brake_can","brake_driver","board_spi","brake_client","telemetry_ui","telemetry_touch","board_profile")]
    [string]$Case = "all",
    [string]$CompilerPath = "",
    [string]$CapturePath = "",
    [string]$PreviewPath = "",
    [string]$WorkspacePath = ""
)
$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$workspace = if ($WorkspacePath) { $WorkspacePath } else { Join-Path $repoRoot ".superpowers\sdd\2026-10-24-six-port-tire-brake-central" }
if (-not $CompilerPath) {
    if ($env:WIRED_TIRE_CXX) { $CompilerPath = $env:WIRED_TIRE_CXX }
    else {
        foreach ($name in @("clang++", "g++")) {
            $found = Get-Command $name -ErrorAction SilentlyContinue
            if ($found) { $CompilerPath = $found.Source; break }
        }
    }
}
if (-not $CompilerPath) {
    $portable = Join-Path $repoRoot ".superpowers/sdd/2026-10-22-wired-tire-thermal-central/tools/zig-x86_64-windows-0.14.1/zig.exe"
    if (Test-Path -LiteralPath $portable) { $CompilerPath = $portable }
}
if (-not $CompilerPath -or -not (Test-Path -LiteralPath $CompilerPath)) {
    throw "Native C++17 compiler missing. Pass -CompilerPath or set WIRED_TIRE_CXX; no tests ran."
}
$cases = @("yf90640", "zones", "schedule", "racechrono", "snapshot", "view", "heatmap", "probe_driver", "ble_driver",
    "brake_modbus", "brake_model", "brake_buzzer", "brake_settings", "brake_wire", "brake_view", "brake_can", "brake_driver", "board_spi", "brake_client")
if ($Case -ne "all") { $cases = @($Case) }
$buildRoot = Join-Path $workspace "native-tests"
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$libraryDir = Join-Path $repoRoot "firmware\libraries\WiredTireCore\src"
$sources = @(Get-ChildItem -LiteralPath $libraryDir -Filter "*.cpp" -File -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })
foreach ($testCase in $cases) {
    $testFile = Join-Path $repoRoot "tests\wired_tire\test_$testCase.cpp"
    if (-not (Test-Path -LiteralPath $testFile)) { throw "Test case missing: $testFile" }
    $executable = Join-Path $buildRoot "test_$testCase.exe"
    $compileArgs = @()
    if ((Split-Path -Leaf $CompilerPath) -match "^zig(\.exe)?$") {
        $compileArgs += "c++"
    }
    $compileArgs += @("-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O1", "-I", $libraryDir, $testFile)
    $compileArgs += $sources
    if ($testCase -in @("probe_driver", "ble_driver")) {
        $channelDir = Join-Path $repoRoot "firmware/wired_tire_channel"
        $driver = if ($testCase -eq "probe_driver") { "ProbeLink.cpp" } else { "RaceChronoBle.cpp" }
        $compileArgs += @("-I", (Join-Path $repoRoot "tests/wired_tire/stubs"), "-I", $channelDir, (Join-Path $channelDir $driver))
    }
    if ($testCase -eq "brake_driver") {
        $brakeDir = Join-Path $repoRoot "firmware/wired_brake_channel"
        $compileArgs += @("-I", (Join-Path $repoRoot "tests/wired_tire/stubs"), "-I", $brakeDir,
            (Join-Path $brakeDir "DualBrakeProbe.cpp"), (Join-Path $brakeDir "BrakeBle.cpp"),
            (Join-Path $brakeDir "BrakeNvsStore.cpp"), (Join-Path $brakeDir "BrakeSpiSlave.cpp"))
    }
    if ($testCase -in @("board_spi", "brake_client")) {
        $displayDir = Join-Path $repoRoot "firmware/wired_tire_display"
        $compileArgs += @("-I", (Join-Path $repoRoot "tests/wired_tire/stubs"), "-I", $displayDir,
            (Join-Path $displayDir "BoardSpiBus.cpp"), (Join-Path $displayDir "BrakeLinkMaster.cpp"), (Join-Path $displayDir "SnapshotMaster.cpp"))
    }
    $compileArgs += @("-o", $executable)
    & $CompilerPath @compileArgs
    if ($LASTEXITCODE -ne 0) { throw "Native compile failed: $testCase" }
    $testArgs = @()
    if ($CapturePath -and $testCase -eq "yf90640") { $testArgs = @("--capture", (Resolve-Path -LiteralPath $CapturePath).Path) }
    if ($PreviewPath -and $testCase -eq "heatmap") { $testArgs = @("--preview", $PreviewPath) }
    & $executable @testArgs
    if ($LASTEXITCODE -ne 0) { throw "Native assertions failed: $testCase" }
}
Write-Host "All requested wired tire core checks passed."
