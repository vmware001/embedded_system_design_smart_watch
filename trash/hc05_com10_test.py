#!/usr/bin/env python3
"""HC-05 通信综合测试 - COM10 @ 115200"""
import serial
import time

print("=" * 50)
print("HC-05 通信测试")
print("端口: COM10, 波特率: 115200")
print("=" * 50)

try:
    ser = serial.Serial('COM10', 115200, timeout=0.5)
    print("COM10 已打开")
except Exception as e:
    print(f"打开失败: {e}")
    exit(1)

print("\n--- 测试1: 自动上报 (等5秒) ---")
for i in range(5):
    time.sleep(1)
    if ser.in_waiting:
        d = ser.read(ser.in_waiting)
        text = d.decode('utf-8', errors='replace').strip()
        print(f"  [{i+1}s] 自动收到: {text[:60]}")
    else:
        print(f"  [{i+1}s] 无数据")

print("\n--- 测试2: 发送 STEP ---")
ser.write(b"STEP\r\n")
time.sleep(1)
resp = ser.read(ser.in_waiting)
if resp:
    print(f"  回复: {resp.decode('utf-8','replace').strip()[:60]}")
    print(f"  HEX: {resp.hex()}")
else:
    print("  无回复")

print("\n--- 测试3: 发送 TEMP ---")
ser.write(b"TEMP\r\n")
time.sleep(1)
resp = ser.read(ser.in_waiting)
if resp:
    print(f"  回复: {resp.decode('utf-8','replace').strip()[:60]}")
else:
    print("  无回复")

print("\n--- 测试4: 发送 TIME ---")
ser.write(b"TIME\r\n")
time.sleep(1)
resp = ser.read(ser.in_waiting)
if resp:
    print(f"  回复: {resp.decode('utf-8','replace').strip()[:60]}")
else:
    print("  无回复")

ser.close()
print("\n测试完成")
