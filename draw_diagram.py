import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.patches import FancyBboxPatch
import numpy as np
from daimon_runtime import setup_plot, save_figure

def main(ctx):
    setup_plot(ctx)
    
    fig, ax = plt.subplots(1, 1, figsize=(32, 26))
    ax.set_xlim(0, 32)
    ax.set_ylim(0, 26)
    ax.set_aspect('equal')
    ax.axis('off')
    
    C_I2C = '#2196F3'
    C_UART = '#FF9800'
    C_GPIO = '#4CAF50'
    C_3V3 = '#F44336'
    C_5V = '#9C27B0'
    C_GND = '#607D8B'
    C_RES = '#795548'
    C_SW = '#E91E63'
    
    def rect(x, y, w, h, color, text, text_color='white', fontsize=10, radius=0.02):
        box = FancyBboxPatch((x, y), w, h, boxstyle=f"round,pad=0.02,rounding_size={radius}", 
                              facecolor=color, edgecolor='black', linewidth=2, alpha=0.85)
        ax.add_patch(box)
        ax.text(x + w/2, y + h/2, text, ha='center', va='center', fontsize=fontsize, 
                color=text_color, fontweight='bold', wrap=True)
        return (x, y, w, h)
    
    def pin(x, y, label, color='black', fontsize=8, offset=(0, 0.35)):
        ax.plot(x, y, 'o', color=color, markersize=10, markeredgecolor='black', markeredgewidth=1.5)
        ox, oy = offset
        ax.text(x + ox, y + oy, label, ha='center', va='bottom', fontsize=fontsize, 
                color='black', fontweight='bold')
        return (x, y)
    
    def hline(y, x1, x2, color, lw=2.5):
        ax.plot([x1, x2], [y, y], color=color, linewidth=lw, solid_capstyle='round')
    
    def vline(x, y1, y2, color, lw=2.5):
        ax.plot([x, x], [y1, y2], color=color, linewidth=lw, solid_capstyle='round')
    
    def resistor_box(x, y, text, w=1.0, h=0.5):
        box = FancyBboxPatch((x - w/2, y - h/2), w, h, 
                              boxstyle="round,pad=0.02,rounding_size=0.02",
                              facecolor='#FFF8E1', edgecolor=C_RES, linewidth=2)
        ax.add_patch(box)
        ax.text(x, y, text, ha='center', va='center', fontsize=9, 
                color=C_RES, fontweight='bold')
    
    # === MODULES ===
    rect(1, 20, 4.5, 3.5, '#FFC107', 'ST-Link V2', 'black', 11, 0.1)
    rect(22, 20, 5, 3.5, '#4CAF50', '1.3" OLED\nSSH1106 I2C', 'white', 11, 0.1)
    rect(22, 14, 4.5, 3.5, '#03A9F4', 'MPU6050\nGY-521 I2C', 'white', 11, 0.1)
    rect(11, 12, 5, 5, '#FF5722', 'STM32F103C8T6\n最小系统板', 'white', 12, 0.1)
    rect(1, 12, 4, 4, '#00BCD4', 'EC11 旋转编码器', 'white', 11, 0.1)
    rect(22, 5, 4.5, 4, '#9C27B0', 'HC-05 蓝牙\n仅安卓支持', 'white', 11, 0.1)
    rect(1, 5, 6, 3.5, '#FF9800', 'TP4056 充电模块\n+ 602040 电池', 'white', 11, 0.1)
    rect(11.5, 5, 3.5, 2, '#8BC34A', '轻触按键', 'white', 11, 0.1)
    
    # === PINS ===
    # STM32 left
    pin(11, 16.0, 'PA0', C_GPIO, 8, (-0.5, 0.3))
    pin(11, 15.2, 'PA1', C_GPIO, 8, (-0.5, 0.3))
    pin(11, 14.4, 'PA2', C_GPIO, 8, (-0.5, 0.3))
    pin(11, 13.6, 'PA3', C_GPIO, 8, (-0.5, 0.3))
    pin(11, 12.8, 'PA9', C_UART, 8, (-0.5, 0.3))
    pin(11, 12.0, 'PA10', C_UART, 8, (-0.5, 0.3))
    
    # STM32 right
    pin(16, 16.0, 'PB6', C_I2C, 8, (0.5, 0.3))
    pin(16, 15.2, 'PB7', C_I2C, 8, (0.5, 0.3))
    pin(16, 14.4, 'PB3', C_SW, 8, (0.5, 0.3))
    pin(16, 13.6, 'PB4', C_SW, 8, (0.5, 0.3))
    pin(16, 12.8, '3.3V', C_3V3, 8, (0.5, 0.3))
    pin(16, 12.0, '5V', C_5V, 8, (0.5, 0.3))
    pin(16, 11.2, 'GND', C_GND, 8, (0.5, 0.3))
    
    # ST-Link bottom
    pin(2, 20, '3.3V', C_3V3, 8, (0, 0.5))
    pin(3, 20, 'SWDIO', C_SW, 8, (0, 0.5))
    pin(4, 20, 'SWCLK', C_SW, 8, (0, 0.5))
    pin(5, 20, 'GND', C_GND, 8, (0, 0.5))
    
    # OLED bottom
    pin(23, 20, 'VCC', C_3V3, 8, (0, 0.5))
    pin(24, 20, 'GND', C_GND, 8, (0, 0.5))
    pin(25, 20, 'SCL', C_I2C, 8, (0, 0.5))
    pin(26, 20, 'SDA', C_I2C, 8, (0, 0.5))
    
    # MPU6050 left
    pin(22, 16.5, 'VCC', C_3V3, 8, (-0.5, 0.3))
    pin(22, 15.7, 'GND', C_GND, 8, (-0.5, 0.3))
    pin(22, 14.9, 'SCL', C_I2C, 8, (-0.5, 0.3))
    pin(22, 14.1, 'SDA', C_I2C, 8, (-0.5, 0.3))
    
    # HC-05 left
    pin(22, 7.5, 'VCC', C_3V3, 8, (-0.5, 0.3))
    pin(22, 6.7, 'GND', C_GND, 8, (-0.5, 0.3))
    pin(22, 5.9, 'TXD', C_UART, 8, (-0.5, 0.3))
    pin(22, 5.1, 'RXD', C_UART, 8, (-0.5, 0.3))
    
    # EC11 right
    pin(5, 15.0, 'CLK', C_GPIO, 8, (0.5, 0.3))
    pin(5, 14.2, 'DT', C_GPIO, 8, (0.5, 0.3))
    pin(5, 13.4, 'SW', C_GPIO, 8, (0.5, 0.3))
    pin(5, 12.6, 'GND', C_GND, 8, (0.5, 0.3))
    pin(5, 11.8, 'VCC', C_3V3, 8, (0.5, 0.3))
    
    # TP4056 right
    pin(7, 7.0, 'OUT+', C_5V, 8, (0.5, 0.3))
    pin(7, 6.2, 'OUT-', C_GND, 8, (0.5, 0.3))
    
    # Key top
    pin(13.25, 7, 'PA3', C_GPIO, 8, (0, 0.5))
    
    # === POWER BUSES (in clear zones) ===
    hline(24.5, 1, 27, C_3V3, 3)
    ax.text(14, 24.8, '3.3V 电源总线', ha='center', va='bottom', fontsize=11, color=C_3V3, fontweight='bold')
    
    hline(3.5, 7, 16, C_5V, 3)
    ax.text(11.5, 3.8, '5V 电源总线', ha='center', va='bottom', fontsize=11, color=C_5V, fontweight='bold')
    
    hline(1, 1, 27, C_GND, 3)
    ax.text(14, 1.3, 'GND 地线总线', ha='center', va='bottom', fontsize=11, color=C_GND, fontweight='bold')
    
    # === CONNECTIONS ===
    # 3.3V drops
    vline(2, 20.0, 24.5, C_3V3)
    vline(16, 12.8, 24.5, C_3V3)
    vline(23, 20.0, 24.5, C_3V3)
    vline(22, 16.5, 24.5, C_3V3)
    vline(22, 7.5, 24.5, C_3V3)
    vline(5, 11.8, 24.5, C_3V3)
    
    # GND drops
    vline(5, 20.0, 1, C_GND)
    vline(16, 11.2, 1, C_GND)
    vline(24, 20.0, 1, C_GND)
    vline(22, 15.7, 1, C_GND)
    vline(22, 6.7, 1, C_GND)
    vline(5, 12.6, 1, C_GND)
    vline(7, 6.2, 1, C_GND)
    vline(13.25, 7, 1, C_GND)
    
    # 5V
    vline(7, 7.0, 3.5, C_5V)
    vline(16, 12.0, 3.5, C_5V)
    
    # SWD
    hline(14.4, 16, 3, C_SW)
    vline(3, 14.4, 20.0, C_SW)
    hline(13.6, 16, 4, C_SW)
    vline(4, 13.6, 20.0, C_SW)
    
    # I2C SCL
    hline(16.0, 16, 22, C_I2C)
    vline(22, 16.0, 14.9, C_I2C)
    hline(16.0, 22, 25, C_I2C)
    vline(25, 16.0, 20.0, C_I2C)
    
    # I2C SDA
    hline(15.2, 16, 22, C_I2C)
    vline(22, 15.2, 14.1, C_I2C)
    hline(15.2, 22, 26, C_I2C)
    vline(26, 15.2, 20.0, C_I2C)
    
    # Pull-up resistors
    vline(24, 16.0, 24.5, C_RES, 2)
    resistor_box(24.8, 20.0, '4.7K')
    vline(25, 15.2, 24.5, C_RES, 2)
    resistor_box(25.8, 19.5, '4.7K')
    
    # UART
    hline(12.8, 11, 9, C_UART)
    vline(9, 12.8, 10.0, C_UART)
    hline(10.0, 9, 22, C_UART)
    vline(22, 10.0, 5.1, C_UART)
    resistor_box(10.5, 7.5, '1K')
    
    hline(12.0, 11, 8, C_UART)
    vline(8, 12.0, 10.0, C_UART)
    hline(10.0, 8, 22, C_UART)
    vline(22, 10.0, 5.9, C_UART)
    
    # EC11
    hline(16.0, 11, 5, C_GPIO)
    hline(15.2, 11, 5, C_GPIO)
    hline(14.4, 11, 5, C_GPIO)
    
    # Key
    vline(13.25, 13.6, 7, C_GPIO)
    vline(13.25, 5, 1, C_GND)
    
    # === LEGEND ===
    legend_x = 28
    legend_y = 14
    rect(legend_x, legend_y, 3.5, 7, '#FAFAFA', '', 'black', 9, 0.05)
    ax.text(legend_x + 1.75, legend_y + 6.5, '图例 & 说明', ha='center', va='center', fontsize=12, fontweight='bold', color='black')
    
    legend_items = [
        (C_I2C, 'I2C总线 (SCL/SDA)'),
        (C_UART, 'UART串口 (TX/RX)'),
        (C_GPIO, 'GPIO/编码器/按键'),
        (C_3V3, '3.3V 电源线'),
        (C_5V, '5V 电源线'),
        (C_GND, 'GND 地线'),
        (C_SW, 'SWD调试线'),
        (C_RES, '电阻 (标注阻值)'),
    ]
    
    for i, (color, text) in enumerate(legend_items):
        y = legend_y + 5.6 - i * 0.75
        ax.plot([legend_x + 0.2, legend_x + 0.8], [y, y], color=color, linewidth=3, solid_capstyle='round')
        ax.text(legend_x + 1.0, y, text, ha='left', va='center', fontsize=9, color='black')
    
    # === PIN TABLE ===
    pin_x = 28
    pin_y = 1.5
    rect(pin_x, pin_y, 3.5, 5.5, '#E3F2FD', '', 'black', 8, 0.05)
    ax.text(pin_x + 1.75, pin_y + 5.2, '引脚分配总表', ha='center', va='center', fontsize=11, fontweight='bold', color='black')
    
    pin_text = [
        'PB6 -> I2C_SCL (OLED+MPU)',
        'PB7 -> I2C_SDA (OLED+MPU)',
        'PA9 -> USART1_TX -> HC-05 RXD',
        'PA10 -> USART1_RX -> HC-05 TXD',
        'PA0 -> EC11 CLK',
        'PA1 -> EC11 DT',
        'PA2 -> EC11 SW',
        'PA3 -> 轻触按键 -> GND',
        'PB3 -> ST-Link SWDIO',
        'PB4 -> ST-Link SWCLK',
    ]
    
    for i, text in enumerate(pin_text):
        ax.text(pin_x + 1.75, pin_y + 4.7 - i * 0.48, text, ha='center', va='center', fontsize=8, color='black')
    
    # === NOTES ===
    note_x = 28
    note_y = 7.5
    rect(note_x, note_y, 3.5, 2.2, '#FFF3E0', '', 'black', 8, 0.05)
    ax.text(note_x + 1.75, note_y + 1.9, '注意事项', ha='center', va='center', fontsize=11, fontweight='bold', color='#E65100')
    notes = [
        'I2C必须外接4.7K上拉到3.3V',
        'PA9->HC-05 RXD 串1K电阻',
        'OLED驱动: SSH1106 (非SSD1306)',
        'HC-05仅支持安卓手机',
    ]
    for i, note in enumerate(notes):
        ax.text(note_x + 1.75, note_y + 1.4 - i * 0.38, note, ha='center', va='center', fontsize=8, color='black')
    
    # Title
    ax.text(16, 25.5, 'STM32F103C8T6 智能手表 面包板接线图', ha='center', va='center', fontsize=22, fontweight='bold', color='black')
    ax.text(16, 25.0, '所有模块为直插/成品模块，面包板可直接搭建 | OLED: SSH1106 | 蓝牙: HC-05 (仅安卓)', ha='center', va='center', fontsize=11, color='gray')
    
    fig.tight_layout(pad=0.5)
    
    out_path = r"C:\Users\admin\Documents\Embedded-System-Design-main\2026课程设计\面包板接线图_新版.png"
    plt.savefig(out_path, dpi=200, bbox_inches='tight', pad_inches=0.2)
    plt.close(fig)
    
    return {"path": out_path}

if __name__ == "__main__":
    main({})
