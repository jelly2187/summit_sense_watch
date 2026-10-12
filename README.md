# 山知 · SummitSense

面向户外与高原场景的多传感器可穿戴感知平台。固件工程名为 `summit_sense_watch`。

当前阶段优先验证原始数据采集、统一时间戳、USB串口传输和PC记录，再逐步加入显示、运动与生理算法。后续研究高原状态分析、佩戴结构、功耗和整机集成。

## 当前进度

- [x] 安装并识别 ESP-IDF v5.5.5。
- [x] 使用官方 `hello_world` 模板创建工程，目标为 ESP32-S3。
- [x] 配置16 MB Flash、DIO模式、80 MHz。
- [x] 编译、UART烧录和串口监视通过，已看到 `Hello world!`。
- [x] 建立Git仓库。
- [ ] 启用并验证8 MB PSRAM。
- [ ] 替换为持续运行的心跳程序，连续验证30分钟。
- [ ] 接入MAX30102，读取器件信息与红光、红外原始PPG。
- [ ] 接入十轴IMU，实现PPG与运动数据同步记录。
- [ ] 完成PC采集工具，再接入GNSS、温度与屏幕。

当前源码仍为官方Hello World示例：输出芯片与Flash信息后倒计时重启。此时的周期性重启是示例预期行为。

## 硬件

| 子系统 | 选型 | 接入计划 |
|---|---|---|
| 主控 | 微雪 ESP32-S3-DEV-KIT-N16R8 | 16 MB Flash、8 MB PSRAM |
| 生理采集 | 野火小智 MAX30102模块 | I²C，红光与红外两路原始PPG |
| 运动与气压 | 亚博十轴IMU惯导模块 | UART1；默认9600 baud、10 Hz，解析通过后提高速率 |
| 定位 | 亚博GNSS，ATGM336H-5N载板 | UART2；默认9600 baud、8N1、1 Hz NMEA |
| 温度 | AS6221 | I²C |
| 显示 | 1.3寸240×240方形彩色SPI屏 | 驱动芯片与引脚待确认 |
| 记录 | USB串口 → PC | 原始流与解析数据保存到PC |

超声波与microSD暂不接入。GPIO分配及各模块供电、逻辑电平需核对后再接线；表中的UART分配为计划，尚未实现。

## 开发环境

- Windows、VS Code。
- ESP-IDF扩展：当前使用2.3.0。
- ESP-IDF：固定为 **v5.5.5**。
- 芯片目标：`esp32s3`。
- Board：`ESP32-S3 chip (via builtin USB-JTAG)`，用于OpenOCD调试配置。
- 当前烧录方式：**UART**，选择板载CH343对应的COM口。

本机IDF目录为 `D:\esp\.espressif\v5.5.5\esp-idf`，工具与Python环境位于 `C:\Espressif\tools`。换电脑时由安装管理器配置环境，无需复用这些绝对路径。

参考：[开发板资料](https://docs.waveshare.net/ESP32-S3-DEV-KIT-N8R8?variant=ESP32-S3-DEV-KIT-N16R8)、[ESP-IDF v5.5.5文档](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/index.html)。

## 编译、烧录与监视

在VS Code打开本工程根目录，按 `Ctrl+Shift+P` 使用以下命令：

| 操作 | ESP-IDF命令 |
|---|---|
| 选择环境 | `Select Current ESP-IDF Version`，选择v5.5.5 |
| 设置目标 | `Set Espressif Device Target`，选择esp32s3 |
| 修改配置 | `SDK Configuration Editor (Menuconfig)` |
| 编译 | `Build Your Project` |
| 选择串口 | `Select Port to Use (COM, tty, usbserial)` |
| 完整UART烧录 | `Flash (UART) Your Project` |
| 查看输出 | `Monitor Device` |

当前配置：Flash为16 MB、DIO、80 MHz；主控制台为UART0、115200 baud；PSRAM尚未启用。

需要使用命令行时，在ESP-IDF终端进入工程目录执行：

```powershell
idf.py build
idf.py -p COM7 flash monitor
```

将 `COM7` 替换为实际端口。监视器按 `Ctrl+]` 退出。同一COM口不能同时供监视器和PC采集程序使用。

## 工程结构

```text
summit_sense_watch/
├── CMakeLists.txt           # 工程名称与构建入口
├── main/
│   ├── CMakeLists.txt       # 主组件构建配置
│   └── hello_world_main.c   # 当前启动示例
├── .vscode/                # 编辑器配置；检查其中的本机路径
├── .gitignore
├── README.md
├── pytest_hello_world.py    # 模板自带测试，尚未运行
└── sdkconfig.ci            # 模板自带CI配置
```

`sdkconfig`与`build/`由配置和构建生成，已被Git忽略。当前尚未生成 `sdkconfig.defaults`。

## 下一步验收

1. 在配置编辑器中启用PSRAM，选择Octal模式、80 MHz和启动初始化；Flash仍使用DIO。
2. 编译、烧录，确认启动日志识别8 MB PSRAM，无初始化失败。
3. 将Hello World改为每秒输出序号、单调微秒时间戳和剩余内存的心跳程序。
4. 连续运行30分钟，无异常重启，序号与时间戳持续递增。
5. 接入MAX30102：先确认器件通信，再保存原始PPG；随后加入IMU同步采集。

采集层、缓存与传输层、算法层和显示层分开组织。初期保留原始数据、序号、设备时间戳、有效状态及丢样统计，方便PC端回放和离线分析。

## 版本管理与配置复现

每完成一个可验证成果提交一次，如基础启动、PSRAM验证、PPG采集和IMU解析。实验记录应注明固件提交号与采样配置。

当前 `.gitignore` 忽略了 `sdkconfig`。基础配置验证完成后，在ESP-IDF终端执行：

```powershell
idf.py save-defconfig
```

将生成的 `sdkconfig.defaults` 提交到Git；后续组件管理器生成的 `dependencies.lock`也应提交。构建产物、虚拟环境和大体积实验原始数据不纳入代码仓库。

## 模块参考资料

厂家资料保留在项目上级目录 `P261010_Watch\数据手册`：

- `10轴IMU惯导模块`
- `野火小智心率血氧模块资料_20241220`
- `亚博智能GPS模块`

本工程以官方Hello World示例为起点。保留原有源码版权与许可声明，后续引入第三方驱动或算法时记录来源和许可。
