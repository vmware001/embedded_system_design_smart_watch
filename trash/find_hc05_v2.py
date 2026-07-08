"""
HC-05 传出端口探测 - 防卡死版本
用法: python find_hc05_v2.py
"""

import serial
import serial.tools.list_ports
import time
import sys

print("=" * 60)
print("  HC-05 传出端口探测 (防卡死版)")
print("=" * 60)
print()

# 获取蓝牙串口
bt_ports = []
for p in serial.tools.list_ports.comports():
    if '蓝牙' in p.description or 'Bluetooth' in p.description:
        bt_ports.append(p.device)
        print(f"发现: {p.device} - {p.description}")

if not bt_ports:
    print("未发现蓝牙串口！")
    sys.exit(1)

print()
print("重要提示:")
print("  - 请关闭所有其他可能占用串口的程序")
print("  - 如果某个端口卡住，按 Ctrl+C 跳过")
print("-" * 60)

found = None

for port in bt_ports:
    print(f"\n测试 {port} ... ", end="", flush=True)
    
    try:
        # 尝试打开，超时5秒
        ser = serial.Serial(port, 9600, timeout=1, write_timeout=1)
        
        # 等自动上报
        time.sleep(3)
        data = ser.read(ser.in_waiting)
        
        if data:
            text = data.decode('utf-8', errors='replace').strip()
            print(f"✅ 收到: {text[:50]}")
            if b'STEP' in data or b'TEMP' in data:
                print(f"   ★★★ {port} 是 HC-05 传出端口！★★★")
                found = port
                ser.close()
                break
        else:
            print("无自动数据", end="")
        
        # 发送测试命令
        ser.write(b"STEP\r\n")
        time.sleep(0.5)
        resp = ser.read(ser.in_waiting)
        ser.close()
        
        if resp:
            rtext = resp.decode('utf-8', errors='replace').strip()
            print(f", 回复: {rtext[:40]}")
            if not found and (b'STEP:' in resp):
                found = port
                break
        else:
            print("")
            
    except PermissionError:
        print("❌ 拒绝访问（被其他程序占用）")
    except serial.SerialException as e:
        print(f"❌ 串口错误: {e}")
    except KeyboardInterrupt:
        print("⏭️ 跳过")
        continue
    except Exception as e:
        print(f"❌ 其他错误: {e}")

print()
print("=" * 60)
if found:
    print(f"✅ 结果: {found}")
    print()
    print("修改 hc05_serial_test.py 中的 PORT 变量:")
    print(f"  PORT = '{found}'")
else:
    print("⚠️ 未找到")
    print()
    print("可能原因:")
    print("  1. STM32 未开机")
    print("  2. HC-05 未正确配对")
    print("  3. 所有端口都被占用")
    print()
    print("建议:")
    print("  - 重启电脑，只配对 HC-05 一个设备")
    print("  - 用设备管理器确认 COM 口状态")
print("=" * 60)

input("\n按回车退出...")
