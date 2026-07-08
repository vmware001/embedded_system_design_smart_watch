#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
终极 BLE 测试 - 尝试所有接收方式
"""

import asyncio
import sys
from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError

WRITE_UUID = "0000ffe2-0000-1000-8000-00805f9b34fb"
READ_UUID  = "0000ffe1-0000-1000-8000-00805f9b34fb"

device_address = None

async def scan(timeout=5):
    print("🔍 扫描 BLE 设备...")
    devices = await BleakScanner.discover(timeout=timeout)
    candidates = []
    for d in devices:
        if d.name and any(k in d.name.upper() for k in ["HC", "JDY", "BT", "BLE", "NSDDD"]):
            candidates.append(d)
            print(f"  ✅ {d.name} - {d.address}")
    if not candidates:
        return None
    return candidates[0].address

async def test_all(address):
    print(f"\n🔗 连接 {address}...")
    async with BleakClient(address) as client:
        print("✅ 已连接")
        
        # 打印服务
        print("\n📋 服务:")
        for service in client.services:
            for char in service.characteristics:
                print(f"  {char.uuid} [{', '.join(char.properties)}]")
        
        # 方式1: 尝试订阅 Notify
        print("\n[方式1] 订阅 Notify...")
        received = []
        def handler(sender, data):
            received.append(data)
            print(f"  📥 Notify 收到: {data.hex()} ({data})")
        try:
            await client.start_notify(READ_UUID, handler)
            print("  ✅ Notify 订阅成功")
            await asyncio.sleep(2)
            await client.stop_notify(READ_UUID)
        except Exception as e:
            print(f"  ❌ Notify 失败: {e}")
        
        # 方式2: 尝试直接 Read
        print("\n[方式2] 直接 Read 特征...")
        try:
            data = await client.read_gatt_char(READ_UUID)
            print(f"  ✅ Read 成功: {data.hex()} ({data})")
        except Exception as e:
            print(f"  ❌ Read 失败: {e}")
        
        # 方式3: 发送数据，再等待看有没有 Notify
        print("\n[方式3] 发送后等待 Notify...")
        try:
            await client.write_gatt_char(WRITE_UUID, b"STEP\r\n")
            print("  📤 已发送 STEP")
            await asyncio.sleep(3)
            if received:
                print(f"  ✅ 收到 {len(received)} 条 Notify 回复")
            else:
                print("  ❌ 未收到任何回复")
        except Exception as e:
            print(f"  ❌ 发送失败: {e}")

async def main():
    addr = await scan()
    if addr:
        await test_all(addr)
        print("\n" + "="*50)
        print("结论:")
        print("  如果 Notify 和 Read 都失败 → 模块不支持上行")
        print("  如果 Notify 成功但无数据 → 模块可能不转发 UART")
        print("="*50)

if __name__ == "__main__":
    asyncio.run(main())
