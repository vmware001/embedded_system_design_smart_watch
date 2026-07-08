#!/usr/bin/env python3
"""
HC-05 蓝牙模块 - 串口测试工具
使用 pyserial 通过 Windows 虚拟 COM 口与 HC-05 通信

前提：
  1. Windows 蓝牙设置中已配对 HC-05
  2. 安装 pyserial: pip install pyserial

用法:
    python hc05_test_com.py
"""

import serial
import serial.tools.list_ports
import time
import sys


def list_ports():
    """列出所有串口，标记蓝牙设备"""
    ports = serial.tools.list_ports.comports()
    bt_ports = []
    
    print("=" * 60)
    print("串口列表")
    print("-" * 60)
    
    for i, p in enumerate(ports):
        is_bt = 'bluetooth' in p.description.lower() or 'bt' in p.description.lower()
        marker = "  <-- 可能是 HC-05" if is_bt else ""
        print(f"[{i}] {p.device:8} | {p.description:40}{marker}")
        if is_bt:
            bt_ports.append(i)
    
    print("-" * 60)
    return ports, bt_ports


def test_port(port_name, baud=9600):
    """测试单个 COM 口，返回是否成功收到数据"""
    print(f"\n尝试打开 {port_name} @ {baud}...")
    
    try:
        ser = serial.Serial(port_name, baud, timeout=2, write_timeout=1)
        print(f"✅ 已打开 {port_name}")
        
        # 等待自动上报数据（STM32 每 2 秒发送一次）
        print("等待 3 秒，看能否收到自动上报...")
        time.sleep(3)
        
        data = ser.read(ser.in_waiting)
        if data:
            text = data.decode('utf-8', errors='replace').strip()
            print(f"📥 收到自动数据: {text[:100]}")
            has_auto = True
        else:
            print("⏳ 3 秒内未收到自动数据")
            has_auto = False
        
        # 发送测试命令
        print("\n发送测试命令 'STEP'...")
        ser.write(b"STEP\r\n")
        time.sleep(0.5)
        
        resp = ser.read(ser.in_waiting)
        if resp:
            rtext = resp.decode('utf-8', errors='replace').strip()
            print(f"📥 回复: {rtext[:100]}")
            
            # 检查是否是预期回复
            if 'STEP:' in rtext or 'STEPS:' in rtext:
                print(f"★★★ {port_name} 是 HC-05 的传出端口！★★★")
                ser.close()
                return True
        else:
            print("📥 无回复")
        
        ser.close()
        return has_auto  # 如果收到了自动数据也算成功
        
    except serial.SerialException as e:
        print(f"❌ 错误: {e}")
        return False


def interactive_mode(port_name, baud=9600):
    """交互模式：持续收发"""
    print(f"\n{'='*60}")
    print(f"交互模式: {port_name} @ {baud}")
    print("输入命令发送给 STM32，输入 'quit' 退出")
    print("常用命令: STEP, TEMP, TIME, DEBUG, SYNC,12:34:56")
    print(f"{'='*60}\n")
    
    try:
        ser = serial.Serial(port_name, baud, timeout=0.1)
        
        while True:
            # 读取收到的数据（非阻塞）
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                text = data.decode('utf-8', errors='replace')
                for line in text.split('\n'):
                    line = line.strip()
                    if line:
                        print(f"[{time.strftime('%H:%M:%S')}] 📥 {line}")
            
            # 检查用户输入
            try:
                cmd = input("📤 ").strip()
            except EOFError:
                break
            
            if cmd.lower() == 'quit':
                break
            
            if cmd:
                ser.write(f"{cmd}\r\n".encode())
        
        ser.close()
        print("已断开连接")
        
    except Exception as e:
        print(f"错误: {e}")


def main():
    print("=" * 60)
    print("  HC-05 串口通信测试")
    print("=" * 60)
    print()
    print("前提：请先在 Windows 蓝牙设置中配对 HC-05")
    print("      PIN 码: 1234 或 0000")
    print()
    
    # 列出串口
    ports, bt_ports = list_ports()
    
    if not ports:
        print("未找到任何串口！")
        return
    
    # 如果有蓝牙串口，优先测试
    test_indices = bt_ports if bt_ports else list(range(len(ports)))
    
    found = None
    for idx in test_indices:
        p = ports[idx]
        if test_port(p.device):
            found = p.device
            break
    
    if not found:
        print("\n自动测试未找到传出端口")
        choice = input("\n是否手动输入 COM 口测试? [y/n]: ").strip().lower()
        if choice == 'y':
            port_name = input("输入 COM 口号 (如 COM5): ").strip()
            if test_port(port_name):
                found = port_name
    
    if found:
        print(f"\n✅ 确认使用 {found}")
        choice = input("是否进入交互模式? [y/n]: ").strip().lower()
        if choice == 'y':
            interactive_mode(found)
    else:
        print("\n⚠️ 未找到可用端口")
        print("可能原因:")
        print("  1. HC-05 未配对")
        print("  2. 配对后未生成 COM 口")
        print("  3. 所有端口被占用")
        print("\n建议:")
        print("  - 重启电脑，只配对 HC-05")
        print("  - 使用 USB-TTL 直接连接 STM32 测试")


if __name__ == "__main__":
    main()
