param(
    [string]$SketchPathInput = "",
    [string]$Fqbn = "esp32:esp32:esp32c3",
    [string]$ArduinoCli = "",
    [switch]$All
)

$ErrorActionPreference = "Stop"

function Find-ArduinoCli {
    param([string]$Requested)

    if ($Requested) {
        return $Requested
    }

    if ($env:ARDUINO_CLI) {
        return $env:ARDUINO_CLI
    }

    $cmd = Get-Command arduino-cli -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $knownPaths = @(
        "E:\arduino\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe",
        "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe",
        "$env:ProgramFiles\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
    )

    foreach ($path in $knownPaths) {
        if ($path -and (Test-Path -LiteralPath $path)) {
            return $path
        }
    }

    throw "arduino-cli not found. Set ARDUINO_CLI or pass -ArduinoCli."
}

function Copy-SketchToTemp {
    param([System.IO.FileInfo]$InoFile)

    $sketchName = [System.IO.Path]::GetFileNameWithoutExtension($InoFile.Name)
    $tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) "automotive-telemetry-arduino"
    $tempSketch = Join-Path $tempRoot $sketchName

    if (Test-Path -LiteralPath $tempSketch) {
        $resolvedTemp = (Resolve-Path -LiteralPath $tempSketch).Path
        $resolvedRoot = if (Test-Path -LiteralPath $tempRoot) { (Resolve-Path -LiteralPath $tempRoot).Path } else { $tempRoot }
        if (-not $resolvedTemp.StartsWith($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove unexpected temp path: $resolvedTemp"
        }
        Remove-Item -LiteralPath $tempSketch -Recurse -Force
    }

    New-Item -ItemType Directory -Path $tempSketch -Force | Out-Null

    $sourceDir = $InoFile.Directory.FullName
    Get-ChildItem -LiteralPath $sourceDir -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $tempSketch $_.Name)
    }

    $expectedMain = Join-Path $tempSketch "$sketchName.ino"
    Copy-Item -LiteralPath $InoFile.FullName -Destination $expectedMain -Force

    return $tempSketch
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$cli = Find-ArduinoCli -Requested $ArduinoCli

if ($All) {
    $sketches = Get-ChildItem -Path (Join-Path $repoRoot "firmware") -Recurse -Filter *.ino
} elseif ($SketchPathInput) {
    $sketchPath = if ([System.IO.Path]::IsPathRooted($SketchPathInput)) { $SketchPathInput } else { Join-Path $repoRoot $SketchPathInput }
    $sketches = @(Get-Item -LiteralPath $sketchPath)
} else {
    throw "Pass -Sketch firmware/path/file.ino or -All."
}

if (-not $sketches -or $sketches.Count -eq 0) {
    throw "No Arduino sketches found."
}

Write-Host "arduino-cli: $cli"
Write-Host "FQBN: $Fqbn"

foreach ($ino in $sketches) {
    Write-Host ""
    Write-Host "==> Compiling $($ino.FullName)"
    $tempSketch = Copy-SketchToTemp -InoFile $ino
    & $cli compile --fqbn $Fqbn $tempSketch
    if ($LASTEXITCODE -ne 0) {
        throw "Compile failed: $($ino.FullName)"
    }
}

Write-Host ""
Write-Host "Arduino compile checks passed."
