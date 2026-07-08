#!/usr/bin/env python3
"""
HC-05 AT 模式查询工具 - 通过 CH340 USB-TTL 直连
接线:
  CH340 RX  -> HC-05 TX
  CH340 TX  -> HC-05 RX
  CH340 GND -> HC-05 GND
  HC-05 EN  -> 3.3V (拉高进入 AT 模式)
  HC-05 VCC -> 5V 或 3.3V (看模块标注)

AT 模式波特率固定: 38400

用法:
    python hc05_at_check.py
"""

import serial
import serial.tools.list_ports
import time
import sys

AT_BAUD = 38400  # AT 模式固定波特率


def find_ch340_port():
    """查找 CH340 对应的 COM 口"""
    ports = serial.tools.list_ports.comports()
    ch340_ports = []
    
    print("=" * 60)
    print("串口设备列表")
    print("-" * 60)
    for i, p in enumerate(ports):
        desc = p.description.lower()
        is_ch340 = 'ch340' in desc or 'usb-serial' in desc or 'usb-serial ch' in desc
        is_usb_ttl = 'usb' in desc and 'serial' in desc
        marker = "  <-- 可能是 CH340" if is_ch340 or is_usb_ttl else ""
        print(f"[{i}] {p.device:8} | {p.description:40}{marker}")
        if is_ch340 or is_usb_ttl:
            ch340_ports.append(p.device)
    print("-" * 60)
    return ch340_ports


def send_at_command(ser, cmd, wait=1.0):
    """发送 AT 命令并读取回复"""
    print(f"\n📤 发送: {cmd.strip()}")
    ser.write(cmd.encode())
    time.sleep(wait)
    
    data = ser.read(ser.in_waiting)
    if not data:
        print("⏳ 无回复")
        return None
    
    # 尝试多种解码
    text = data.decode('utf-8', errors='replace').strip()
    print(f"📥 回复: {text}")
    
    # 也显示 HEX
    hex_str = ' '.join(f'{b:02X}' for b in data)
    print(f"📥 HEX: {hex_str}")
    
    return text


def check_at_mode(port_name):
    """测试 AT 模式连接"""
    print(f"\n{'='*60}")
    print(f"尝试 AT 模式: {port_name} @ {AT_BAUD}")
    print(f"{'='*60}")
    print("【请确认】")
    print("  - HC-05 EN 已接 3.3V")
    print("  - HC-05 已上电（LED 慢闪 2秒周期）")
    print("  - CH340 已插电脑")
    print()
    
    try:
        ser = serial.Serial(port_name, AT_BAUD, timeout=2)
        print(f"✅ 已打开串口 {port_name}")
        
        # 1. 基础 AT 测试
        print("\n【1/4】基础 AT 测试...")
        resp = send_at_command(ser, "AT\r\n", 1.0)
        if resp and "OK" in resp:
            print("✅ AT 模式连接成功！")
        else:
            print("⚠️ 未收到 OK，尝试其他波特率...")
            for baud in [9600, 115200]:
                ser.close()
                ser = serial.Serial(port_name, baud, timeout=2)
                resp = send_at_command(ser, "AT\r\n", 1.0)
                if resp and "OK" in resp:
                    print(f"✅ AT 模式在 {baud} 波特率下成功！")
                    break
            else:
                print("❌ 所有常见波特率都失败")
                print("   可能原因:")
                print("   - EN 未接 3.3V（上电前必须接）")
                print("   - TX/RX 接反了")
                print("   - HC-05 未上电")
                ser.close()
                return False
        
        # 2. 查询版本
        print("\n【2/4】查询固件版本...")
        resp = send_at_command(ser, "AT+VERSION?\r\n", 1.0)
        if resp:
            if "VERSION" in resp or "OK" in resp:
                print("✅ 版本查询成功")
            else:
                print("⚠️ 回复异常，可能是 v3.0/v4.0 固件不兼容此命令")
        
        # 3. 查询名称
        print("\n【3/4】查询蓝牙名称...")
        send_at_command(ser, "AT+NAME?\r\n", 1.0)
        
        # 4. 查询当前 UART 参数
        print("\n【4/4】查询当前 UART 参数...")
        send_at_command(ser, "AT+UART?\r\n", 1.0)
        
        ser.close()
        print("\n✅ 测试完成")
        return True
        
    except serial.SerialException as e:
        print(f"❌ 串口错误: {e}")
        return False


def main():
    print("=" * 60)
    print("  HC-05 AT 模式查询工具")
    print("  (通过 CH340 USB-TTL 直连)")
    print("=" * 60)
    print()
    print("接线确认:")
    print("  CH340 RX  -> HC-05 TX")
    print("  CH340 TX  -> HC-05 RX")
    print("  CH340 GND -> HC-05 GND")
    print("  HC-05 EN  -> 3.3V (AT 模式)")
    print("  HC-05 VCC -> 5V/3.3V")
    print()
    print("注意:")
    print("  EN 拉高必须在 HC-05 上电前或上电同时完成！")
    print("  上电后 LED 应为慢闪（约2秒一次），不是快闪")
    print()
    
    ch340_ports = find_ch340_port()
    
    if not ch340_ports:
        print("未找到 CH340 串口！")
        print("请检查：")
        print("  - CH340 驱动是否安装（设备管理器 → 端口）")
        print("  - CH340 是否已插电脑")
        port = input("\n手动输入 COM 口号: ").strip()
        if port:
            check_at_mode(port)
    else:
        for port in ch340_ports:
            if check_at_mode(port):
                break

    print("\n" + "=" * 60)
    print("【固件版本判断】")
    print("=" * 60)
    print("如果 AT 命令有响应，但版本查询返回乱码或无文档：")
    print("  → 很可能是 v3.0/v4.0 固件（有 230 字节缓冲 bug）")
    print("  → 建议更换为 JDY-31 或 HC-04 模块")
    print()
    print("如果 AT 命令完全无响应：")
    print("  → EN 可能未正确拉高，或接线错误")
    print("  → 尝试 TX/RX 对调后再试")
    print("=" * 60)

    input("\n按回车退出...")


if __name__ == "__main__":
    main()
