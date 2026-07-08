"""
HC-05 串口检测工具 - 零依赖版本
不需要安装 pyserial，直接用 Windows WMI 查询串口

用法:
    python find_com_no_dep.py
"""

import subprocess
import sys


def get_com_ports_via_wmi():
    """使用 WMI 查询串口设备"""
    try:
        result = subprocess.run(
            ['powershell', '-Command',
             'Get-WmiObject Win32_SerialPort | Select-Object DeviceID, Description, PNPDeviceID | Format-Table -AutoSize'],
            capture_output=True, text=True, timeout=10
        )
        return result.stdout
    except Exception as e:
        return f"WMI 查询失败: {e}"


def get_com_ports_via_reg():
    """使用注册表查询串口映射"""
    try:
        result = subprocess.run(
            ['powershell', '-Command',
             'Get-ItemProperty "HKLM:\HARDWARE\DEVICEMAP\SERIALCOMM" | Select-Object * | Format-List'],
            capture_output=True, text=True, timeout=10
        )
        return result.stdout
    except Exception as e:
        return f"注册表查询失败: {e}"


def test_port_open(port, baud=9600):
    """尝试打开串口测试"""
    try:
        import serial
        ser = serial.Serial(port, baud, timeout=1)
        ser.close()
        return True
    except ImportError:
        return None  # pyserial 未安装
    except Exception as e:
        return False


def main():
    print("=" * 60)
    print("  HC-05 串口检测设备")
    print("=" * 60)
    print()

    # 方法1: WMI
    print("【方法1: WMI 查询串口设备】")
    print("-" * 60)
    wmi_output = get_com_ports_via_wmi()
    print(wmi_output if wmi_output.strip() else "  未找到物理串口设备")
    print()

    # 方法2: 注册表
    print("【方法2: 注册表串口映射】")
    print("-" * 60)
    reg_output = get_com_ports_via_reg()
    print(reg_output if reg_output.strip() else "  未找到注册表项")
    print()

    # 方法3: 如果 pyserial 可用，检测可打开串口
    print("【方法3: 检测可打开串口】")
    print("-" * 60)
    try:
        import serial.tools.list_ports
        ports = serial.tools.list_ports.comports()
        if ports:
            for p in ports:
                bt_marker = ""
                if 'bluetooth' in p.description.lower() or 'bt' in p.description.lower():
                    bt_marker = "  <-- 可能是蓝牙设备"
                print(f"  {p.device:8} | {p.description:40}{bt_marker}")
        else:
            print("  未找到串口")
    except ImportError:
        print("  pyserial 未安装，跳过此检测")
        print("  如需安装: pip install pyserial")
    print()

    print("=" * 60)
    print("【HC-05 使用说明】")
    print("=" * 60)
    print()
    print("1. 如果上面列表为空，请先配对 HC-05:")
    print("   Windows 设置 → 蓝牙 → 添加设备 → HC-05")
    print("   PIN 码: 1234 或 0000")
    print()
    print("2. 配对成功后重新运行本脚本，应该能看到:")
    print("   COMx | Bluetooth 链接上的标准串行")
    print()
    print("3. 记住 COM 口号，然后运行:")
    print("   python hc05_serial_test.py")
    print("   (修改脚本里的 PORT = 'COMx')")
    print()
    print("4. 或使用 PuTTY 直接连接 COMx @ 9600")
    print("=" * 60)

    input("\n按回车键退出...")


if __name__ == "__main__":
    main()
