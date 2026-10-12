# 山知 · SummitSense

面向户外与高原场景的多传感器可穿戴感知平台。固件工程名为 `summit_sense_watch`。

当前阶段优先验证原始数据采集、统一时间戳、USB串口传输和PC记录，再逐步加入显示、运动与生理算法。后续研究高原状态分析、佩戴结构、功耗和整机集成。

## 当前进度

- [x] 安装并识别 ESP-IDF v5.5.5。
- [x] 使用官方 `hello_world` 模板创建工程，目标为 ESP32-S3。
- [x] 配置16 MB Flash、DIO模式、80 MHz。
- [x] 编译、UART烧录和串口监视通过，已看到 `Hello world!`。
- [x] 建立Git仓库。
- [x] 启用Octal 80 MHz PSRAM，串口确认8 MB可分配容量。
- [x] 替换为持续运行的心跳程序；用户反馈持续观察未发现异常。
- [ ] 接入MAX30102，读取器件信息与红光、红外原始PPG。
- [ ] 接入十轴IMU，实现PPG与运动数据同步记录。
- [ ] 完成PC采集工具，再接入GNSS、温度与屏幕。

当前源码已改为持续运行的心跳程序，不再主动倒计时重启。启动输出IDF版本、芯片、复位原因、Flash容量和PSRAM可分配堆容量；随后每秒输出一条CSV记录。已烧录；用户提供的串口记录显示时间与序号递增、内存稳定，并反馈后续观察未发现异常。

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

当前配置：Flash为16 MB、DIO、80 MHz；主控制台为UART0、115200 baud；PSRAM已启用：Octal模式、80 MHz、启动初始化与内存测试、允许malloc分配。

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
│   └── hello_world_main.c   # 当前心跳程序
├── .vscode/                # 编辑器配置；检查其中的本机路径
├── .gitignore
├── README.md
├── pytest_hello_world.py    # 模板自带测试，尚未运行
└── sdkconfig.ci            # 模板自带CI配置
```

`sdkconfig`与`build/`由配置和构建生成，已被Git忽略。已生成并纳入版本管理的 `sdkconfig.defaults` 保存当前板级配置，供新环境复现。

## 下一步验收

1. 断电连接MAX30102，确认3.3V供电、共地和I²C接口。
2. 加入I²C驱动，验证7位地址0x57应答并读取器件ID；尚未实现。
3. 配置红光与红外采样，批量读取FIFO，输出时间戳、序号及溢出状态。
4. 建立PC原始记录，再加入IMU同步采集。

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

### 心跳输出格式

```text
type,seq,uptime_us,internal_free_bytes,internal_min_free_bytes,psram_free_bytes
SYS,0,123456,250000,249000,0
```

`seq`从0递增，`uptime_us`为本次启动以来的微秒时间；内存字段单位为字节。重启后序号与时间会重新开始，并再次出现启动信息。PSRAM关闭时输出提示，PSRAM内存为0；启用并加入堆后应看到非零容量。内部空闲内存与PSRAM分开记录，避免总内存掩盖内部内存不足。历史最低空闲内存不会随释放内存回升，不能单凭其下降认定泄漏。

此程序用于运行状态验证，不代表传感器采集或PSRAM完整读写压力测试。基础版本已完成烧录并由用户观察运行状态；后续修改仍需复查异常重启、记录间隔与内存变化。模板自带的 `pytest_hello_world.py` 不适用于修改后的输出，后续再替换为采集平台测试。
