param(
    [Parameter(Mandatory=$true)][ValidateSet("channel","brake","display")][string]$Target,
    [Parameter(Mandatory=$true)][string]$ElfPath,
    [Parameter(Mandatory=$true)][string]$ArduinoCli
)
$ErrorActionPreference = "Stop"
if (-not (Test-Path -LiteralPath $ElfPath)) { throw "ELF missing: $ElfPath" }
$arduinoData = ((& $ArduinoCli config get directories.data) -join "").Trim()
if ($LASTEXITCODE -ne 0) { throw "Cannot find Arduino data directory" }
if ($env:ARDUINO_DIRECTORIES_DATA) { $arduinoData = $env:ARDUINO_DIRECTORIES_DATA }
if (-not $arduinoData) {
    $taskUserDirectory = [Environment]::GetFolderPath("UserProfile")
    if ($env:OS -eq "Windows_NT") { $arduinoData = Join-Path $env:LOCALAPPDATA "Arduino15" }
    elseif ($IsMacOS) { $arduinoData = Join-Path $taskUserDirectory "Library/Arduino15" }
    else { $arduinoData = Join-Path $taskUserDirectory ".arduino15" }
}
$pattern = Join-Path $arduinoData "packages/esp32/tools/esp-rv32/*/bin/riscv32-esp-elf-nm*"
$nm = Get-ChildItem -Path $pattern -File | Where-Object { $_.Name -match '^riscv32-esp-elf-nm(\.exe)?$' } | Select-Object -First 1
if (-not $nm) { throw "RISC-V nm unavailable" }
$symbols = @(& $nm.FullName --defined-only $ElfPath)
if ($LASTEXITCODE -ne 0) { throw "ELF symbol inspection failed" }
$required = if ($Target -eq "channel") {
    @("(esp_ble_gatts_send_indicate|ble_gatts_notify_custom)", "spi_slave_queue_trans", "uart_set_mode")
} elseif ($Target -eq "brake") {
    @("(esp_ble_gatts_send_indicate|ble_gatts_notify_custom)", "spi_slave_queue_trans", "uart_set_mode", "nvs_set_blob", "nvs_get_blob")
} else {
    @("esp_lcd_new_dsi_bus", "spi_device_queue_trans", "esp_lcd_panel_draw_bitmap", "esp_lcd_new_panel_ek79007", "esp_lcd_touch_new_i2c_gt911", "i2c_master_transmit_receive")
}
foreach ($symbol in $required) {
    if (-not ($symbols | Where-Object { $_ -match "\s$symbol$" })) {
        throw "Offline link coverage missing required hardware symbol: $symbol"
    }
}
Write-Host "PASS wired tire $Target full hardware-path linkage (IO remains disabled)."
