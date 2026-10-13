# 山知 · SummitSense

面向户外与高原场景的多传感器可穿戴感知平台。当前固件工程为 `summit_sense_watch`，优先完成原始数据采集、统一时间戳、USB串口传输与PC记录，已加入生理实验算法、温度、IMU与手表演示界面，继续推进统一状态和整机验证。

## 当前进度

- [x] ESP-IDF v5.5.5环境、编译、UART烧录与串口监视通过。
- [x] ESP32-S3、16 MB Flash与8 MB Octal PSRAM已验证。
- [x] 持续心跳已烧录，用户反馈运行观察未发现异常。
- [x] 基础版本已提交Git：`b7d3181`。
- [x] MAX30102通信验证通过：地址0x57、PART_ID=0x15、REV_ID=0x03。
- [x] 按ESP-IDF组件划分应用入口、板级配置与模块代码。
- [x] MAX30102双通道原始采集已有实物响应：用户反馈放手指后RED/IR数值变化。
- [x] PPG波形和约100 Hz记录验证通过，PC支持实时显示与自动保存。
- [x] 心率与未标定血氧效果显示已实机运行，准确性对照待完成。
- [x] IMU运动数据接收、PC曲线与三维姿态展示已实现，三轴数据已有用户实测反馈。
- [x] IMU气压USB预配置、保存及断电回接验证通过。
- [x] AS6221温度与PC温度页完成，用户确认功能通过。
- [x] SPI屏幕与镜像修正通过实屏确认。
- [x] 七页LVGL中文演示界面、BOOT导航与自动轮播完成并编译通过；同源预览检查通过。
- [ ] 新界面实屏验收、触摸与真实数据快照接入。
- [ ] GNSS户外定位、动态高度、运动标注与长期同步记录验证。

界面操作及预览：[watch_ui.md](docs/watch_ui.md)。本轮屏幕数值均标明“演示”，联合测试延期。

## 目录与职责

```text
summit_sense_watch/
├── main/
│   ├── app_main.c                 # 启动与模块调用顺序
│   └── CMakeLists.txt
├── components/
│   ├── board_config/
│   │   ├── include/board_config.h # 板级引脚与基础速率
│   │   └── CMakeLists.txt
│   ├── system_monitor/
│   │   ├── include/system_monitor.h
│   │   ├── system_monitor.c       # 启动信息与SYS心跳
│   │   └── CMakeLists.txt
│   ├── max30102/
│       ├── include/max30102.h     # 模块公开接口
│       ├── max30102.c             # I²C探测与器件ID读取
│       ├── max30102_sampling.c    # FIFO采集、队列和原始数据输出
│       └── CMakeLists.txt
│   ├── physiology/              # 心率与实验血氧窗口算法
│   ├── yahboom_imu/             # UART运动、姿态、气压协议
│   ├── as6221/                  # I²C温度读取与有效状态
│   ├── display_panel/           # ST7789S硬件初始化与SPI DMA
│   ├── display_test/            # 独立色块/边框诊断
│   └── watch_ui/                # LVGL中文页面、样例与导航
├── docs/
│   ├── architecture.md           # 分层、依赖与扩展规则
│   ├── hardware.md               # 硬件与接线
│   └── validation.md             # 验收步骤和实际证据
├── tools/
│   ├── serial_logger/            # 实时波形、记录与质量统计
│   ├── physiology_replay/        # 同一C算法的主机回放
│   ├── watch_ui_preview/         # 同源LVGL渲染与布局/内存检查
│   ├── ui_assets/                # 中文字体生成工具与许可
│   └── README.md
├── tests/                       # 已知信号与失效处理测试
├── .vscode/                      # 编辑器与调试配置
├── .devcontainer/                # 保留的开发容器配置，尚未验证
├── .clangd                       # 编辑器配置
├── .gitignore
├── CMakeLists.txt                # ESP-IDF工程入口
├── sdkconfig.defaults            # 可复现的板级配置
└── README.md
```

生成的 `sdkconfig`、`build/`与下载的 `managed_components/`不提交Git。模板中的 `hello_world_main.c`已替换为应用入口，失效的Hello World测试和示例CI配置已移除。只有已实现的模块建立代码组件；GNSS及统一快照等后续模块按相同结构加入。

完整规则见 [架构说明](docs/architecture.md)，接线见 [硬件说明](docs/hardware.md)，验证记录见 [验收说明](docs/validation.md)。

## 环境与当前配置

- Windows、VS Code；ESP-IDF扩展当前为2.3.0。
- 固定ESP-IDF **v5.5.5**，目标 **esp32s3**。
- 微雪ESP32-S3-DEV-KIT-N16R8：16 MB Flash、8 MB PSRAM。
- Flash：DIO、80 MHz；PSRAM：Octal、80 MHz、启动初始化与内存测试、允许malloc分配。
- Board：`ESP32-S3 chip (via builtin USB-JTAG)`，用于OpenOCD调试。
- 当前烧录：UART，经CH343串口；主控制台UART0、115200 baud。

本机IDF目录为 `D:\esp\.espressif\v5.5.5\esp-idf`，工具在 `C:\Espressif\tools`。换电脑后通过安装管理器重新选择环境，检查 `.vscode/`中的本机路径与COM口。

## VS Code操作

按 `Ctrl+Shift+P` 搜索带 `ESP-IDF:` 前缀的命令：

| 操作 | 命令 |
|---|---|
| 选择环境 | `Select Current ESP-IDF Version` |
| 设置芯片 | `Set Espressif Device Target` |
| 配置 | `SDK Configuration Editor (Menuconfig)` |
| 编译 | `Build Your Project` |
| 串口 | `Select Port to Use (COM, tty, usbserial)` |
| UART烧录 | `Flash (UART) Your Project` |
| 查看输出 | `Monitor Device` |

首次设置目标为esp32s3；后续调整配置不需要反复设置目标。迁移目录后重新编译即可刷新构建索引，在编辑器中打开 `main/app_main.c`。

需要命令行时，在ESP-IDF终端执行：

```powershell
idf.py build
idf.py -p COM6 flash monitor
```

将COM6替换为实际端口。监视器按 `Ctrl+]`退出；同一串口不能同时供监视器和PC采集工具使用。

## 当前运行流程

`app_main`依次打印启动信息、执行MAX30102一次性通信检查，然后持续输出SYS心跳，同时启动IMU、温度与独立演示UI。通信失败会打印错误并继续心跳；通信成功后启动100 Hz原始PPG采集与串口输出任务；同时由physiology组件计算心率与未标定血氧实验估算，每秒输出METRIC。

```text
type,seq,uptime_us,internal_free_bytes,internal_min_free_bytes,psram_free_bytes
SYS,0,123456,384419,352388,8386308
```

序号与时间戳在重启后重新开始。时间单位微秒，内存单位字节。内部空闲内存和PSRAM分别记录；历史最低空闲量不会因释放内存回升，不能单凭它下降判定泄漏。

## 下一步

1. 目视验收七页手表演示界面与BOOT导航。
2. 接入触摸和统一有效状态，再把演示数值替换为真实采集快照。
3. 有户外条件后接入GNSS并验证定位；模块共同固定后开展联合运动采集。
4. 有参考设备后进行一致性、误差和血氧标定评估。

## 版本管理

每个可验证成果独立提交；实验记录标明固件提交号与采样配置。修改Menuconfig后运行 `idf.py save-defconfig`，将 `sdkconfig.defaults`一起提交。组件管理器生成的 `dependencies.lock`也应提交，勿手动修改。

构建产物、环境与实验原始数据不纳入代码仓库。资料保留在上级项目的 `数据手册`目录。项目源自官方Hello World示例，保留相关源码许可；新增第三方组件时记录来源与许可。

参考：[ESP-IDF组件与构建体系](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-guides/build-system.html)、[微雪开发板](https://docs.waveshare.net/ESP32-S3-DEV-KIT-N8R8?variant=ESP32-S3-DEV-KIT-N16R8)。

原始PPG字段与质量标志见 [协议说明](docs/ppg_protocol.md)，PC操作与实验步骤见 [验收说明](docs/validation.md)。

实时查看并保存：关闭Monitor，在工程根目录运行 `python tools/serial_logger/record.py --port COM6 --seconds 60 --live`。按实际端口替换COM6；输出目录内的quality.json提供逐会话采样质量统计。

心率与实验血氧组件已加入，算法和有效状态说明见 [physiology.md](docs/physiology.md)。数值属于原型输出，血氧calibrated始终为0；准确性对照待完成。

汇报用文档：[2026-10-13阶段工作进展报告](docs/work_progress_2026-10-13.md)。

IMU运动与气压输出已验证，三维姿态及记录工具已实现。当前采用USB预配置后的三线接收；具体步骤和命令通路局限见 [imu.md](docs/imu.md)。

自动串口诊断：`python tools/serial_diagnostics/diagnose.py --port COM6 --seconds 15`；需要捕获完整启动时加 `--reset`，会正常重启ESP32。工具保存筛选日志与诊断JSON，详见tools/serial_diagnostics/README.md。

IMU气压输出已通过直接USB预配置、保存及断电后ESP32接收验证。当前使用5V/GND/TX→IO18三线接收，RX暂留空；约10 Hz气压和模块高度可显示。详情见docs/imu.md，ESP32到IMU的命令发送通路另行排查。

AS6221温度组件及上位机温度页已加入，使用I²C1 IO4/IO5、地址0x48，转换与读取目标4 Hz；接线及验证见 [temperature.md](docs/temperature.md)。已有1 Hz温度实机记录，4 Hz版本已获用户功能通过反馈，长期误差与热响应仍待量化。GNSS USB通信已验证，室外定位留待有条件时进行。

TK013F2261 V1.3屏幕独立色块/边框测试已加入，接线与目视验收见 [display.md](docs/display.md)。使用IO11/12/13/14、3.3V，暂不接触摸、复位或背光GPIO；屏幕显示及水平镜像修正已获用户确认通过。

手表七页中文界面已加入，启动入口从屏幕测试切换为LVGL演示模式。短按BOOT翻页，长按1秒切换8秒自动轮播；所有屏幕数值明确为样例，串口采集继续。实现、预览与验收见 [watch_ui.md](docs/watch_ui.md)。
