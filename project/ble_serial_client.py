#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
STM32 Smart Watch BLE 通信工具

基于 bleak 库，通过 BLE GATT 特性与模块通信：
  - FFE2 (write): 手机/电脑 → 模块 → STM32 UART
  - FFE1 (notify): STM32 UART → 模块 → 手机/电脑

使用方法：
  python ble_serial_client.py --scan           # 扫描设备
  python ble_serial_client.py                  # 自动扫描+连接+交互
  python ble_serial_client.py --addr XX:XX:XX:XX:XX:XX  # 指定地址

注意：BLE 模块同一时间通常只能连接一个设备。如果手机还连着，电脑会连上但收不到数据！

依赖：
  pip install bleak
"""

import asyncio
import sys
import argparse
import time
from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError

# GATT 特性 UUID
WRITE_CHAR_UUID = "0000ffe2-0000-1000-8000-00805f9b34fb"  # 写（电脑→STM32）
READ_CHAR_UUID  = "0000ffe1-0000-1000-8000-00805f9b34fb"  # 读/通知（STM32→电脑）

# 已知设备地址（扫描后自动保存）
device_address = None

# 全局状态
step_count = 0
temperature = 0.0
rx_count = 0  # 收到 notify 的次数
last_rx_time = 0


def _get_rssi(device):
    """兼容不同 bleak 版本获取 RSSI"""
    return getattr(device, 'rssi', None)


async def scan_devices(timeout=5):
    """扫描 BLE 设备，找到手表模块"""
    print("🔍 扫描 BLE 设备...")
    devices = await BleakScanner.discover(timeout=timeout)
    
    candidates = []
    for d in devices:
        if d.name:
            name_upper = d.name.upper()
            # 匹配常见 BLE 模块名称
            if any(k in name_upper for k in ["HC", "JDY", "BT", "BLE", "NSDDD", "WATCH", "SMART"]):
                candidates.append(d)
                rssi = _get_rssi(d)
                rssi_str = f"  (RSSI: {rssi}dBm)" if rssi is not None else ""
                print(f"  ✅ {d.name} - {d.address}{rssi_str}")
    
    if not candidates:
        print("❌ 没找到匹配的 BLE 模块")
        print("   请确认模块已上电且未连接其他设备")
        return None
    
    # 返回信号最强的（兼容无 rssi 的情况）
    try:
        best = max(candidates, key=lambda d: _get_rssi(d) or -100)
    except:
        best = candidates[0]
    print(f"\n🎯 自动选择: {best.name} ({best.address})")
    return best.address


def notification_handler(sender, data):
    """
    处理从 STM32 发来的通知数据（FFE1 notify）
    """
    global step_count, temperature, rx_count, last_rx_time
    
    rx_count += 1
    last_rx_time = time.time()
    
    # 调试：显示原始数据信息
    # print(f"[DEBUG] RX raw: {len(data)} bytes, hex: {data[:20].hex()}")
    
    # 过滤空数据（全部是 0x00）
    if not data or set(data) == {0}:
        return
    
    # 去掉 0x00 填充字节
    clean = data.replace(b'\x00', b'')
    if not clean:
        return
    
    # 尝试解码
    try:
        text = clean.decode('utf-8', errors='ignore').strip()
    except:
        text = clean.hex()
    
    if not text:
        return
    
    # 解析自动上报数据
    if "STEPS:" in text and "TEMP:" in text:
        try:
            parts = text.split()
            for part in parts:
                if part.startswith("STEPS:"):
                    step_count = int(part.split(":")[1])
                elif part.startswith("TEMP:"):
                    temperature = float(part.split(":")[1])
            print(f"📊 [自动上报] 步数: {step_count} | 温度: {temperature}°C")
        except (ValueError, IndexError):
            pass
    
    # 解析 DEBUG 消息
    elif "STEP!" in text:
        print(f"🚶 [DEBUG] {text}")
    
    # 命令回显或其他数据
    else:
        print(f"📥 [RX] {text}")


async def monitor_rx_task(timeout=10):
    """
    后台任务：监控是否收到数据
    如果 timeout 秒内没有收到任何 notify，提示用户可能手机还连着
    """
    global rx_count
    start_rx = rx_count
    start_time = time.time()
    
    await asyncio.sleep(timeout)
    
    if rx_count == start_rx:
        print("\n⚠️ 警告: 10 秒内未收到任何数据！")
        print("   可能原因:")
        print("   1. 📱 手机还连着 HC-05 → 请先断开手机蓝牙")
        print("   2. 🔌 STM32 未发送数据 → 检查固件是否正常")
        print("   3. 📡 连接不稳定 → 尝试重新运行脚本")


async def interactive_session(client):
    """
    交互会话：持续接收通知 + 发送命令
    """
    global device_address
    
    # 启用通知（订阅 FFE1）
    print("📡 启用数据接收...")
    try:
        await client.start_notify(READ_CHAR_UUID, notification_handler)
        print("✅ 已订阅通知")
        print("   等待 STM32 自动上报数据...（每 2 秒一次）\n")
    except Exception as e:
        print(f"❌ 订阅通知失败: {e}")
        print("   可能手机还占用了 notify，请先断开手机")
        return
    
    # 启动监控任务（10 秒后检查是否收到数据）
    monitor = asyncio.create_task(monitor_rx_task(10))
    
    print("="*50)
    print("  STM32 BLE 控制台")
    print("="*50)
    print("命令: step | reset | temp | sw | status | quit")
    print("="*50 + "\n")
    
    while True:
        try:
            cmd = await asyncio.to_thread(input, "> ")
            cmd = cmd.strip()
            if not cmd:
                continue
            
            if cmd in ('quit', 'q', 'exit'):
                break
            
            elif cmd == 'step':
                await send_ble_data(client, "STEP")
            
            elif cmd == 'reset':
                await send_ble_data(client, "RESET")
                print("🔄 已发送重置")
            
            elif cmd == 'temp':
                await send_ble_data(client, "TEMP")
            
            elif cmd == 'sw':
                await send_ble_data(client, "SW")
            
            elif cmd == 'status':
                print(f"📊 步数: {step_count} | 温度: {temperature}°C")
                print(f"📨 收到数据包: {rx_count}")
                print(f"🔌 连接: {client.is_connected}")
            
            else:
                await send_ble_data(client, cmd)
        
        except KeyboardInterrupt:
            print("\n👋 再见!")
            break
    
    # 取消监控任务
    monitor.cancel()
    
    # 停止通知
    try:
        await client.stop_notify(READ_CHAR_UUID)
    except:
        pass


async def send_ble_data(client, message, add_crlf=True):
    """
    通过 BLE 发送数据到 STM32
    """
    if not client.is_connected:
        print("❌ 未连接")
        return False
    
    # 构造数据
    if add_crlf and not message.endswith('\r\n'):
        message += '\r\n'
    data = message.encode('utf-8')
    
    try:
        # 使用 response=False 发送（Write Command，不需要回复）
        await client.write_gatt_char(WRITE_CHAR_UUID, data, response=False)
        print(f"📤 > {message.strip()}")
        return True
    except BleakError as e:
        print(f"❌ 发送失败: {e}")
        return False


async def main():
    parser = argparse.ArgumentParser(description='STM32 BLE 通信工具')
    parser.add_argument('--scan', action='store_true', help='仅扫描设备')
    parser.add_argument('--addr', help='指定设备 MAC 地址')
    parser.add_argument('--no-crlf', action='store_true', help='不自动追加 \\r\\n')
    args = parser.parse_args()
    
    # 检查 bleak 是否安装
    try:
        import bleak
    except ImportError:
        print("❌ 请先安装 bleak:  pip install bleak")
        sys.exit(1)
    
    global device_address
    
    # 扫描模式
    if args.scan:
        await scan_devices()
        return
    
    # 确定设备地址
    addr = args.addr or device_address
    if not addr:
        addr = await scan_devices()
        if not addr:
            print("❌ 未找到设备，退出")
            sys.exit(1)
        device_address = addr
    
    print(f"\n🔗 正在连接 {addr}...")
    
    try:
        async with BleakClient(addr) as client:
            print(f"✅ 已连接 {addr}")
            
            # 进入交互会话（去掉 is_paired 检查，避免报错）
            await interactive_session(client)
            
    except BleakError as e:
        print(f"❌ 连接失败: {e}")
        print("💡 建议:")
        print("   1. 确保模块已上电")
        print("   2. 📱 断开手机等其他设备的连接（最重要！）")
        print("   3. 在 Windows 蓝牙设置中移除设备后重新配对")
        print("   4. 尝试指定地址: --addr XX:XX:XX:XX:XX:XX")
        sys.exit(1)
    except Exception as e:
        print(f"❌ 错误: {e}")
        sys.exit(1)


if __name__ == "__main__":
    asyncio.run(main())
