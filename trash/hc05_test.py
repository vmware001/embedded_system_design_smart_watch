#!/usr/bin/env python3
"""
HC-05 蓝牙模块测试脚本
用法:
  1. 安装 pyserial:  pip install pyserial
  2. 运行: python hc05_test.py

支持两种测试方式:
  A. USB-TTL 直连 HC-05（最准确）
  B. 通过 STM32 透传（如果 HC-05 已焊在主板上）
"""

import serial
import serial.tools.list_ports
import time
import sys


def list_ports():
    """列出所有可用串口"""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("未找到任何串口设备！")
        print("请检查：")
        print("  - USB-TTL/CH340 驱动是否安装")
        print("  - HC-05 是否已配对（Windows 蓝牙设置里查看 COM 口）")
        return None

    print("\n可用串口列表：")
    print("-" * 50)
    for i, p in enumerate(ports):
        print(f"  [{i}] {p.device} - {p.description}")
    print("-" * 50)
    return ports


def select_port(ports):
    """让用户选择串口"""
    while True:
        try:
            idx = input(f"\n选择串口编号 [0-{len(ports)-1}]: ").strip()
            idx = int(idx)
            if 0 <= idx < len(ports):
                return ports[idx].device
            print("编号无效，请重新输入")
        except ValueError:
            print("请输入数字")


def at_test(port_name, baudrate=38400, timeout=2):
    """
    进入 AT 模式测试
    需要：上电前按住 HC-05 的 KEY/EN 按钮（或拉高到 3.3V）
    """
    print(f"\n{'='*50}")
    print(f"尝试 AT 模式: {port_name} @ {baudrate} baud")
    print(f"{'='*50}")
    print("【操作提示】")
    print("  1. 按住 HC-05 上的 KEY/EN 按钮不放")
    print("  2. 给 HC-05 上电（或按 STM32 复位键）")
    print("  3. 看到 HC-05 LED 慢闪（约 2 秒一次）后松开按钮")
    print("  4. 按回车继续...")
    input()

    try:
        with serial.Serial(port_name, baudrate, timeout=timeout) as ser:
            print(f"已打开串口 {port_name}")

            # 发送基础 AT 测试
            print("\n[1/3] 发送 AT...")
            ser.write(b"AT\r\n")
            time.sleep(0.5)
            resp = ser.read(ser.in_waiting).decode('utf-8', errors='ignore').strip()
            print(f"  回复: '{resp}'")
            if "OK" in resp:
                print("  ✓ AT 模式连接成功！")
            else:
                print("  ✗ 无 OK 回复，可能未进入 AT 模式或波特率不对")
                return None

            # 查询版本
            print("\n[2/3] 发送 AT+VERSION? ...")
            ser.write(b"AT+VERSION?\r\n")
            time.sleep(0.5)
            resp = ser.read(ser.in_waiting).decode('utf-8', errors='ignore').strip()
            print(f"  回复: '{resp}'")

            # 提取版本号
            version = None
            for line in resp.split('\n'):
                line = line.strip()
                if line.startswith('+VERSION:'):
                    version = line.replace('+VERSION:', '').strip()
                    break
                elif line and line != 'OK':
                    version = line

            if version:
                print(f"  ★ 固件版本: {version}")
                if '2.0' in version:
                    print("  ★★★ 这是 v2.0 固件，无缓冲 bug，恭喜！★★★")
                elif '3.0' in version or '4.0' in version:
                    print("  ★★★ 这是 v3.0/v4.0 固件，存在 230 字节缓冲 bug！★★★")
                else:
                    print("  ? 未知版本，建议记录后搜索")
            else:
                print("  ! 无法解析版本号")
                version = "unknown"

            # 查询当前 UART 参数
            print("\n[3/3] 发送 AT+UART? ...")
            ser.write(b"AT+UART?\r\n")
            time.sleep(0.5)
            resp = ser.read(ser.in_waiting).decode('utf-8', errors='ignore').strip()
            print(f"  回复: '{resp}'")

            return version

    except serial.SerialException as e:
        print(f"串口错误: {e}")
        return None


def data_mode_test(port_name, baudrate=9600, timeout=1):
    """
    数据模式测试：连接已配对的 HC-05
    不需要按按钮，直接打开串口收发
    """
    print(f"\n{'='*50}")
    print(f"数据模式测试: {port_name} @ {baudrate} baud")
    print(f"{'='*50}")
    print("【操作提示】")
    print("  1. 确保 HC-05 LED 快闪（已配对/连接状态）")
    print("  2. 按回车开始测试...")
    input()

    try:
        with serial.Serial(port_name, baudrate, timeout=timeout) as ser:
            print(f"已打开串口 {port_name}")
            print("等待接收 STM32 自动上报数据（每 2 秒一次）...")
            print("按 Ctrl+C 退出\n")

            start = time.time()
            while time.time() - start < 30:  # 测试 30 秒
                if ser.in_waiting > 0:
                    data = ser.read(ser.in_waiting)
                    text = data.decode('utf-8', errors='replace')
                    for line in text.split('\n'):
                        line = line.strip()
                        if line:
                            print(f"[{time.strftime('%H:%M:%S')}] 收到: {line}")

                # 每 5 秒发送一个测试命令
                if int(time.time() - start) % 5 == 0:
                    cmd = b"STEP\r\n"
                    ser.write(cmd)
                    print(f"[{time.strftime('%H:%M:%S')}] 发送: STEP")
                    time.sleep(1)

            print("\n30 秒测试结束")

    except KeyboardInterrupt:
        print("\n用户中断")
    except serial.SerialException as e:
        print(f"串口错误: {e}")
        print("提示: 如果是 Windows 蓝牙配对生成的 COM 口，")
        print("      请确保选的是【传出端口】(Outgoing)，而非传入端口")


def main():
    print("="*60)
    print(" HC-05 蓝牙模块诊断工具")
    print("="*60)
    print()
    print("本脚本将帮助你：")
    print("  1. 查询 HC-05 固件版本（确认是否有 230 字节缓冲 bug）")
    print("  2. 测试数据模式双向通信")
    print()

    # 检查 pyserial
    try:
        import serial
    except ImportError:
        print("错误：未安装 pyserial")
        print("请先运行:  pip install pyserial")
        sys.exit(1)

    # 列出串口
    ports = list_ports()
    if not ports:
        print("\n如果没有 USB-TTL 模块，也可以通过 STM32 透传查询版本")
        print("请先把下面这段代码烧录到 STM32，然后按提示操作")
        print()
        print("="*60)
        print("【STM32 透传方案】")
        print("="*60)
        print("我已在固件中添加了 AT 透传功能，")
        print("蓝牙发送 'ATVER' 即可触发版本查询")
        sys.exit(0)

    port_name = select_port(ports)

    # 主菜单
    while True:
        print("\n" + "="*50)
        print("选择测试模式:")
        print("  [1] AT 模式 - 查询固件版本（需按住 KEY 上电）")
        print("  [2] 数据模式 - 收发数据测试（需 Windows 配对）")
        print("  [q] 退出")
        choice = input("选择: ").strip().lower()

        if choice == '1':
            # 尝试常见 AT 波特率
            for baud in [38400, 9600, 115200]:
                result = at_test(port_name, baudrate=baud)
                if result:
                    break
                print(f"\n波特率 {baud} 失败，尝试下一个...")

        elif choice == '2':
            # 尝试数据模式
            for baud in [9600, 115200, 38400]:
                print(f"\n尝试数据模式波特率 {baud}...")
                data_mode_test(port_name, baudrate=baud)
                retry = input("\n是否尝试其他波特率? [y/n]: ").strip().lower()
                if retry != 'y':
                    break

        elif choice == 'q':
            print("再见！")
            break


if __name__ == "__main__":
    main()
