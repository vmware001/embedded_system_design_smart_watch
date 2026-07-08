#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BLE 诊断工具 - 用于排查为什么收不到数据
"""

import asyncio
import sys
from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError

WRITE_UUID = "0000ffe2-0000-1000-8000-00805f9b34fb"
READ_UUID  = "0000ffe1-0000-1000-8000-00805f9b34fb"

async def diag_device(addr):
    print(f"\n{'='*60}")
    print(f"🔬 诊断模式: {addr}")
    print(f"{'='*60}\n")
    
    async with BleakClient(addr) as client:
        print(f"✅ 已连接")
        print(f"   MTU: {client.mtu_size if hasattr(client, 'mtu_size') else 'unknown'}")
        
        # 列出所有服务和特性
        print("\n📋 服务与特性列表:")
        for service in client.services:
            print(f"\n  服务: {service.uuid}")
            for char in service.characteristics:
                props = ','.join(char.properties)
                mark = ""
                if char.uuid.lower() == READ_UUID:
                    mark = " <-- [读取/通知]"
                elif char.uuid.lower() == WRITE_UUID:
                    mark = " <-- [写入]"
                print(f"    特性: {char.uuid}")
                print(f"      属性: {props}{mark}")
        
        # 尝试直接读取 FFE1
        print(f"\n📖 直接读取 FFE1...")
        try:
            data = await client.read_gatt_char(READ_UUID)
            print(f"   读取成功: {len(data)} bytes")
            print(f"   HEX: {data.hex()}")
            print(f"   ASCII: {data!r}")
        except Exception as e:
            print(f"   读取失败: {e}")
        
        # 尝试订阅 notify
        print(f"\n📡 尝试订阅 FFE1 notify...")
        
        rx_count = 0
        def on_notify(sender, data):
            nonlocal rx_count
            rx_count += 1
            print(f"\n🎉 [NOTIFY #{rx_count}] {len(data)} bytes from {sender}")
            print(f"   HEX: {data[:50].hex()}")
            print(f"   ASCII: {data!r}")
        
        try:
            await client.start_notify(READ_UUID, on_notify)
            print("   ✅ 订阅成功！等待 15 秒数据...")
            print("   （请晃动 STM32 或等待自动上报）\n")
            await asyncio.sleep(15)
            
            if rx_count == 0:
                print("\n⚠️ 15 秒内未收到任何 notify！")
                print("   可能原因:")
                print("   1. STM32 的 BleTask 没有在发数据")
                print("   2. 数据被手机拦截了（手机真的断开了吗？）")
                print("   3. Windows BLE 驱动问题")
                print("   4. 特性 UUID 不匹配")
            else:
                print(f"\n✅ 共收到 {rx_count} 次 notify")
            
            await client.stop_notify(READ_UUID)
        except Exception as e:
            print(f"   ❌ 订阅失败: {e}")

async def main():
    print("🔍 扫描 BLE 设备...")
    devices = await BleakScanner.discover(timeout=5)
    
    candidates = [d for d in devices if d.name and any(k in d.name.upper() for k in ["HC", "JDY", "BT", "BLE"])]
    
    if not candidates:
        print("❌ 没找到设备")
        return
    
    for i, d in enumerate(candidates):
        print(f"  [{i}] {d.name} - {d.address}")
    
    if len(candidates) == 1:
        await diag_device(candidates[0].address)
    else:
        idx = int(input("选择设备编号: "))
        await diag_device(candidates[idx].address)

asyncio.run(main())
