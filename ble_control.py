#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BLE 单向控制脚本 - 默认使用 FFE2（手机→模块→UART）
运行方式:
  python ble_control.py STEP                  # 发文本 STEP\r\n
  python ble_control.py SYNC,14:30:00         # 同步时间
  python ble_control.py --raw "53 54 45 50"     # 发原始 HEX 字节
  python ble_control.py --scan                  # 扫描设备
"""

import asyncio
import sys
import argparse
from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError

# FFE2: write (phone -> module -> STM32 UART)
# FFE1: notify (module -> phone), 一般不要用来写
WRITE_CHAR_UUID = "0000ffe2-0000-1000-8000-00805f9b34fb"
READ_CHAR_UUID  = "0000ffe1-0000-1000-8000-00805f9b34fb"

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
        print("❌ 没找到模块")
        return None
    return candidates[0].address


def parse_raw_hex(cmd: str) -> bytes:
    """把空格分隔的 hex 字符串转成 bytes，例如 '53 54 45 50' -> b'STEP'"""
    parts = cmd.strip().split()
    return bytes(int(p, 16) for p in parts)


async def send_command(address, cmd, raw=False, no_crlf=False, use_ffe1=False):
    async with BleakClient(address) as client:
        print(f"🔗 已连接 {address}")

        # Windows 强制配对
        try:
            if not await client.is_paired():
                print("🔐 正在配对...")
                await client.pair()
                print("✅ 配对完成")
        except Exception as e:
            print(f"⚠️ 配对跳过: {e}")

        # 构造要发送的字节
        if raw:
            data = parse_raw_hex(cmd)
        else:
            data = cmd.encode('utf-8')
            if not no_crlf and not data.endswith(b'\r\n'):
                data += b'\r\n'

        print(f"📤 准备发送 {len(data)} 字节")
        print(f"   发送 HEX: {data.hex()}")
        print(f"   发送 ASCII: {data!r}")

        # 默认写 FFE2；如果用 --ffe1 参数才写 FFE1
        char_uuid = READ_CHAR_UUID if use_ffe1 else WRITE_CHAR_UUID
        char_name = "FFE1" if use_ffe1 else "FFE2"
        print(f"   目标特征: {char_name} ({char_uuid})")

        try:
            await client.write_gatt_char(char_uuid, data)
            print(f"✅ 已发送 (via {char_name})")
            print("📺 请在 OLED 上查看结果")
        except BleakError as e:
            print(f"❌ 写入 {char_name} 失败: {e}")
            print("💡 建议: 在 Windows 设置 → 蓝牙 → 移除该设备 → 重新配对")


async def main():
    parser = argparse.ArgumentParser(description='BLE 单向控制手表')
    parser.add_argument('command', nargs='?', help='要发送的命令 (如 STEP, TEMP, SYNC,14:30:00)')
    parser.add_argument('--scan', action='store_true', help='仅扫描设备')
    parser.add_argument('--addr', help='指定设备地址')
    parser.add_argument('--raw', action='store_true', help='把 command 当空格分隔的 HEX 发送，例如 "53 54 45 50"')
    parser.add_argument('--no-crlf', action='store_true', help='不自动追加 \\r\\n')
    parser.add_argument('--ffe1', action='store_true', help='强制使用 FFE1 特征写（通常不建议）')
    args = parser.parse_args()

    try:
        import bleak
    except ImportError:
        print("❌ 请先安装: pip install bleak")
        sys.exit(1)

    global device_address

    if args.scan:
        await scan()
        return

    addr = args.addr or device_address
    if not addr:
        addr = await scan()
        if not addr:
            return
        device_address = addr

    if args.command:
        await send_command(addr, args.command, raw=args.raw, no_crlf=args.no_crlf, use_ffe1=args.ffe1)
    else:
        print("\n" + "="*50)
        print("交互模式 - 输入命令发送，Ctrl+C 退出")
        print("常用: STEP, TEMP, TIME, SYNC,14:30:00")
        print("      --raw 模式: 53 54 45 50")
        print("="*50)
        while True:
            try:
                cmd = input("\n📤 命令: ").strip()
                if not cmd:
                    continue
                await send_command(addr, cmd, raw=args.raw, no_crlf=args.no_crlf, use_ffe1=args.ffe1)
            except KeyboardInterrupt:
                print("\n👋 退出")
                break


if __name__ == "__main__":
    asyncio.run(main())
