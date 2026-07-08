#!/usr/bin/env python3
"""
HC-05 蓝牙模块 - 串口测试脚本
需要安装 pyserial:  pip install pyserial

用法:
  1. 先用 find_com.ps1 或设备管理器找到 HC-05 的 COM 口号
  2. 修改本脚本中的 PORT 变量
  3. 运行: python hc05_serial_test.py

HC-05 注意事项:
  - HC-05 是经典蓝牙 SPP，不是 BLE！
  - 必须先在 Windows 蓝牙设置里配对
  - 配对后会产生虚拟 COM 口
  - 使用传出端口 (Outgoing) 发送数据
"""

import serial
import serial.tools.list_ports
import time

# ============ 配置区域 ============
# 修改为你的实际 COM 口号（从设备管理器或 find_com.ps1 获取）
PORT = "COM5"  # <-- 改成你的实际端口号！
BAUD = 9600    # 和你 STM32 代码中的波特率一致
# ==================================


def list_ports():
    """列出所有串口"""
    print("=" * 50)
    print("可用串口列表：")
    print("-" * 50)
    ports = serial.tools.list_ports.comports()
    for p in ports:
        print(f"  {p.device:8} - {p.description}")
    print("=" * 50)
    return ports


def test_connection(port_name, baud):
    """测试与 HC-05 的串口通信"""
    print(f"\n尝试打开 {port_name} @ {baud} baud...")
    
    try:
        ser = serial.Serial(port_name, baud, timeout=1)
        print(f"✅ 成功打开串口 {port_name}")
        print(f"   等待 STM32 自动上报数据（每 2 秒一次）...\n")
        
        # 先读 10 秒，看自动上报
        start = time.time()
        while time.time() - start < 10:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                text = data.decode('utf-8', errors='replace').strip()
                print(f"[{time.strftime('%H:%M:%S')}] 📥 收到: {text}")
            time.sleep(0.1)
        
        # 发送测试命令
        print("\n" + "=" * 50)
        print("发送测试命令：")
        print("=" * 50)
        
        commands = ["STEP", "TEMP", "TIME", "DEBUG"]
        for cmd in commands:
            print(f"\n📤 发送: {cmd}")
            ser.write(f"{cmd}\r\n".encode())
            time.sleep(0.5)
            
            if ser.in_waiting > 0:
                resp = ser.read(ser.in_waiting).decode('utf-8', errors='replace').strip()
                print(f"📥 回复: {resp}")
            else:
                print("📥 暂无回复（可能命令未被执行或模块缓冲中）")
        
        ser.close()
        print("\n✅ 测试完成，串口已关闭")
        
    except serial.SerialException as e:
        print(f"❌ 串口错误: {e}")
        print("提示:")
        print("  - 确认 COM 口号正确")
        print("  - 确认 HC-05 已在 Windows 蓝牙设置中配对")
        print("  - 确认没有其他程序占用该串口")


def main():
    print("=" * 50)
    print("  HC-05 串口通信测试工具")
    print("=" * 50)
    print()
    
    ports = list_ports()
    
    # 自动检测是否有用户配置的端口
    port_exists = any(p.device == PORT for p in ports)
    
    if not port_exists:
        print(f"\n⚠️  警告: {PORT} 不在当前可用串口列表中！")
        user_port = input("请输入正确的 COM 口号 (如 COM5): ").strip()
        if user_port:
            global PORT
            PORT = user_port
    
    test_connection(PORT, BAUD)


if __name__ == "__main__":
    main()
