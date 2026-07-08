# 查找串口设备
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  串口设备扫描工具" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 方法1：使用 WMI 查询串口
Write-Host "【方法1：WMI 串口查询】" -ForegroundColor Yellow
$ports = Get-WmiObject Win32_SerialPort | Select-Object DeviceID, Description, PNPDeviceID
if ($ports) {
    $ports | Format-Table -AutoSize
} else {
    Write-Host "  未找到物理串口设备" -ForegroundColor Gray
}
Write-Host ""

# 方法2：使用注册表（包含虚拟串口/蓝牙串口）
Write-Host "【方法2：注册表串口映射】" -ForegroundColor Yellow
$regPath = "HKLM:\HARDWARE\DEVICEMAP\SERIALCOMM"
if (Test-Path $regPath) {
    $items = Get-ItemProperty $regPath
    $items.PSObject.Properties | Where-Object { $_.Name -notmatch "^PS" } | ForEach-Object {
        Write-Host "  $($_.Name) -> $($_.Value)"
    }
} else {
    Write-Host "  未找到注册表项" -ForegroundColor Gray
}
Write-Host ""

# 方法3：检测可用串口（尝试打开）
Write-Host "【方法3：检测可打开串口 (9600 baud)】" -ForegroundColor Yellow
for ($i = 1; $i -le 20; $i++) {
    $comName = "COM$i"
    try {
        $port = New-Object System.IO.Ports.SerialPort $comName, 9600, "None", 8, 1
        $port.Open()
        Write-Host "  $comName : 可打开（可用）" -ForegroundColor Green
        $port.Close()
    } catch {
        # 静默失败
    }
}
Write-Host ""

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "【HC-05 使用指南】" -ForegroundColor White
Write-Host ""
Write-Host "1. 先在 Windows 蓝牙设置里配对 HC-05"
Write-Host "   (PIN: 1234 或 0000)"
Write-Host ""
Write-Host "2. 配对成功后，看上面的列表："
Write-Host "   找到 'Bluetooth' 或 'HC-05' 相关的 COM 口"
Write-Host ""
Write-Host "3. 通常会有两个 COM 口："
Write-Host "   - 传出端口 (Outgoing) -> 用于给 HC-05 发送数据"
Write-Host "   - 传入端口 (Incoming) -> 一般不用"
Write-Host ""
Write-Host "4. 记住传出端口号（如 COM5），运行测试脚本时使用它"
Write-Host "========================================" -ForegroundColor Cyan

Read-Host "`n按回车键退出"
