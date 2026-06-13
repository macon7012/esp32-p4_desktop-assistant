# ESP32-P4 Desktop Assistant

基于乐鑫 **ESP32-P4** 的桌面智能助手，搭载 7 寸 MIPI LCD 触摸屏，集成 AI 智能灯光、语音控制、传感器监测、WiFi 联网、USB 外设扩展等功能，提供类桌面操作系统的触控交互体验。

> 本项目基于开源项目大量修改而成，删减并增加了许多功能模块，最终打造为一个多功能桌面助手。

---

## ✨ 功能特性

### 🖥 桌面交互界面
| 功能 | 说明 |
|------|------|
| **桌面主屏** | LVGL 图形界面，状态栏显示时间/日期/星期、WiFi 状态、USB/SD 连接状态 |
| **应用切换** | 支持多应用页面切换（通过底部菜单栏） |
| **触控交互** | 电容触摸屏，支持点击、滑动、拖拽手势 |
| **屏幕背光** | PWM 背光亮度可调 |

### 📱 内置应用
| 应用 | 说明 |
|------|------|
| **2048 游戏** | 触屏滑动操作的经典 2048 数字游戏，带动画效果 |
| **计算器** | 科学计算器，支持 DEG/RES 模式，带历史表达式显示 |
| **日历** | LVGL 日历组件，支持年月下拉选择、日期高亮 |
| **文件管理** | SD 卡文件浏览（通过 FATFS） |
| **USB OTG** | USB HID + MSC 复合设备管理界面 |

### 🏠 智能家居控制
| 功能 | 硬件资源 | 说明 |
|------|----------|------|
| **灯光控制** | GPIO21 (PWM) | 0~100% 亮度无极调节，触摸滑块控制 |
| **风扇控制** | GPIO7 (PWM) + GPIO8/9 (方向) | 4 档风速：关闭/低/中/高 |
| **自动模式** | AI 模型驱动 | 根据传感器数据自动开关灯 |
| **传感器面板** | BH1750 + DHT11 | 实时显示温度、湿度、光照、人体存在状态 |

### 🤖 AI 智能灯光
| 组件 | 说明 |
|------|------|
| **推理框架** | TensorFlow Lite Micro |
| **模型结构** | 3 层神经网络（FullyConnected → ReLU → Logistic） |
| **输入特征** | 人体存在（0/1）+ 环境光照度（lux） |
| **输出** | 开灯概率（>50% 则自动开灯） |
| **运行方式** | FreeRTOS 独立任务，每 2 秒推理一次，绑定到 Core 1 |

### 🎤 语音控制
| 功能 | 说明 |
|------|------|
| **语音识别** | 基于 ESP-SR 的离线语音命令识别 |
| **唤醒词** | 支持语音唤醒 |
| **AFE 前端** | 音频前端处理（回声消除、降噪等） |
| **控制命令** | 语音控制灯光开关、风扇档位、自动模式切换 |

### 🌐 WiFi 联网
| 功能 | 说明 |
|------|------|
| **自动连接** | 上电自动连接已保存的 WiFi |
| **配网界面** | LVGL 图形化 WiFi 扫描与密码输入（BOOT 键进入） |
| **NTP 对时** | 连接 WiFi 后自动同步网络时间（UTC+8） |
| **CSI 人体检测** | 基于 WiFi CSI 信号的人体存在感知 |
| **SD 卡共存** | WiFi 与 SD 卡分时复用，SD 卡插入自动挂起 WiFi |

### 🔌 USB 扩展
| 功能 | 说明 |
|------|------|
| **USB Serial JTAG** | 调试串口，连接状态实时显示在状态栏 |
| **USB HID** | 模拟键盘/鼠标输入设备 |
| **USB MSC** | 挂载 U 盘/SD 卡读卡器作为大容量存储，自动检测插拔 |

### ⚙ 系统功能
| 功能 | 说明 |
|------|------|
| **OTA 升级** | 支持远程固件升级 |
| **RTC 时钟** | 实时时钟，默认 2026-01-01（联网后自动同步） |
| **CAN 总线** | TWAI 通信自检 |
| **低功耗** | 支持 Light Sleep / Deep Sleep |
| **图片解码** | 支持 BMP / GIF / JPEG / PNG 格式 |

---

## 🛠 硬件配置

| 组件 | 型号/规格 |
|------|-----------|
| **主控** | ESP32-P4 (RISC-V 双核 400MHz) |
| **屏幕** | 7 寸 MIPI DSI LCD (1024×600)，电容触摸 |
| **光照传感器** | BH1750 (I2C) |
| **温湿度传感器** | DHT11 (GPIO10) |
| **音频** | ES8311 编解码器 + I2S 接口 |
| **Flash** | 16MB (DIO 模式, 80MHz) |
| **PSRAM** | 已启用 |

### GPIO 分配

| 外设 | GPIO |
|------|------|
| 屏幕背光 PWM | GPIO23 |
| 灯光 PWM | GPIO21 |
| 风扇 PWM | GPIO7 |
| 风扇方向 AIN1 | GPIO8 |
| 风扇方向 AIN2 | GPIO9 |
| 喇叭使能 | GPIO11 |
| DHT11 数据 | GPIO10 |

---

## 📁 项目结构

```
esp32-p4_desktop-assistant/
├── main/
│   ├── main.c                        # 入口：初始化外设 → 启动 LVGL 桌面
│   ├── ai_model.cpp / ai_model.h     # TensorFlow Lite 推理（C++）
│   ├── smart_home_ctrl.c/.h          # 灯光/风扇 PWM 驱动
│   ├── APP/
│   │   ├── lvgl_demo.c               # LVGL 入口（触摸驱动注册）
│   │   ├── esp_lvgl_port.c           # LVGL 移植层
│   │   ├── MAIN_UI/                  # 桌面应用界面
│   │   │   ├── app_wifi.c            # WiFi 扫描/配网界面
│   │   │   ├── app_2048.c            # 2048 游戏
│   │   │   ├── app_brush.c           # 画板
│   │   │   ├── app_calculator.c      # 计算器
│   │   │   ├── app_calendar.c        # 日历
│   │   │   ├── app_camera.c          # 相机预览
│   │   │   └── app_file.c            # 文件管理器
│   │   ├── HANDLE/
│   │   │   ├── handle.c              # 事件处理/页面切换/状态栏更新
│   │   │   └── voice_control.c       # 语音识别控制
│   │   ├── HID_MSC/
│   │   │   └── usb_hid_msc.c         # USB HID + MSC 主机驱动
│   │   ├── IMAGE/                    # 图片资源（开机画面等）
│   │   └── include/                  # 公共头文件
│   ├── CMakeLists.txt
│   └── canon.pcm                     # 音频资源
├── components/
│   ├── BSP/                          # 板级驱动
│   │   ├── LED/  LCD/  TOUCH/        # 基础外设驱动
│   │   ├── MYIIC/  MYI2S/  LEDC/    # 总线驱动
│   │   ├── MYES8311/                 # 音频编解码器
│   │   ├── KEY/  RTC/  TWAI/        # 按键/时钟/CAN
│   │   └── bh1750/  dht11/          # 传感器驱动
│   └── Middlewares/
│       ├── MYFATFS/                  # FATFS 封装
│       └── PICTURE/                  # 图片解码（BMP/GIF/JPEG/PNG）
├── managed_components/               # ESP-IDF 组件依赖
├── sdkconfig                         # 项目配置
└── README.md
```

---

## 🚀 快速开始

### 环境要求

- **ESP-IDF**: v5.5.1
- **工具链**: RISC-V 32-bit (`riscv32-esp-elf`)
- **芯片**: ESP32-P4
- **Flash**: 16MB

### 编译与烧录

```bash
# 激活 ESP-IDF
. /path/to/esp-idf/export.sh

# 编译
idf.py build

# 烧录并查看输出
idf.py -p /dev/ttyUSB0 flash monitor
```

### 启动模式

| 操作 | 模式 |
|------|------|
| **正常上电** | 启动桌面助手，自动连接 WiFi |
| **按住 BOOT 键上电** | 进入 WiFi 配网模式 |

---

## 📝 依赖组件

| 组件 | 用途 |
|------|------|
| LVGL | 图形界面框架 |
| TensorFlow Lite Micro | AI 推理引擎 |
| ESP-SR / ESP-AFE | 语音识别与音频前端 |
| ES8311 | 音频编解码器 |
| esp_driver_jpeg | JPEG 硬件解码 |
| esp_lcd | MIPI LCD 驱动框架 |
| esp-idf-lib/dht | DHT 传感器驱动 |
| cJSON | JSON 解析 |
| FATFS | SD 卡文件系统 |
| USB Host (MSC + HID) | USB 大容量存储与人机接口 |

---

**硬件平台**: ESP32-P4  \
**屏幕**: 7 寸 1024×600 MIPI LCD  \
**开发环境**: ESP-IDF v5.5.1  \
**工具链**: RISC-V 32-bit
