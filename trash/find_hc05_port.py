"""
HC-05 传出端口自动探测
用法: python find_hc05_port.py
"""

import serial
import serial.tools.list_ports
import time

print("=" * 60)
print("  HC-05 传出端口自动探测")
print("=" * 60)
print()

# 先列出所有蓝牙串口
bt_ports = []
for p in serial.tools.list_ports.comports():
    if '蓝牙' in p.description or 'Bluetooth' in p.description:
        bt_ports.append(p.device)
        print(f"发现蓝牙串口: {p.device} - {p.description}")

if not bt_ports:
    print("未发现蓝牙串口！请先配对 HC-05")
    exit(1)

print()
print("正在逐个测试...")
print("-" * 60)

found = None
for port in bt_ports:
    print(f"\n测试 {port} @ 9600 ... ", end="", flush=True)
    try:
        ser = serial.Serial(port, 9600, timeout=3)
        time.sleep(3)  # 等3秒，让STM32自动上报数据
        
        data = ser.read(ser.in_waiting)
        ser.close()
        
        if data:
            text = data.decode('utf-8', errors='replace').strip()
            print(f"✅ 收到数据！")
            print(f"   内容: {text[:60]}")
            if b'STEP' in data or b'TEMP' in data or b'STEPS' in data:
                print(f"   ★★★ 确认这是 HC-05 的传出端口！★★★")
                found = port
                break
        else:
            print("无自动数据")
            
        # 再试发送命令
        ser = serial.Serial(port, 9600, timeout=2)
        ser.write(b"STEP\r\n")
        time.sleep(0.5)
        resp = ser.read(ser.in_waiting)
        ser.close()
        
        if resp:
            print(f"   发送STEP有回复: {resp.decode('utf-8','replace').strip()[:40]}")
            if not found:
                found = port
        
    except Exception as e:
        print(f"❌ 错误: {e}")

print()
print("=" * 60)
if found:
    print(f"✅ 结果: {found} 是 HC-05 的传出端口")
    print()
    print("现在可以运行:  python hc05_serial_test.py")
    print(f"并修改脚本中的 PORT = '{found}'")
else:
    print("⚠️ 未找到明确匹配的端口")
    print("可能原因:")
    print("  - STM32 未开机或未连接")
    print("  - HC-05 未正确配对")
    print("  - 波特率不是 9600")
print("=" * 60)

input("\n按回车退出...")
