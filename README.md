# Smart Watch - 嵌入式智能手表

基于 STM32F103C8T6 + MPU6050 + OLED + BLE 的计步器与智能手表项目。

---

## 硬件清单

| 组件 | 型号 | 接口/参数 | 说明 |
|------|------|-----------|------|
| MCU | STM32F103C8T6 | HSI 64MHz, 72MHz 主频 | 核心处理器 |
| 加速度计 | MPU6050 | I2C 0xD0, ±2g, DLPF 44Hz | 步态检测 |
| 显示屏 | SSH1106 OLED | I2C 0x78, 128×64 | 5 页 UI |
| 蓝牙模块 | HC-05 (BLE 透传) | UART 115200, FFE1/FFE2 | 手机通信 |
| 按键 | 微动开关 | PA1, 上拉输入 | 短按/长按 |
| 温度传感器 | MPU6050 内置 | 精度 ±1°C | 环境温度 |

### 接线图（文字描述）

```
  3.3V ──┬── MPU6050 VCC
         ├── SSH1106 VCC
         ├── HC-05 VCC
         ├── 4.7kΩ ──┬── I2C_SCL (PB6)
         │            └── 4.7kΩ ─── I2C_SDA (PB7)   [I2C 总线外接上拉]
         └── 微动开关一端
  GND  ──┬── MPU6050 GND
         ├── SSH1106 GND
         ├── HC-05 GND
         ├── MPU6050 AD0        [接地 → 地址 0xD0]
         └── 微动开关另一端

  STM32          外设
  ───────────────────────────
  PB6 (SCL) ─────┬── MPU6050 SCL
                 └── SSH1106 SCL
  PB7 (SDA) ─────┬── MPU6050 SDA
                 └── SSH1106 SDA
  PA9 (TX) ──1kΩ──► HC-05 RX    [串口限流保护，防止 5V 灌入 STM32]
  PA10 (RX) ◄────── HC-05 TX    [3.3V 直连，STM32 可识别]
  PA1  ─────────── 微动开关中间 [内部上拉输入模式]
```

**接线说明**
- **I2C 总线**：PB6/PB7 为开漏输出，必须外接 **4.7kΩ 上拉电阻** 到 3.3V。MPU6050 与 OLED 共用同一 I2C 总线。
- **MPU6050 地址**：AD0 引脚接地 → 从机地址 `0xD0`（写）/`0xD1`（读）。
- **UART 串口**：HC-05 模块虽标称 3.3V，但部分批次 TX 引脚可能残留 5V 电平。在 STM32 的 PA9（TX）与 HC-05 RX 之间串联 **1kΩ 电阻** 限流，防止灌电流损坏 MCU；PA10（RX）直接连接，无需电阻。
- **按键**：微动开关一端接 PA1，另一端接 GND。STM32 配置为内部上拉输入，松开时高电平，按下时低电平。
- **OLED 地址**：SSH1106 模块地址 `0x78`（含写标志位），硬件上电自动识别。

---

## 软件架构

### 开发环境
- **IDE**: VSCode + STM32CubeMX + CMake + Ninja
- **编译器**: GCC ARM 14.3.1
- **RTOS**: FreeRTOS (4 任务)
- **HAL 时基**: TIM4

### 任务分配

| 任务 | 优先级 | 周期 | 职责 |
|------|--------|------|------|
| ScreenTask | Normal | 100ms | OLED 刷新、时钟 tick |
| SensorTask | Normal | 50ms | MPU6050 读取、计步算法 |
| BleTask | Normal | 100ms | 蓝牙 RX/TX、命令解析 |
| IdleTask | Low | 20ms | 按键扫描、状态机 |

---

## 功能实现

### 1. 计步器算法 (v35)

**核心思想**: 走路产生周期性加速度（圆弧运动），而随机抖动无规律。通过**周期检测 + 方差阈值 + 回落触发**区分真假步态。

#### 优化点

| 优化 | 实现 | 效果 |
|------|------|------|
| **消除 `sqrtf`** | 全部比较改用 `mag_sq = ax²+ay²+az²` | 节省 ~90% CPU 周期（STM32F103 无 FPU） |
| **O(1) 滑动平均** | 维护 `window_sum` 变量，更新时 2 次加减 | 无论窗口多大，固定计算量 |
| **开机自校准** | 前 2 秒静止采集，计算真实基线 | 适应任意佩戴角度 |
| **周期检测** | 检测最近 2 步间隔是否稳定在 300-1500ms | 过滤孤立抖动（1步丢弃，2步+确认） |

#### 算法流程

```
1. 读取 MPU6050 三轴加速度
2. 计算平方模长 mag_sq = ax²+ay²+az²
3. 更新 O(1) 滑动平均窗口（20 点 = 1 秒）
4. 计算方差（10 点 = 500ms）
5. 检测回落触发：mag_sq < avg 且 peak_drop > threshold
6. 记录步态间隔，检查是否周期性
7. 周期性确认 → 计步；否则丢弃
```

#### 参数

```c
#define AVG_WINDOW      20      // 1s 滑动平均
#define VAR_WINDOW      10      // 500ms 方差窗口
#define PEAK_DIFF_SQ    51000000LL  // 回落阈值（平方单位）
#define VAR_THRESHOLD_SQ 2000000000LL // 方差阈值（平方单位）
#define DEBOUNCE_MS     250     // 防抖 250ms
#define MIN_INTERVAL    300     // 最快步频
#define MAX_INTERVAL    1500    // 最慢步频
```

#### 数字滤波器设计（FIR 滑动平均）

计步算法中需要提取加速度信号的**直流基线**（重力分量），以检测交流波动（步态）。我们采用 **FIR 滑动平均滤波器**（Moving Average, N=20），原因在“IIR 滤波器记忆问题”中详细说明。

**1. 差分方程**

$$y[n] = \frac{1}{N}\sum_{k=0}^{N-1}x[n-k]$$

其中 $N=20$，$T_s=50\text{ms}$（SensorTask 周期），窗口时长 $N\cdot T_s = 1\text{s}$。$y[n]$ 为当前基线，$x[n]$ 为原始平方模长。

**2. 系统函数（Z 变换）**

对差分方程两边取 Z 变换：

$$Y(z) = \frac{1}{N}\sum_{k=0}^{N-1}z^{-k}X(z)$$

$$H(z) = \frac{Y(z)}{X(z)} = \frac{1}{N}\cdot\frac{1-z^{-N}}{1-z^{-1}}$$

这是 **FIR 滤波器** 的系统函数，分子 $1-z^{-N}$ 产生 $N-1$ 个零点，分母 $1-z^{-1}$ 在 $z=1$（即 $\omega=0$）处有一个极点，被零点抵消，系统稳定且无反馈（无 IIR 的“记忆”问题）。

**3. 频率响应**

令 $z = e^{j\omega}$，其中 $\omega = 2\pi f T_s$：

$$H(e^{j\omega}) = \frac{1}{N}e^{-j\omega(N-1)/2}\cdot\frac{\sin(\omega N/2)}{\sin(\omega/2)}$$

幅频响应：

$$|H(e^{j\omega})| = \frac{1}{N}\left|\frac{\sin(\omega N/2)}{\sin(\omega/2)}\right|$$

- **直流增益**：$|H(e^{j0})| = 1$（完全保留基线）
- **第一个零点**：$\omega = 2\pi/N$，对应 $f = 1/(N\cdot T_s) = 1\text{Hz}$。步频约为 $1\sim2\text{Hz}$，该频率处增益为 0，恰好滤除步态周期性分量，只保留直流。
- **-3dB 截止频率近似**：$f_c \approx \dfrac{0.443}{N\cdot T_s} = \dfrac{0.443}{1.0} \approx 0.44\text{Hz}$。高于此频率的噪声被衰减。

**4. O(1) 实时优化**

朴素实现每次求和 $N$ 个数，复杂度 $O(N)$。利用滑动窗口的递推性质：

$$S[n] = S[n-1] + x[n] - x[n-N]$$
$$y[n] = \frac{S[n]}{N}$$

每次更新只需 **2 次加减 + 1 次除法**，复杂度 $O(1)$，与窗口大小无关。在 72MHz 的 STM32F103 上，即使 $N=100$ 也几乎不增加 CPU 负载。

**5. 为什么选 FIR 而非 IIR？**

| 特性 | IIR（一阶滞后） | FIR（滑动平均） |
|------|---------------|----------------|
| 系统函数 | $H(z)=\dfrac{\alpha}{1-(1-\alpha)z^{-1}}$ | $H(z)=\dfrac{1}{N}\dfrac{1-z^{-N}}{1-z^{-1}}$ |
| 阶跃响应 | 指数衰减，时间常数 $\tau\approx T_s/\alpha$ | $N$ 步后完全达到稳态，无拖尾 |
| 记忆效应 | 强（$\alpha=0.1$ 需 25s 衰减） | 无（$N$ 步后旧样本完全退出） |
| 稳定性 | 需关心极点位置 | 永远稳定（无反馈） |
| 适合场景 | 需要陡峭滚降 | 需要无记忆、线性相位 |

**结论**：计步器需要基线快速跟踪佩戴姿态变化（如从坐姿到站姿），FIR 的有限窗口恰好满足“无记忆”要求；IIR 的无限脉冲响应会导致姿态变化后基线长时间漂移，产生误计步。

---

### 2. UI 系统

#### 5 页面导航

```
Home [1/5] → Steps [2/5] → Info [3/5] → Bluetooth [4/5] → Stopwatch [5/5]
```

| 页面 | 显示内容 | 短按功能 |
|------|----------|----------|
| Home | 时间、步数、温度 | 下一页 |
| Steps | 步数、进度条、目标 | 下一页 |
| Info | 三轴加速度、滤波值 | 下一页 |
| Bluetooth | RX 统计、HEX 数据 | 下一页 |
| Stopwatch | 秒表 MM:SS.d、状态 | 开始/暂停 |

#### 按键逻辑

- **短按** (<500ms): Stopwatch 页 = 开始/暂停；其他页 = 切下一页
- **长按** (>1000ms): 回主页 + **清零秒表**

---

### 3. 蓝牙通信

#### GATT 特性

| UUID | 属性 | 方向 | 用途 |
|------|------|------|------|
| `0000ffe1-...` | notify | 模块→手机 | 接收自动上报数据 |
| `0000ffe2-...` | write | 手机→模块 | 发送控制命令 |

#### 自动上报格式

```
STEPS:55 TEMP:20\r\n
```

每 2 秒自动发送一次。

#### 手机控制命令

```bash
python ble_control.py STEP       # 查询步数
python ble_control.py RESET      # 重置步数
python ble_control.py TEMP       # 查询温度
python ble_control.py SW         # 开关秒表
python ble_control.py SYNC,14:30:00  # 同步时间
```

#### 手机 APP 截图

> 实时日志显示 `STEPS:55 TEMP:20`，数据后附 `\x00` 填充。

#### 为什么每次要填充到 230 字节？

**现象**：最初调试时，STM32 明明调用了 `HAL_UART_Transmit()` 发送数据，但手机端要么收不到，要么等 10 秒以上才收到一次，极不稳定。

**根因**：我们使用的 HC-05（实际是某廉价 BLE 透传模块）固件版本为 **v3.0/v4.0**，该版本存在已知的 **230 字节缓冲 Bug**：模块内部维护一个 230 字节的 FIFO 缓冲区，**数据不满 230 字节不会立即通过 BLE 发出去**，而是等缓冲区填满或超时（约 10~15 秒）才批量发送。

| 发送内容 | 实际长度 | 模块行为 | 手机接收 |
|---------|---------|---------|---------|
| `STEPS:55 TEMP:20\r\n` | 18 字节 | 缓冲中，等待填满 | 无（或 10s+ 延迟） |
| 填充至 230 字节 | 230 字节 | 立即触发发送 | 实时收到 |

**Workaround**：每次发送时，将有效数据拷贝到 230 字节的缓冲区，剩余部分用 `\0`（NULL）填充，然后一次性发送 230 字节。BLE 模块收到后立即发送，手机端收到后按 C 字符串规则自然忽略尾部 `\0`。

```c
void Bluetooth_SendString(const char *str) {
    // HC-05 v3.0/v4.0 firmware bug: buffers up to 230 bytes before sending
    // Workaround: pad every transmission to 230 bytes with nulls
    uint8_t buf[230];
    uint16_t len = strlen(str);
    
    if (len > 230) len = 230;
    memcpy(buf, str, len);
    // Pad with zeros to 230 bytes (module ignores trailing nulls)
    for (uint16_t i = len; i < 230; i++) {
        buf[i] = 0x00;
    }
    HAL_UART_Transmit(bt_uart, buf, 230, 500);
}
```

**代价与权衡**：
- **吞吐量**：每次有效负载仅 ~20 字节，却要发送 230 字节，物理层效率仅 **~9%**
- **实时性**：换来了每 2 秒稳定推送一次，否则延迟不可接受
- **替代方案**：更换无此 bug 的模块（如 JDY-31、HC-04 v2.0），但受限于课程提供的硬件未替换

---

## 遇到的问题与解决方案

### 1. IIR 滤波器记忆问题

**现象**: 平放时 `mag` 稳定在 17000，但 IIR 滤波器（α=0.1）需要 25 秒才能从 20000 衰减到基线。

**根因**: Kalman 滤波器和一阶滞后滤波都有"记忆"，衰减极慢。

**解决**: 去掉 IIR，改用 **FIR 移动平均**（N=20，1 秒窗口）。无记忆，平放后 1 秒恢复基线。

---

### 2. v26 死区 Bug（多计数根源）

**现象**: 平放不走路，步数也会缓慢增加。

**根因**: `else if (mag < 16500)` 留下了一个死区：16500-18000 之间的噪声既不触发也不重置计数器，导致噪声跨峰累积。

```c
// 错误代码
if (mag > 18000) { ... }        // 触发计数
else if (mag < 16500) { ... }   // 重置计数器
// 16500~18000 之间：什么都不做！噪声在这里累积
```

**解决**: 改成 `else`（任何非触发样本都重置计数器），彻底消除死区。

---

### 3. v28 连续确认多计数

**现象**: 走 10 步实际计 13 步。

**根因**: 一个步态波峰很宽，"连续确认"机制在一个波峰内触发了多次。

**解决**: 改为 **回落触发**（fall-back trigger）——检测从峰值回落到平均线以下，而不是检测峰值本身。每个波峰只触发一次。

---

### 4. 小幅度运动误触发

**现象**: 轻敲桌子或手臂晃动也会被计步。

**根因**: 回落触发对任何超过阈值的波动都敏感，无法区分"走路"和"抖动"。

**解决**: 增加 **方差检测**（`VAR_THRESHOLD_SQ`）。走路的方差远大于轻微抖动，只有方差足够大才认为是有效步态。

---

### 5. 轻微抖动 1 步 vs 正常走路

**现象**: 偶尔抖动 1 下也被计入。

**根因**: 单步检测无法区分"偶然抖动"和"开始走路"。

**解决**: 引入 **周期检测**（状态机）。第 1 步进入 PENDING 状态，3 秒内没有第 2 步则丢弃。正常走路的步态间隔稳定在 300-1500ms，连续 2 步确认后才计入。

---

### 6. 电脑蓝牙接收不到数据

**现象**: 手机 APP 能收到 `STEPS:55 TEMP:20`，电脑 `bleak` 脚本订阅成功但 15 秒无数据。

**根因**: 廉价 BLE 模块的 notify 通道同一时间只能服务一个 Central 客户端。手机之前连接过，模块固件缓存了手机的 CCC 配置，导致电脑的订阅实际未生效。

**诊断**: `ble_diag.py` 显示订阅成功但无 notify，确认是模块固件限制而非代码问题。

**解决**: 放弃电脑接收，改用 **手机 APP 接收数据 + 电脑发送命令** 的方案。`ble_control.py` 作为手机端控制工具。

---

### 7. UI 文字重叠

**现象**: Steps 页面的 `Goal: 10000` 和底部页码框重叠；Stopwatch 页面的 `Short:Start/Pause` 和底部页码框重叠。

**根因**: SSH1106 高度 64px，底部框从 y=55 开始，文字放在 y=50 会重叠。

**解决**: 上移文字到 y=40，底部框保留在 y=55。Stopwatch 页面将提示和状态合并为一行（`RUN Press=Pause`）。

---

## 版本演进

| 版本 | 算法 | 状态 |
|------|------|------|
| v14 | 原始 open-watch 算法 | 失败（Kalman 记忆 + 单轴触发） |
| v23 | IIR 一阶滞后 | 失败（衰减太慢） |
| v26 | 回落触发 + 连续确认 | 有死区 bug |
| v27 | 修复死区 bug | 工作但多计数 |
| v28 | 移动平均 + 连续确认 | 走 10 计 13 |
| v29 | 回落触发（无方差） | 小幅度运动误触发 |
| v30 | 回落触发 + 方差检测 | 工作正常 |
| v31 | 状态机 + 连续确认 | 太保守 |
| v32 | 周期检测（有 bug） | STABLE_COUNT 重复定义 |
| v33 | 修复 v32 bug | 敏感但工作 |
| v34 | 消除 sqrtf + O(1) + 自校准 | 优化版 |
| **v35** | 最终版 + Stopwatch 页面 | **当前版本** |

---

## 快速开始

```bash
# 1. 克隆项目
git clone https://github.com/vmware001/embedded_system_design_smart_watch.git

# 2. 进入工程目录
cd embedded_system_design_smart_watch/project

# 3. 编译
ninja -t clean
cmake -B build -G Ninja
cmake --build build

# 4. 下载到 STM32（需 ST-Link）
# VSCode 按 F5 或 OpenOCD

# 5. 手机蓝牙控制
pip install bleak
python ../ble_control.py --scan
python ../ble_control.py SW
```

---

> **注意**：`picture/` 目录下的实物照片、面包板接线图、物料详情图片等由于文件较大（超过 GitHub 单文件 50MB 建议限制），未上传至 GitHub。如需查看，请在本地仓库或课程设计提交资料中获取。

## 项目结构

```
.
├── README.md                 # 本文件
├── ble_control.py            # 手机 BLE 控制脚本
├── project/                  # STM32 工程
│   ├── Core/Src/
│   │   ├── main.c           # 主任务（RTOS 调度）
│   │   ├── mpu6050.c       # 计步算法 v35
│   │   ├── ui.c            # 5 页 UI + 秒表
│   │   ├── bluetooth.c     # BLE 串口通信
│   │   └── ssh1106.c       # OLED 驱动
│   ├── Core/Inc/
│   ├── Drivers/             # HAL 库
│   ├── Middlewares/         # FreeRTOS
│   ├── CMakeLists.txt
│   └── build/               # 编译输出（gitignore）
├── trash/                   # 废弃测试脚本
│   ├── ble_test.py
│   ├── hc05_test.py
│   ├── find_com*.py
│   └── ...
├── picture/                 # 项目照片（本地查看，未上传 GitHub）
├── 面包板接线图.png          # 本地查看，未上传 GitHub
└── 系统设计文档.docx
```

---

> 本项目为 2026 年嵌入式系统设计课程设计作业。

---

## 许可证

**GNU General Public License v3.0 (GPLv3.0)**

本程序是自由软件：你可以再发布本程序，或对本程序进行修改，前提是你必须遵守自由软件联盟发布的 GNU 通用公共许可证（GPLv3.0）的条款。你可以选择以该许可证的第 3 版或（随你的意愿）任何更新的版本发布。

本程序发布的目的是希望它有用，但 **不提供任何担保**；甚至没有适销性或特定用途适用性的默示担保。详情请参见 GNU 通用公共许可证。

- 完整的 GPLv3.0 英文原文：[https://www.gnu.org/licenses/gpl-3.0.html](https://www.gnu.org/licenses/gpl-3.0.html)
- 非正式中文参考：[https://www.gnu.org/licenses/gpl-3.0.zh-cn.html](https://www.gnu.org/licenses/gpl-3.0.zh-cn.html)

**开源组件声明**：本项目使用了以下开源组件，其许可证保持原条款不变：
- STM32 HAL / LL 库（BSD-3-Clause，ST 许可）
- FreeRTOS（MIT 许可证）
- open-watch 项目（MIT 许可证，坎特伯雷大学）
