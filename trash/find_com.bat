@echo off
chcp 65001 >nul
echo ============================================
echo  查找串口设备 (COM 口)
echo ============================================
echo.

echo 【方法1：设备管理器快捷方式】
echo  即将打开设备管理器，请展开 "端口(COM 和 LPT)" 查看
echo  寻找类似 "Bluetooth 链接上的标准串行" 的条目
echo.
pause
start devmgmt.msc
echo.

echo 【方法2：命令行列表】
echo  正在列出所有串口...
echo.

for /f "tokens=1,2,*" %%a in ('reg query HKLM\HARDWARE\DEVICEMAP\SERIALCOMM 2^>nul') do (
    echo   %%a = %%c
)

echo.
echo 【方法3：MODE 命令检测】
echo  检测 COM1-COM20 中哪些可用...
echo.

for /L %%i in (1,1,20) do (
    mode COM%%i: BAUD=9600 PARITY=n DATA=8 STOP=1 >nul 2>&1
    if !errorlevel! == 0 (
        echo   COM%%i: 可用
    )
)

echo.
echo ============================================
echo  提示：
echo  1. HC-05 配对后通常显示为：
echo     "Bluetooth 链接上的标准串行 (COMx)"
echo  2. 如果有两个（COMx/COMy），选【传出】那个
echo  3. 波特率默认 9600（和你 STM32 代码一致）
echo ============================================
pause
