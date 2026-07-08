"""
HC-05 传出端口自动检测脚本
在 Windows PowerShell 中运行:  python find_hc05_outgoing.py
"""

import subprocess
import sys


def get_bt_ports():
    """获取蓝牙串口的详细信息，包括传入/传出标识"""
    cmd = '''
Get-WmiObject Win32_SerialPort | 
    Where-Object { $_.Description -like "*蓝牙*" -or $_.Description -like "*Bluetooth*" } |
    Select-Object DeviceID, Description, PNPDeviceID |
    Format-List
'''
    result = subprocess.run(['powershell', '-Command', cmd], 
                          capture_output=True, text=True, timeout=15)
    return result.stdout


def get_reg_serialcomm():
    """从注册表获取串口映射"""
    cmd = 'Get-ItemProperty "HKLM:\\HARDWARE\\DEVICEMAP\\SERIALCOMM" | Format-List'
    result = subprocess.run(['powershell', '-Command', cmd],
                          capture_output=True, text=True, timeout=15)
    return result.stdout


def guess_outgoing():
    """
    尝试判断哪个是传出端口
    Windows 蓝牙串口配对通常产生两个COM口:
    - Outgoing (传出): 用于给设备发送数据
    - Incoming (传入): 用于接收设备数据
    
    根据经验，通常传出端口号较小或按顺序排列。
    最可靠的方法是尝试打开测试。
    """
    print("=" * 60)
    print("  HC-05 传出端口分析")
    print("=" * 60)
    print()
    
    # 获取WMI详细信息
    wmi_info = get_bt_ports()
    print("【蓝牙串口详细信息】")
    print(wmi_info)
    print()
    
    # 获取注册表映射
    reg_info = get_reg_serialcomm()
    print("【注册表串口映射】")
    print(reg_info)
    print()
    
    print("=" * 60)
    print("【分析与建议】")
    print("=" * 60)
    print()
    print("你有 4 个蓝牙串口，应该是 2 对蓝牙设备配对产生的:")
    print("  每对 = 传入端口(Incoming) + 传出端口(Outgoing)")
    print()
    print("【传出端口】是你需要用来给 HC-05 发送数据的")
    print("【传入端口】一般用于接收，通常不用")
    print()
    print("判断方法:")
    print("  1. 如果只配对了 HC-05 一个蓝牙设备:")
    print("     4个COM口中，2个是HC-05的(传入/传出)")
    print("     另外2个可能是其他蓝牙设备或之前的配对残留")
    print()
    print("  2. 最可靠的确认方式：逐个尝试")
    print()
    print("=" * 60)
    print("【请执行以下测试】")
    print("=" * 60)
    print()
    print("在 PowerShell 中逐条执行（每次换一个COM口）:")
    print()
    print('  python -c "import serial; s=serial.Serial(\'COM3\', 9600, timeout=2); import time; time.sleep(3); print(\'收到:\', s.read(s.in_waiting)); s.close()"')
    print('  python -c "import serial; s=serial.Serial(\'COM4\', 9600, timeout=2); import time; time.sleep(3); print(\'收到:\', s.read(s.in_waiting)); s.close()"')
    print('  python -c "import serial; s=serial.Serial(\'COM7\', 9600, timeout=2); import time; time.sleep(3); print(\'收到:\', s.read(s.in_waiting)); s.close()"')
    print('  python -c "import serial; s=serial.Serial(\'COM8\', 9600, timeout=2); import time; time.sleep(3); print(\'收到:\', s.read(s.in_waiting)); s.close()"')
    print()
    print("能收到 'STEPS:' 开头的数据的那个 COM 口，就是 HC-05 的传出端口")
    print()
    print("或者更简单: 使用 PuTTY 连接每个 COM 口，看哪个能收到数据")


if __name__ == "__main__":
    guess_outgoing()
    input("\n按回车键退出...")
