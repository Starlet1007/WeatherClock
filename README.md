# WeatherClock - 智能天气时钟

一个基于 STM32F407 + ESP32-C3 双芯架构的桌面天气时钟。STM32F407 做主控驱动屏幕和传感器，ESP32-C3 跑 AT 固件负责 WiFi 联网，跑 FreeRTOS，显示时间、室内温湿度、室外天气，带 240×320 彩色 LCD 屏幕。

## 功能

| 功能 | 说明 |
|------|------|
| ⏰ 时间显示 | NTP 网络对时，掉电 RTC 保持 |
| 🌡️ 室内温湿度 | AHT20 传感器，每 3 秒刷新 |
| 🌤️ 室外天气 | 心知天气 API，每分钟刷新 |
| 📶 WiFi 状态 | 实时显示连接状态和信号强度 |
| 🎨 天气图标 | 晴天/多云/雨/雪/雷电/大风 6 种图标 |

## 硬件清单

| 模块 | 型号 | 数量 | 作用 |
|------|------|------|------|
| 主控 | STM32F407 开发板 | 1 | 驱动屏幕、读取传感器、跑 FreeRTOS |
| WiFi | ESP32-C3（AT 固件） | 1 | WiFi 联网、HTTP 请求、SNTP 对时 |
| 屏幕 | ST7789 驱动 240×320 TFT LCD | 1 | 彩色显示 |
| 温湿度 | AHT20 模块 | 1 | 室内温湿度采集 |

## 接线

### ST7789 屏幕（SPI2）

| 屏幕引脚 | STM32 引脚 |
|----------|------------|
| SCL / CLK | PB13 (SPI2_SCK) |
| SDA / MOSI | PB15 (SPI2_MOSI) |
| RES | 自定义 GPIO |
| DC | 自定义 GPIO |
| CS | PB12 (SPI2_NSS) |
| VCC | 3.3V |
| GND | GND |

### ESP32-C3 WiFi 模块（USART2，AT 固件）

| ESP32-C3 引脚 | STM32 引脚 |
|--------------|------------|
| TX | PA3 (USART2_RX) |
| RX | PA2 (USART2_TX) |
| VCC | 3.3V |
| GND | GND |
| EN | 3.3V（拉高使能） |

> ESP32-C3 需刷入 ESP AT 固件才能用 AT 指令通信。固件下载：https://docs.espressif.com/projects/esp-at/

### AHT20 温湿度传感器（I2C2）

| AHT20 引脚 | STM32 引脚 |
|-----------|------------|
| SCL | PB10 (I2C2_SCL) |
| SDA | PB11 (I2C2_SDA) |
| VCC | 3.3V |
| GND | GND |

## 软件架构

```
main.c
  ├── board_lowlevel_init()    # 时钟、外设初始化
  ├── workqueue_init()         # 工作队列（异步任务）
  └── main_init()              # 主任务
        ├── board_init()       # 串口、RTC、AHT20
        ├── ui_init()          # 屏幕显示任务
        ├── welcome_page       # 启动欢迎页
        ├── wifi_init()        # ESP32-C3 初始化
        ├── wifi_page          # WiFi 连接页
        ├── mloop_init()       # 主循环定时器
        └── main_page          # 主界面
```

**核心机制：**
- **UI 消息队列** — 所有屏幕绘制通过队列投递，单任务串行刷新，避免冲突
- **工作队列** — 耗时操作（网络请求等）异步执行，不阻塞 UI
- **定时器驱动** — FreeRTOS 软件定时器周期性触发数据更新

**刷新频率：**

| 内容 | 间隔 |
|------|------|
| 时间 | 1 秒 |
| 室内温湿度 | 3 秒 |
| WiFi 状态 | 5 秒 |
| 室外天气 | 1 分钟 |
| NTP 对时 | 1 小时 |

## 编译和烧录

### 依赖

- **MCU 库**：STM32F4xx 标准外设库（STM32F407）
- **RTOS**：FreeRTOS
- **WiFi 驱动**：ESP32-C3 需烧录 ESP AT 固件，STM32 侧用 AT 指令驱动
- **组件驱动**：ST7789、AHT20
- **工具链**：ARM GCC（arm-none-eabi-gcc）或 Keil MDK

### 编译

使用 Makefile 或 IDE 导入项目，确保包含以下目录：
- `font/` — 字体文件
- `image/` — 图标文件
- `page/` — 页面文件

> 本项目仅包含应用层代码，底层驱动（ST7789、AHT20、ESP AT 等）需自行准备。

### 配置

修改 `app.h` 中的配置：

```c
#define APP_VERSION "v1.0"           // 版本号
#define WIFI_SSID   "你的WiFi名"      // ← 改成你的
#define WIFI_PASSWD "你的WiFi密码"    // ← 改成你的
```

天气 API 在 `app.c` 的 `outdoor_update()` 中修改：

```c
const char *weather_url = 
    "https://api.seniverse.com/v3/weather/now.json"
    "?key=你的API_KEY"          // ← 去 seniverse.com 免费注册获取
    "&location=你的城市ID"       // ← 城市 ID
    "&language=en&unit=c";
```

## 页面流程

```
开机 → 欢迎页 → WiFi 连接页 → 连接成功 → 主界面
                                    ↓ 失败
                                错误页面
```

## 主界面布局

```
┌──────────────────────────────┐
│  📶 WiFi-SSID                │  ← 白色背景
│                              │
│   12 : 30                    │  ← 大字体时间（闪烁冒号）
│   2025.06.19  星期五          │
├──────────┬───────────────────┤
│ 室内     │ 室外              │
│ 🌡 25°C │ ☀️ 晴天            │  ← 卡其色 / 蓝色背景
│ 💧 48%  │ 32°C              │
└──────────┴───────────────────┘
```
