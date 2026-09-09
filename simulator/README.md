# AuraGeek Windows Simulator

本机根目录 `D:/My_Project/AuraGeek`，工具与 LVGL 源码集中在 `D:/LVGL-Dev`。
原生 Windows、CMake、MinGW GCC、SDL2；不需要 LVGL Editor、GUI Guider、容器或虚拟机。

在 VS Code 打开 `AuraGeek.code-workspace`（单根工程），运行“终端 → 运行任务”：

- `AuraGeek: Simulator`：编译并启动新 UI。
- `AuraGeek: Official LVGL Demo`：官方 `lv_demo_widgets()`。
- `AuraGeek: UI replay test`：页面切换回放，截图位于 `simulator/build/windows-debug/screenshots`。
- `AuraGeek: Daily stock cache`：受日请求预算约束的 QQQ/VOO 历史缓存更新。

命令行：`powershell -File tools/simulator.ps1 run`；最后参数也可为 `build`、`demo`、`test`、`stocks`。
PC 可执行文件：`simulator/build/windows-debug/aurageek_sim.exe`。
编译成功后也可以直接双击该 EXE；所需 SDL2/MinGW DLL 自动复制到同一目录。

## 交互

鼠标滚轮或左右键：从内容页进入菜单，在菜单内选择项目。
鼠标左击菜单图标直接进入；中键/Enter 确认；右键/Escape 返回。
股票页点击顶部标的或中键/Enter 切换 QQQ、VOO，点击 6M/1Y/3Y/5Y/ALL 切换范围。实物编码器范围操作待接线后继续验证。
左键拖动不等于滚轮；当前用滚轮模拟编码器。

## 数据与验证边界

- PC 使用操作系统时间。天气默认未知、温湿度未连接；ESP32 使用其 Wi-Fi/NTP/Open-Meteo 服务。
- 股票加载 `cache/QQQ.csv`、`cache/VOO.csv`；缺失时显示真实离线参考快照并标注 REF CACHE / OFFLINE，不是合成行情。参考快照截至 2026-09-04。价格及 MA20/MA55 先按完整日线计算，再裁切抽样；涨跌幅对应当前区间。
- 频谱 PC 使用 48 kHz 测试 PCM，经 512 点 Hann 窗 FFT、48 频段和平滑后绘制；底部标注 SIMULATED PCM。
- ESP32 无 USB PCM 输入时显示 USB AUDIO NOT CONNECTED。
- AI 页纯黑，仅显示用户素材中的待机 GIF，不表示语音聊天已经接通。
- `--smoke` 回放 9 个阶段：首页、菜单、QQQ 全历史、QQQ 三年、VOO 三年、频谱、表情、菜单、首页；截图为 `screenshots/page-0.bmp` 至 `page-8.bmp`。
- 回放为应用内事件调用，不等同 Windows 鼠标注入或实物 EC11 测试。CTest 的 `aurageek_tests` 验证 FFT 及日线计算。

## 实物 USB 测试

只接屏幕时，通过 CH343 串口输入 `ui spectrum`。原生 USB 口连电脑，将播放器输出选择为“耳机 (TinyUSB UAC1)”。

项目自带定向测试，不改变 Windows 默认音频设备、不录制电脑音频：

```powershell
python .\tools\test_usb_audio.py --play --seconds 5 --frequency 1000
```

主机发送成功与设备 FFT 验证是两项证据，应同时检查串口 `peak_hz`。512 点 / 48 kHz 的频率分辨率为 93.75 Hz，因此 1 kHz 峰值落在 1031.25 Hz 附近是正常量化结果。

## 工具版本

CMake 4.4.0、Ninja 1.13.1、MinGW GCC 15.2.0（x64 POSIX SEH UCRT）、SDL2 2.32.10、LVGL 9.5.0。
构建入口明确指定版本，不修改系统 PATH。GCC 及 SDL2 必须使用实际英文路径，指向中文路径的 Junction 不足以解决链接器问题。
`CMakePresets.json` 是唯一的 PC 工具路径配置。`src/ui/AuraUi.cpp` 与 `src/ui/resources` 同时编入 PC/ESP32。
不要把 CMake 检测阶段的 `CMakeDetermineCompilerABI_*.bin` 当作应用；真正产物是 EXE。

## 官方来源

- https://lvgl.io/docs/open/integration/pc/windows
- https://github.com/lvgl/lv_port_pc_vscode
- https://github.com/lvgl/lvgl/tree/v9.5.0
- https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10

官方 PC port 的 SDL 初始化模式与 Widgets Demo 用作基础；AuraGeek 使用项目自己的 CMake/HAL 入口和共享 C++ UI。
