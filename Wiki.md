# AuraGeek 软件设计 Wiki

主控 ESP32-S3-WROOM-1-N16R8；LVGL 9.5.0；ST7789 320×240 横屏。
工程 `D:/My_Project/AuraGeek`，PC 工具环境 `D:/LVGL-Dev`。2026-09-09 软件基线。

## 开发方式

VS Code 单根工作区 + PlatformIO 负责固件；CMake/SDL2 Windows Simulator 负责 UI 预览。
不再使用 GUI Guider、LVGL Editor/Pro Editor。共享 UI 源码为 `src/ui/AuraUi.*`，图片/GIF 位于 `src/ui/resources`。
运行方法见 [simulator/README.md](simulator/README.md)，工程约束见 [DEVELOPMENT.md](DEVELOPMENT.md)。

```text
AuraGeek/
  Doc/                 原始模块资料、UI 素材
  include/             配置、GPIO、LVGL 配置
  src/app/             系统编排
  src/drivers/         TFT、WS2812、EC11、PSRAM 内存池
  src/services/        网络、时间、天气、股票、FFT
  src/ui/              PC/ESP32 共用原生 LVGL 页面与资源
  simulator/           CMake、SDL2 输入、官方例程、PC 数据缓存
  tools/               构建和运行入口
  platformio.ini       ESP32 构建配置
```

## 页面与输入

开机默认首页，使用 `wallpaper.jpg` 壁纸、半透明卡片，显示时间、日期、天气、温度、湿度及联网状态；首页不显示 AURA/DESK 和 LOCAL TIME 标题。
时、分由 LVGL 自绘加宽七段数码管显示，右侧放大秒钟环沿圆形刻度顺时针运行；日期行右侧以青、蓝、紫、粉、珊瑚、黄、绿的 7 点渐变轨道标识当前星期，固定按周一至周日（1→2→3→4→5→6→7）排列，当天点更亮更粗。天气卡右侧是放大的非数据含义流动双波纹，用于自然填充留白；波纹上的流动滚珠会随当前天气切换颜色：晴天金黄、云天天蓝、雨天青蓝、雷暴紫色、雪天冰蓝、雾天银灰，天气未知时使用琥珀色。
页面切换退出 180 ms、进入 220 ms；退出采用 ease-in，进入采用 ease-out。菜单卡片选择采用 180～220 ms 的 `cubic-bezier(.4, 0, .2, 1)` 缩放/位移动画；只有秒针、天气波纹和频谱等持续循环效果允许匀速推进。
频谱有彩色环形柱和峰值衰减，AI 使用待机 GIF。
温湿度采用素材库的 `temperature.png`、`humidity.png`，未接传感器不生成假数据。
未联网/未校时为 `--:--`；天气失败为 `未知 --℃`；温湿度未接为 `--.-°C`、`-- %`。

菜单使用素材库 `Menu_wallpaper.jpg`，生成资源时高质量缩放为 320×240，不修改原始素材。旋转 EC11 从内容页进入图标菜单，继续旋转选择。首页/股票/频谱/Agent 图标分别使用
`Desktop_homepage.png`、`Stock_analysis.png`、`Music_spectrum.png`、`Agent.png`。
EC11 按压单击确认/进入；股票页单击切换 QQQ/VOO；双击返回上一级；长按 1.2 秒直接回首页。
独立 KEY0 短按返回上一级，长按 0.8 秒直接回首页。两路按键软件去抖均为 20 ms，EC11 双击窗口为 300 ms。
PC 用鼠标滚轮或方向键旋转、左击图标/中键/Enter 确认、右键/Escape 返回。
股票页左击顶部标的切换 QQQ/VOO，点击 6M/1Y/3Y/5Y/ALL 切换范围。
只有屏幕的调试阶段，可通过 CH343 串口发送 `ui home/menu/stocks/spectrum/agent`、`ui next` 和 `range 0..4`（每次一条，LF 结尾）。
范围按钮目前使用模拟器鼠标或串口验证；EC11/KEY0 逻辑已接入固件，但旋转、单击、双击、长按仍以实物操作结果作为最终验收。

## 股票

标的已确认 QQQ (`105.QQQ`)、VOO (`107.VOO`)。采用直接 HTTP API，无额外 SDK。
`push2.../stock/get` 为即时行情；日 K 折线使用 `push2his.eastmoney.com/api/qt/stock/kline/get`。
采用 `klt=101`、`fqt=0` 不复权收盘价，美元计价，避免把日线表示为实时价格。
PC 和固件首次请求接口可提供的完整日线，后续从最后交易日期开始增量合并。
固件最多保存 12000 条，完整日期/收盘价存 LittleFS，RAM 数据在 PSRAM；图表从原始日线计算 MA20/MA55 后再压缩为最多 280 个显示点。
页面包含收盘价、所选范围首尾价格变化百分比、三条曲线、价格轴、起止日期、总条数、来源和缓存状态。百分比不是实时涨跌或含分红总回报。
MA20/55 不足样本时不绘制。ALL 表示本地可用全部历史，不保证覆盖基金成立以来每个交易日。

主 API 失败时，使用已获取的参考站点 `https://thefitville.cn:8099/api/data` 离线快照，明确显示 `REF CACHE`。
快照更新时间 2026-09-08，最后交易日 2026-09-04：QQQ 6453 条（始于 2001-01-02），VOO 4021 条（始于 2010-09-09）。
该参考数据沿用上游调整口径，仅作离线兜底；绝不与东方财富不复权数据拼接。直连成功后整套切换为 `EM RAW`。
快照由 `tools/import_reference_stocks.py` 从已下载 JSON 生成，编入固件，也用于 PC 缓存为空时的预览；没有虚构行情。

上海周二至周六 09:00 后，每个标的每天最多 3 次尝试，成功即结束当日预算。预算与重试截止时间先落盘，再发请求。
股票请求严格串行，标的之间间隔 2～2.5 秒；普通错误退避 60/300 秒，403 至少等待一天，429 至少等待一小时，并遵守更长的 Retry-After。
固件后台每分钟只检查预算，不代表每分钟请求；PC 更新脚本有互斥文件，不自动安装系统定时任务。
只联网且已校时后调度，失败保留旧数据；不轮换域名、不伪造浏览器身份。间隔或请求头不能保证服务端永不限流。
完整 JSON 只解析所需元数据，日线逐条处理并每 64 条让出 CPU，防止全量字符串去重占用 CPU 触发看门狗。
历史数据可能被供应商修订，增量更新不会自动修正更早历史；不复权跨拆股时也可能有跳变，需要显式数据维护。

## 音乐频谱

目标链路：电脑 UAC 音频输出 → ESP32-S3 原生 USB 接收 → PCM 缓冲 → 单声道 → Hann 窗 → 512 点 FFT → 48 频段 → 环形动画。
固件使用项目私有 `.pio-core` 中 Arduino-ESP32 3.3.11 官方 `USBAudioCard`：48 kHz、16 位小端、双声道播放输入，无麦克风录音端点。项目级 `platforms_dir`、`packages_dir`、`cache_dir` 防止 pioarduino 平台、框架和 Home 包影响其他 PlatformIO 工程；不能只写会被 VS Code 环境变量覆盖的 `core_dir`。
USB 回调只合并声道并写入有界 PCM 环；主循环处理最新连续 512 点做 FFT。过载丢弃旧样本，统计计数，不阻塞 USB 接收。
PC 使用明确标识的模拟 PCM 验证动画，固件无输入时为静止环/未连接；静音时即使 USB 仍发送零样本，FFT 也衰减到零。
Windows 可能显示官方接口名 `耳机 (TinyUSB UAC1)`。在应用或系统音频输出中选择它即可发送电脑播放音频；这不是系统音频监听器，不会自动抓取其他输出设备的声音。
目前没有连接外部扬声器，选择此输出后主要用于频谱分析，不会通过 ESP32 发声。
原生 USB D-→GPIO19、D+→GPIO20；它就是此前枚举为 COM12 的原生 USB 物理口。启用当前 UAC 固件后该口应枚举为音频播放设备而不是串口；CH343 COM9 只用于烧录和串口日志，不承载 USB 音频。

## 小智 AI

页面已接入 `机器人表情包gif_20260820` 中待机眨眼 GIF，资源编入 Flash。
界面背景纯黑，只显示动态表情，不显示标题、状态或其他文字。
INMP441、MAX98357A 和小智会话协议尚未联调；无文字设计不代表语音功能已经接通。
后续待机/聆听/思考/说话/异常状态驱动表情，不以循环动画冒充聊天接通。
不实现用户 Wi-Fi 上传 GIF 功能。

## 网络与显示

只连接首选 `Innoxsz-2.4G` 和备选 `tangguzi`；凭据仅放本机 `Secrets.local.h`。
ESP32 联网后 NTP 校时 UTC+8；Open-Meteo 深圳天气数字码映射中文及图标。
天气和股票后台任务不操作 LVGL。HTTPS 当前沿用原 MVP `setInsecure()`，发布前需 CA 验证。

保留实物确认的显示参数：`setRotation(1)`、`TFT_RGB_ORDER TFT_BGR`、`TFT_INVERSION_OFF`、
RGB565 `pushColors(..., true)` 字节交换、HSPI 20 MHz、20 行局部双缓冲。
LVGL 384 KiB 池从 PSRAM 分配。大图和 GIF 不占用静态内部 RAM 池。

## GPIO 基线

| 模块 | 信号 → GPIO |
|---|---|
| ST7789 | CS1→10，MOSI→11，CLK→12，DC→13，RES→14，BLK→18；MISO/CS2 不用 |
| AHT20 | SDA→8，SCL→9 |
| INMP441 | WS→4，SCK→5，SD→6 |
| MAX98357A | BCLK→7，LRC→15，DIN→16，SD_MODE→17 |
| EC11 | A→39，B→40，PUSH/SW→21 |
| 独立按键 | KEY0→41 |
| WS2812B | 板载/正式三灯 DIN→48；开发板独立三灯样机可用47 |
| UART0 | TX→43，RX→44 |
| 原生 USB | D-→19，D+→20，UAC1 播放输入 |

电源开关使用拨动开关，EC11 不承担硬件开关机。供电、电气关系、保护和 PCB 元器件由硬件同事设计。

### 2.4 寸 TFT + EC11 模块 12 针接线

| 模块针脚 | 模块信号 | ESP32-S3 |
|---:|---|---|
| 1 | GND | GND |
| 2 | VCC | 3.3V |
| 3 | SCL/CLK | GPIO12 |
| 4 | SDA/MOSI | GPIO11 |
| 5 | RES | GPIO14 |
| 6 | DC | GPIO13 |
| 7 | CS | GPIO10 |
| 8 | BLK | GPIO18 |
| 9 | A | GPIO39 |
| 10 | B | GPIO40 |
| 11 | PUSH | GPIO21 |
| 12 | KEY0 | GPIO41 |

模块原理图中 A、B、PUSH、KEY0 均已有 10 kΩ 上拉及 RC 滤波，固件仍使用 `INPUT_PULLUP`；PUSH/KEY0 按下为低电平。接线时模块与开发板必须共地。针脚编号和信号名以 `Doc/2.4寸TFT彩屏+EC11旋转编码器模块/引脚说明.docx` 及同目录原理图为准。

## 当前验证边界

- 官方 Widgets Demo 已在 Windows 原生运行并正常退出。
- 新 UI 在 Simulator 完成 11 阶段回放，包含曲线转场、菜单壁纸、QQQ/VOO、范围切换、频谱与 Agent；截图为 LVGL 实际渲染。CTest 覆盖股票端点、均线预热、FFT 1 kHz/3 kHz 与静音衰减。
- PC 频谱回放是测试 PCM；股票主接口失败时保留带来源标识的真实离线快照，并执行日预算。
- 新固件已编译并通过 CH343 COM9 烧录；16 MB Flash / 8 MB PSRAM 检测正常，TFT 驱动颜色和方向参数保留。
- 实机 LittleFS 重启加载通过 checksum 校验：QQQ 6454 条、VOO 4022 条，末日 2026-09-08，来源 EM RAW / CACHED。该状态是本次已验证快照，后续随日线更新变化。
- Windows UAC1 枚举正常；定向发送 1 kHz / 3 kHz 测试音，设备 FFT 分别检测到 1031.2 Hz / 3000.0 Hz；停止输出后衰减到零。频率格点间隔为 93.75 Hz。
- 实机串口页面回放覆盖首页、菜单、股票、频谱、纯黑表情并返回首页，前后空闲 heap / PSRAM 数值一致；该验证不能替代人眼对实物新 UI 的最终确认。
- 小智会话、AHT20、EC11、外接扬声器及三灯模块动态验收仍待硬件接入。

用户已删除旧归档 `AuraGeek-Retired-20260908`；当前工程不依赖它，不能再视为可恢复备份。

仅接屏幕时，CH343 串口 115200 baud 可输入 `ui home`、`ui menu`、`ui stocks`、`ui spectrum`、`ui agent`，或 `ui next`、`range 0..4`、`stock status`。批量回放命令在 `tools/bench_ui.txt`；这些命令不清除股票请求预算。量产可关闭 `AURAGEEK_BENCH_COMMANDS`。
