#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AT 模式测试 - 尝试通过 AT 命令开启透传/Notify

使用方法:
  1. EN 接 3.3V（进入 AT 模式，慢闪）
  2. 运行: python ble_at_test.py
  3. 如果收到 OK，尝试 AT 命令
"""

import asyncio
import sys
from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError

WRITE_UUID = "0000ffe2-0000-1000-8000-00805f9b34fb"
READ_UUID  = "0000ffe1-0000-1000-8000-00805f9b34fb"

async def scan():
    print("🔍 扫描 BLE 设备...")
    devices = await BleakScanner.discover(timeout=5)
    for d in devices:
        if d.name and any(k in d.name.upper() for k in ["HC", "JDY", "BT", "BLE", "NSDDD"]):
            print(f"  ✅ {d.name} - {d.address}")
            return d.address
    print("❌ 没找到模块")
    return None

async def at_test(address):
    print(f"\n🔗 连接 {address}...")
    async with BleakClient(address) as client:
        print("✅ 已连接")
        
        received = []
        def handler(sender, data):
            received.append(data)
            try:
                text = data.decode('utf-8').strip()
                print(f"  📥 收到: {text}")
            except:
                print(f"  📥 收到(HEX): {data.hex()}")
        
        # 订阅 Notify（AT 模式下 Notify 通常可用）
        try:
            await client.start_notify(READ_UUID, handler)
            print("✅ Notify 订阅成功")
        except Exception as e:
            print(f"⚠️ Notify 订阅: {e}")
        
        # 常见 AT 命令列表
        at_commands = [
            b"AT\r\n",                    # 测试
            b"AT+VERSION\r\n",           # 查询版本
            b"AT+BAUD\r\n",              # 查询波特率
            b"AT+MODE?\r\n",             # 查询工作模式
            b"AT+MODE=1\r\n",            # 尝试设置透传模式
            b"AT+NOTIFY=1\r\n",          # 尝试开启 Notify
            b"AT+UART?\r\n",             # 查询 UART 参数
            b"AT+UART=9600,0,0\r\n",    # 设置波特率 9600
        ]
        
        print("\n🧪 开始发送 AT 命令...")
        for cmd in at_commands:
            cmd_str = cmd.decode().strip()
            print(f"\n📤 发送: {cmd_str}")
            received.clear()
            
            try:
                await client.write_gatt_char(WRITE_UUID, cmd)
                await asyncio.sleep(1)  # 等回复
                
                if not received:
                    print("  ⚠️ 无回复")
            except Exception as e:
                print(f"  ❌ 发送失败: {e}")
                break
        
        try:
            await client.stop_notify(READ_UUID)
        except:
            pass
        
        print("\n" + "="*50)
        print("测试结束。如果收到 OK，说明 AT 模式可用。")
        print("如果没收到任何回复，说明模块在当前波特率下不响应 AT 命令。")
        print("="*50)

async def main():
    addr = await scan()
    if addr:
        await at_test(addr)

if __name__ == "__main__":
    asyncio.run(main())
