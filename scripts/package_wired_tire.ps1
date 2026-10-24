param([string]$SourceRoot = "", [string]$OutputRoot = "")
$ErrorActionPreference = "Stop"
function Get-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace("-", "").ToLowerInvariant() }
    finally { $algorithm.Dispose(); $stream.Dispose() }
}
if (-not $SourceRoot) { $SourceRoot = Join-Path $PSScriptRoot ".." }
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
$required = @(
    "firmware/wired_tire_channel/wired_tire_channel.ino",
    "firmware/wired_tire_display/wired_tire_display.ino",
    "firmware/wired_brake_channel/wired_brake_channel.ino",
    "firmware/libraries/WiredTireCore/library.properties",
    "firmware/libraries/WiredTireCore/src/WiredTireCore.h",
    "scripts/compile_wired_tire.ps1", "scripts/check_wired_tire_link.ps1",
    "scripts/test_wired_tire_core.ps1", "scripts/capture_yf90640.py", "tests/wired_tire/test_heatmap.cpp"
)
foreach ($file in @("BrakeChannelConfig.h","DualBrakeProbe.h","DualBrakeProbe.cpp","BrakeNvsStore.h","BrakeNvsStore.cpp","BrakeBle.h","BrakeBle.cpp","BrakeSpiSlave.h","BrakeSpiSlave.cpp")) { $required += "firmware/wired_brake_channel/$file" }
foreach ($file in @("BoardSpiBus.h","BoardSpiBus.cpp","BrakeLinkMaster.h","BrakeLinkMaster.cpp","Board7BProfile.h","Board7BPanel.h","Board7BPanel.cpp","Board7BTouch.h","Board7BTouch.cpp","SnapshotMaster.h","SnapshotMaster.cpp","HeatmapRenderer.h","HeatmapRenderer.cpp","PanelDriver.h","PanelDriver.cpp","DisplayConfig.h")) { $required += "firmware/wired_tire_display/$file" }
$required += "firmware/wired_tire_display/src/vendor/SOURCE_MANIFEST.json"
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $SourceRoot $relative) -PathType Leaf)) { throw "Required source file missing: $relative" }
}
$vendorRoot = Join-Path $SourceRoot "firmware/wired_tire_display/src/vendor"
$vendorManifest = Get-Content -LiteralPath (Join-Path $vendorRoot "SOURCE_MANIFEST.json") -Raw | ConvertFrom-Json
foreach ($entry in $vendorManifest) {
    $file = Join-Path $vendorRoot $entry.file
    if (-not (Test-Path -LiteralPath $file -PathType Leaf) -or (Get-Sha256 $file) -ne $entry.sha256) { throw "Vendor source missing or SHA256 mismatch: $($entry.file)" }
}
$deliveryDoc = @(Get-ChildItem -LiteralPath (Join-Path $SourceRoot "docs") -Filter "12_*.md" -File)
if ($deliveryDoc.Count -ne 1) { throw "Expected one offline delivery instruction document" }
if (-not $OutputRoot) { $OutputRoot = Join-Path $SourceRoot "output/wired-tire" }
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path
$commit = "unversioned"
$dirty = $null
if (Get-Command git -ErrorAction SilentlyContinue) {
    $previousAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $candidate = ((& git -C $SourceRoot rev-parse HEAD 2>$null) -join "").Trim()
        if ($LASTEXITCODE -eq 0 -and $candidate -match "^[a-f0-9]{40}$") {
            $commit = $candidate
            $changes = @(& git -C $SourceRoot status --porcelain 2>$null)
            $dirty = $changes.Count -ne 0
        }
    } finally { $ErrorActionPreference = $previousAction }
}
$shortCommit = if ($commit -eq "unversioned") { $commit } else { $commit.Substring(0,7) }
$package = Join-Path $OutputRoot ("offline-" + $shortCommit + "-" + [Guid]::NewGuid().ToString("N").Substring(0,8))
New-Item -ItemType Directory -Path $package | Out-Null
$roots = @("firmware/wired_tire_channel", "firmware/wired_brake_channel", "firmware/wired_tire_display", "firmware/libraries/WiredTireCore",
    "scripts", "tests/wired_tire", "docs", "hardware/wired_tire_central")
$allowedScripts = @("compile_wired_tire.ps1","check_wired_tire_link.ps1","test_wired_tire_core.ps1",
    "capture_yf90640.py","capture_brake_rs485.py","package_wired_tire.ps1","check_docs_consistency.py")
foreach ($relativeRoot in $roots) {
    $source = Join-Path $SourceRoot $relativeRoot
    if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw "Required directory missing: $relativeRoot" }
    foreach ($file in Get-ChildItem -LiteralPath $source -File -Recurse) {
        if ($file.Name -like "Approved*Profile.h" -or $file.FullName -match "[\\/]__pycache__[\\/]") { continue }
        if ($relativeRoot -eq "scripts" -and $file.Name -notin $allowedScripts) { continue }
        $relative = $file.FullName.Substring($SourceRoot.Length + 1)
        $destination = Join-Path $package $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $destination
    }
}
Copy-Item -LiteralPath $deliveryDoc[0].FullName -Destination (Join-Path $package "README_OFFLINE.md")
foreach ($relative in @("README.md","CODEX.md","AGENTS.md","firmware/brake_temp_alarm_display_12v_rs485/README.md")) {
    $destination=Join-Path $package $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $SourceRoot $relative) -Destination $destination
}
Compress-Archive -LiteralPath (Join-Path $package "firmware/libraries/WiredTireCore") -DestinationPath (Join-Path $package "WiredTireCore.zip")
$files = @(Get-ChildItem -LiteralPath $package -Recurse -File | Sort-Object FullName | ForEach-Object {
    [ordered]@{
        path = $_.FullName.Substring($package.Length + 1).Replace("\","/")
        bytes = $_.Length
        sha256 = Get-Sha256 $_.FullName
    }
})
$manifest = [ordered]@{
    version = "0.2.0-offline-six-port"
    source_commit = $commit
    source_dirty = $dirty
    build_profile = "offline"
    arduino_esp32_core = "3.3.7"
    hardware_validated = $false
    actual_fps_measured = $false
    verification_run_by_packager = $false
    files = $files
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $package "MANIFEST.json") -Encoding UTF8
$archive = $package + ".zip"
Compress-Archive -LiteralPath $package -DestinationPath $archive
Write-Output "PACKAGE_DIRECTORY=$package"
Write-Output "PACKAGE_ZIP=$archive"
Write-Output ("PACKAGE_SHA256=" + (Get-Sha256 $archive))
exit 0
