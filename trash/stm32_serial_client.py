#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
STM32 Smart Watch 蓝牙串口通信工具 (诊断增强版)

解决 Windows 蓝牙虚拟串口连接问题：
1. 自动检测传出(Outgoing)端口
2. 诊断连接失败原因
3. 支持手动指定端口

使用方法：
  python stm32_serial_client.py          # 自动检测
  python stm32_serial_client.py -p COM3  # 手动指定
  python stm32_serial_client.py --list   # 只列出串口
"""

import serial
import serial.tools.list_ports
import threading
import time
import sys
import argparse


class STM32SerialClient:
    def __init__(self, port=None, baudrate=115200, timeout=1):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser = None
        self.running = False
        self.rx_thread = None
        self.step_count = 0
        self.temperature = 0.0
        self.last_rx_data = ""
        
    def _list_all_ports(self):
        """列出所有串口的详细信息"""
        ports = serial.tools.list_ports.comports()
        print("\n📋 可用串口列表:")
        print("-" * 60)
        for i, port in enumerate(ports):
            print(f"  [{i}] {port.device}")
            print(f"      描述: {port.description}")
            print(f"      硬件ID: {port.hwid}")
            print(f"      制造商: {port.manufacturer or 'N/A'}")
            print()
        return ports
    
    def _find_candidate_ports(self):
        """
        找出可能的蓝牙传出端口
        Windows 蓝牙配对后通常有两个端口，传出端口才能发送数据
        """
        ports = serial.tools.list_ports.comports()
        candidates = []
        
        for port in ports:
            desc = port.description.lower()
            hwid = port.hwid.lower()
            
            # 蓝牙串口的特征
            is_bluetooth = any(k in desc for k in [
                '蓝牙', 'bluetooth', '标准串行', 'serial',
                'bt', 'spp', 'rfcomm'
            ])
            
            # 排除 obvious 的非蓝牙设备
            is_obviously_not_bt = any(k in desc for k in [
                'ch340', 'cp210', 'ftdi', 'pl2303', 'arduino',
                'modem', 'gps', 'phone'
            ])
            
            if is_bluetooth and not is_obviously_not_bt:
                candidates.append({
                    'device': port.device,
                    'desc': port.description,
                    'hwid': port.hwid,
                    'score': 100  # 高优先级
                })
            elif not is_obviously_not_bt:
                # 也加入其他串口，但优先级低
                candidates.append({
                    'device': port.device,
                    'desc': port.description,
                    'hwid': port.hwid,
                    'score': 50
                })
        
        # 按优先级排序
        candidates.sort(key=lambda x: x['score'], reverse=True)
        return candidates
    
    def _try_open(self, port, verbose=True):
        """尝试打开串口"""
        try:
            ser = serial.Serial(
                port=port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=self.timeout,
                write_timeout=1,
                rtscts=False,  # 蓝牙虚拟串口不需要流控
                dsrdtr=False
            )
            if verbose:
                print(f"  ✅ 成功打开")
            return ser
        except serial.SerialException as e:
            if verbose:
                print(f"  ❌ 打开失败: {e}")
            return None
    
    def _test_communication(self, ser, verbose=True):
        """
        测试串口双向通信
        方法：等待自动上报数据，或发送命令看是否有响应
        """
        try:
            # 清空缓冲区
            ser.reset_input_buffer()
            ser.reset_output_buffer()
            
            if verbose:
                print("  ⏳ 等待自动上报数据 (3秒)...", end=' ')
            
            # 等待 3 秒看是否有自动数据
            start = time.time()
            while time.time() - start < 3:
                if ser.in_waiting > 0:
                    data = ser.read(ser.in_waiting)
                    clean = data.replace(b'\x00', b'')
                    if clean:
                        if verbose:
                            try:
                                preview = clean.decode('utf-8', errors='ignore')[:50]
                            except:
                                preview = clean.hex()[:50]
                            print(f"收到数据! [{preview}...]")
                        return True
                time.sleep(0.1)
            
            if verbose:
                print("无自动数据")
                print("  📤 发送测试命令...", end=' ')
            
            # 尝试发送命令
            ser.write(b'STEP\r\n')
            ser.flush()
            
            # 等待响应
            start = time.time()
            while time.time() - start < 2:
                if ser.in_waiting > 0:
                    data = ser.read(ser.in_waiting)
                    clean = data.replace(b'\x00', b'')
                    if clean:
                        if verbose:
                            try:
                                preview = clean.decode('utf-8', errors='ignore')[:50]
                            except:
                                preview = clean.hex()[:50]
                            print(f"有响应! [{preview}...]")
                        return True
                time.sleep(0.1)
            
            if verbose:
                print("无响应")
            return False
            
        except Exception as e:
            if verbose:
                print(f"测试出错: {e}")
            return False
    
    def connect(self, auto_detect=True, verbose=True):
        """连接串口"""
        if self.port and not auto_detect:
            print(f"\n🔌 尝试连接指定端口: {self.port}")
            ser = self._try_open(self.port, verbose=verbose)
            if ser and self._test_communication(ser, verbose=verbose):
                self.ser = ser
                self._start_rx()
                print(f"\n✅ 已连接 {self.port} @ {self.baudrate} baud")
                return True
            else:
                if ser:
                    ser.close()
                print(f"\n❌ {self.port} 无法通信")
                return False
        
        # 自动检测模式
        print("\n🔍 自动检测蓝牙串口...")
        candidates = self._find_candidate_ports()
        
        if not candidates:
            print("❌ 没有找到任何串口！")
            print("\n可能原因:")
            print("  1. HC-05 未配对（Windows 蓝牙设置 → 添加设备）")
            print("  2. 蓝牙驱动未安装")
            print("  3. 需要先在手机断开 HC-05 连接")
            return False
        
        print(f"找到 {len(candidates)} 个候选串口\n")
        
        for cand in candidates:
            port = cand['device']
            print(f"🔄 测试 {port} ({cand['desc']})...")
            
            ser = self._try_open(port, verbose=verbose)
            if not ser:
                continue
            
            if self._test_communication(ser, verbose=verbose):
                self.ser = ser
                self.port = port
                self._start_rx()
                print(f"\n✅ 成功连接到 {port}")
                print(f"   描述: {cand['desc']}")
                return True
            else:
                ser.close()
        
        print("\n❌ 所有候选串口都无法通信")
        print("\n排查建议:")
        print("  1. 确保 HC-05 已配对（PIN: 1234 或 0000）")
        print("  2. 在设备管理器中查看传出(Outgoing) COM 口号")
        print("  3. 先用手机断开 HC-05，再运行脚本")
        print("  4. 尝试手动指定端口: python stm32_serial_client.py -p COMx")
        print("  5. 检查 STM32 是否已上电并运行固件")
        return False
    
    def _start_rx(self):
        self.running = True
        self.rx_thread = threading.Thread(target=self._rx_loop, daemon=True)
        self.rx_thread.start()
    
    def disconnect(self):
        self.running = False
        if self.rx_thread:
            self.rx_thread.join(timeout=1)
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("🔌 已断开")
    
    def _rx_loop(self):
        while self.running:
            try:
                if self.ser and self.ser.in_waiting > 0:
                    data = self.ser.read(self.ser.in_waiting)
                    if data:
                        self._process_rx(data)
            except Exception as e:
                if self.running:
                    print(f"接收错误: {e}")
            time.sleep(0.05)
    
    def _process_rx(self, data):
        clean = data.replace(b'\x00', b'')
        if not clean:
            return
        try:
            text = clean.decode('utf-8', errors='ignore')
        except:
            text = clean.decode('latin-1', errors='ignore')
        
        for line in text.split('\n'):
            line = line.strip()
            if not line:
                continue
            self.last_rx_data = line
            
            if "STEPS:" in line and "TEMP:" in line:
                try:
                    parts = line.split()
                    for part in parts:
                        if part.startswith("STEPS:"):
                            self.step_count = int(part.split(":")[1])
                        elif part.startswith("TEMP:"):
                            self.temperature = float(part.split(":")[1])
                    print(f"📊 [上报] 步数: {self.step_count} | 温度: {self.temperature}°C")
                except:
                    pass
            elif "STEP!" in line:
                print(f"🚶 [DEBUG] {line}")
            else:
                print(f"📥 {line}")
    
    def send(self, message):
        if not self.ser or not self.ser.is_open:
            print("❌ 未连接")
            return False
        if not message.endswith('\r\n'):
            message += '\r\n'
        try:
            self.ser.write(message.encode('utf-8'))
            self.ser.flush()
            print(f"📤 > {message.strip()}")
            return True
        except Exception as e:
            print(f"❌ 发送失败: {e}")
            return False


def interactive_mode(client):
    print("\n" + "="*50)
    print("  STM32 蓝牙串口控制台")
    print("="*50)
    print("命令: step | reset | temp | sw | status | quit")
    print("="*50 + "\n")
    
    while True:
        try:
            cmd = input("> ").strip()
            if not cmd:
                continue
            if cmd in ('quit', 'q', 'exit'):
                break
            elif cmd == 'step':
                client.send("STEP")
            elif cmd == 'reset':
                client.send("RESET")
                print("🔄 已发送重置")
            elif cmd == 'temp':
                client.send("TEMP")
            elif cmd == 'sw':
                client.send("SW")
            elif cmd == 'status':
                s = client.get_status()
                print(f"连接: {'✅' if s['connected'] else '❌'} {s['port']}")
                print(f"步数: {s['step_count']} | 温度: {s['temperature']}°C")
            else:
                client.send(cmd)
        except KeyboardInterrupt:
            break


def main():
    parser = argparse.ArgumentParser(description='STM32 蓝牙串口工具')
    parser.add_argument('-p', '--port', help='指定串口号')
    parser.add_argument('-b', '--baud', type=int, default=115200)
    parser.add_argument('--list', action='store_true', help='只列出串口')
    args = parser.parse_args()
    
    client = STM32SerialClient(port=args.port, baudrate=args.baud)
    
    if args.list:
        client._list_all_ports()
        return
    
    if not client.connect(auto_detect=(args.port is None)):
        sys.exit(1)
    
    time.sleep(0.5)
    try:
        interactive_mode(client)
    finally:
        client.disconnect()


if __name__ == '__main__':
    main()
