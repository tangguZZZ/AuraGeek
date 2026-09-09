# AuraGeek

AuraGeek 是一款基于 ESP32-S3-N16R8 和 LVGL 9.5.0 的桌面极客智能摆件。当前工程包含 320×240 横屏 UI、天气与 NTP 时钟、温湿度占位、EC11/按键导航、QQQ/VOO 趋势页、USB Audio FFT 频谱页和小智 AI 表情页框架。

## 当前状态

- 首页 UI 已完成实物验收并定版。
- ST7789 显示方向、RGB565 色彩链路和 LVGL 刷新已在实物验证。
- Windows SDL2 模拟器与 ESP32-S3 固件共享同一套 `AuraUi` 实现。
- 固件使用 PlatformIO 构建；模拟器使用 `D:\LVGL-Dev` 中的官方 LVGL 9.5.0、CMake、SDL2 环境。

## 快速入口

- [总体设计与接口说明](Wiki.md)
- [开发与构建说明](DEVELOPMENT.md)
- [UI 素材](Doc/UI素材)
- [UI 核心实现](src/ui/AuraUi.cpp)
- [PC 模拟器说明](simulator/README.md)

## 凭据安全

复制 `include/config/Secrets.example.h` 为 `include/config/Secrets.local.h`，再在本机填写 Wi-Fi 信息。`Secrets.local.h` 已加入 `.gitignore`，不得提交真实密码。

## 构建

```powershell
pio run -e esp32-s3-devkitc-1
```

烧录端口以本机实际枚举为准；当前开发板下载串口使用 COM9，原生 USB Audio 测试接口与下载串口分离。
