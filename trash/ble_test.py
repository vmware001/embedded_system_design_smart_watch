#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BLE 透传测试脚本 - 用于 HC-05 兼容版(BLE模式) 测试
需要先安装 bleak: pip install bleak

运行方式:
  python ble_test.py
  python ble_test.py --auto  (自动选择第一个HC设备)
  python ble_test.py --addr XX:XX:XX:XX:XX:XX  (直接连接指定地址)
"""

import asyncio
import sys
import argparse
from bleak import BleakClient, BleakScanner

# 常见 BLE 透传模块的 UUID (写入和读取分开)
# 根据用户设备：写入用 ffe2，读取用 ffe1
WRITE_CHAR_UUID = "0000ffe2-0000-1000-8000-00805f9b34fb"
READ_CHAR_UUID  = "0000ffe1-0000-1000-8000-00805f9b34fb"


async def scan_devices(timeout=5):
    """扫描附近的 BLE 设备"""
    print("🔍 正在扫描 BLE 设备...")
    devices = await BleakScanner.discover(timeout=timeout)
    
    candidates = []
    for d in devices:
        if d.name:
            # 匹配常见蓝牙模块名称
            if any(key in d.name.upper() for key in ["HC", "JDY", "BT", "BLE", "NSDDD"]):
                candidates.append(d)
                print(f"  [候选] {d.name} - {d.address}")
            else:
                print(f"  [其他] {d.name} - {d.address}")
    
    if not candidates:
        print("\n❌ 没有找到 BLE 模块，请确认：")
        print("   1. 模块已上电（LED 快闪/慢闪）")
        print("   2. 电脑蓝牙已开启并支持 BLE")
        return None
    
    return candidates

async def test_ble(address, auto_mode=False):
    """连接并测试 BLE 透传"""
    print(f"\n🔗 正在连接 {address}...")
    
    async with BleakClient(address) as client:
        print(f"✅ 已连接!")
        
        # 检查服务和特征（新版 bleak 使用 services 属性）
        services_list = list(client.services)
        print(f"\n📋 发现 {len(services_list)} 个服务:")
        has_notify = False
        for service in services_list:
            print(f"   Service: {service.uuid}")
            for char in service.characteristics:
                props = ", ".join(char.properties)
                print(f"     Char: {char.uuid} [{props}]")
                if "notify" in char.properties:
                    has_notify = True
        
        if not has_notify:
            print("\n⚠️ 警告: 该设备没有 Notify 特征，可能无法接收回复")
        
        # 启用 Notify
        print("\n🔔 正在订阅通知...")
        received_count = [0]
        
        def notify_handler(sender, data):
            received_count[0] += 1
            try:
                text = data.decode('utf-8')
                print(f"📥 收到[{received_count[0]}]: {text.strip()}")
            except:
                print(f"📥 收到[{received_count[0]}] (HEX): {data.hex()}")
        
        try:
            await client.start_notify(READ_CHAR_UUID, notify_handler)
            print(f"✅ 已订阅通知 (Notify) on {READ_CHAR_UUID}")
        except Exception as e:
            print(f"❌ 订阅失败: {e}")
            return
        
        # 交互式发送命令
        if not auto_mode:
            print("\n" + "="*50)
            print("交互模式 - 输入命令发送，输入 'q' 退出")
            print("常用命令: STEP, TEMP, TIME, SYNC,14:30:00")
            print("="*50)
            
            while True:
                try:
                    cmd = input("\n📤 发送命令: ").strip()
                    if cmd.lower() in ('q', 'quit', 'exit'):
                        break
                    if not cmd:
                        continue
                    
                    data = cmd.encode('utf-8') + b"\r\n"
                    # 新版 bleak 的 write_gatt_char 没有 response 参数
                    await client.write_gatt_char(WRITE_CHAR_UUID, data)
                    print(f"   已发送 ({len(data)} bytes)")
                    await asyncio.sleep(1)  # 等回复
                    
                except KeyboardInterrupt:
                    break
                except Exception as e:
                    print(f"❌ 发送错误: {e}")
        else:
            # 自动测试模式
            print("\n🧪 自动测试模式...")
            commands = [b"STEP\r\n", b"TEMP\r\n", b"TIME\r\n"]
            for cmd in commands:
                print(f"\n📤 发送: {cmd.decode().strip()}")
                await client.write_gatt_char(WRITE_CHAR_UUID, cmd)
                await asyncio.sleep(2)
            
            await asyncio.sleep(2)
            print(f"\n📊 共收到 {received_count[0]} 条回复")
            
            if received_count[0] == 0:
                print("⚠️ 没有收到任何回复，可能原因：")
                print("   1. STM32 的 UART TX 没有正确输出")
                print("   2. 模块没有将 UART 数据转发到 BLE")
                print("   3. 模块的 BLE Notify 功能未正确启用")
        
        await client.stop_notify(READ_CHAR_UUID)
        print("\n🔌 已断开通知")

async def main():
    parser = argparse.ArgumentParser(description='BLE 透传测试工具')
    parser.add_argument('--addr', help='直接连接指定地址 (如 65:E8:00:9F:EC:BD)')
    parser.add_argument('--auto', action='store_true', help='自动测试模式')
    parser.add_argument('--scan', action='store_true', help='仅扫描设备')
    args = parser.parse_args()
    
    print("=" * 60)
    print("BLE 透传测试工具")
    print("=" * 60)
    
    try:
        import bleak
    except ImportError:
        print("\n❌ 请先安装 bleak:")
        print("   pip install bleak")
        sys.exit(1)
    
    # 仅扫描模式
    if args.scan:
        await scan_devices()
        return
    
    # 直接连接指定地址
    if args.addr:
        await test_ble(args.addr, auto_mode=args.auto)
        return
    
    # 扫描并选择
    candidates = await scan_devices()
    if not candidates:
        return
    
    print(f"\n✅ 找到 {len(candidates)} 个候选设备")
    for i, d in enumerate(candidates):
        print(f"  [{i}] {d.name} - {d.address}")
    
    while True:
        try:
            if args.auto and len(candidates) > 0:
                idx = 0
                print(f"\n自动选择: {candidates[0].name}")
            else:
                idx = int(input("\n请选择设备编号: "))
            
            if 0 <= idx < len(candidates):
                await test_ble(candidates[idx].address, auto_mode=args.auto)
                break
        except ValueError:
            print("输入无效，请重新选择")

if __name__ == "__main__":
    asyncio.run(main())
