# 硬件与接线总表

更新日期：2026-10-14。以下依据当前固件配置、模块丝印和已完成的接线/验证记录整理；描述的是当前原型接法。

## 主板与连接原则

主板为微雪ESP32-S3-DEV-KIT-N16R8，16 MB Flash、8 MB Octal PSRAM。当前Flash为DIO 80 MHz，PSRAM为Octal 80 MHz。

- 表中的IO编号是开发板丝印对应的GPIO号，不是排针位置序号。
- 改线前断电；接入ESP32的模块GND统一接开发板GND。
- 3V3和5V是电源引脚，不是GPIO；多个3.3V模块可共用3V3/GND电源节点，信号线按表分别连接。
- 当前ESP32通过板载USB串口连接电脑，供电、UART烧录和日志传输沿用已验证方式，无需外接USB转串口。
- 以模块丝印确认引脚，不以杜邦线颜色推断功能。

引脚配置集中在 [board_config.h](../components/board_config/include/board_config.h)；BOOT按钮使用GPIO0，定义在watch_ui.c。下表中的已分配但未使用引脚和后续规划不是新增接线要求。

## 当前模块接线总表

| 模块 | 模块引脚/接口 | ESP32或电脑端 | 当前用途与状态 |
|---|---|---|---|
| 野火MAX30102 | 3V3 | ESP32 3V3 | 3.3V供电 |
| 野火MAX30102 | GND | ESP32 GND | 共地 |
| 野火MAX30102 | SDA | IO8 | I²C0数据，100 kHz，7位地址0x57 |
| 野火MAX30102 | SCL | IO9 | I²C0时钟 |
| 野火MAX30102 | INT | IO10 | 已接/分配，当前不使用中断；10ms轮询FIFO |
| 亚博十轴IMU | 5V | ESP32 5V | 当前载板5V供电 |
| 亚博十轴IMU | GND | ESP32 GND | 共地 |
| 亚博十轴IMU | TX | IO18 | UART1接收，9600 baud、8N1 |
| 亚博十轴IMU | RX | **留空** | 当前三线方案；IO17仅保留待排查的发送通路 |
| 亚博十轴IMU | 自身USB | **常规采集时拔掉** | 仅单独USB预配置时接电脑，见下文 |
| EV9-AS6221 | VCC | ESP32 3V3 | 当前模块3.3V供电 |
| EV9-AS6221 | GND | ESP32 GND | 共地 |
| EV9-AS6221 | SDA | IO4 | I²C1数据，100 kHz，7位地址0x48 |
| EV9-AS6221 | SCL | IO5 | I²C1时钟 |
| EV9-AS6221 | ADD0 | ESP32 GND | 地址选择 |
| EV9-AS6221 | ALERT/ADD1 | ESP32 GND | 当前地址选择接法，报警功能不使用 |
| TK013F2261 V1.3屏幕 | 上接口3.3V | ESP32 3V3 | 屏幕及载板供电 |
| TK013F2261 V1.3屏幕 | 上接口GND | ESP32 GND | 共地 |
| TK013F2261 V1.3屏幕 | 下接口CS | IO13 | SPI2片选 |
| TK013F2261 V1.3屏幕 | 下接口SCK | IO12 | SPI2时钟，模式0、10 MHz |
| TK013F2261 V1.3屏幕 | 下接口DC | IO14 | LCD命令/数据选择 |
| TK013F2261 V1.3屏幕 | 下接口MOSI | IO11 | 主控向LCD发送数据，无MISO接线 |
| 屏幕CST816D触摸 | 上接口SDA/SCL | **均留空** | 触摸尚未接入，不能接成LCD SPI信号 |
| 亚博GNSS | 自身USB | 电脑USB，**仅测试时连接** | USB转串口NMEA测试；未连接ESP32 |
| 开发板BOOT按钮 | 板载按钮 | GPIO0，**无需外接线** | 短按翻页；长按1秒切换轮播 |

## GPIO占用与未启用接口

| GPIO | 用途 | 当前状态 |
|---|---|---|
| IO0 | 板载BOOT按钮 | UI运行时使用；上电/复位时不要按住，否则进入下载模式 |
| IO4 / IO5 | AS6221 SDA / SCL | I²C1已启用 |
| IO8 / IO9 | MAX30102 SDA / SCL | I²C0已启用 |
| IO10 | MAX30102 INT | 已接但未由软件使用 |
| IO11 / IO12 | LCD MOSI / SCK | SPI2已启用 |
| IO13 / IO14 | LCD CS / DC | 已启用 |
| IO17 | UART1 TX，曾计划接IMU RX | 固件分配但当前不接模块；ESP32→IMU命令通路待排查 |
| IO18 | UART1 RX，接IMU TX | 已启用 |
| IO16 | 曾提出作为后续GNSS RX | **仅规划候选**，未接线、未写入board_config、未初始化UART2 |

V1.3屏幕没有外露RST/BL_CTR接口，**不需要接IO15或IO21**，也没有已启用的GPIO背光调光。屏幕触摸暂无引脚分配。其他GPIO不在本轮占用表内，不代表均适合直接用于新外设。

## MAX30102：I²C0与原始PPG

模块已有上拉，主控内部上拉关闭；使用7位地址0x57，不填写带读写位的0xAE/0xAF。PART_ID寄存器0xFF返回0x15，当前REV_ID为0x03。通信检查通过后采集100 Hz红光/红外PPG；INT接线不参与当前轮询读取。

原始记录格式见 [ppg_protocol.md](ppg_protocol.md)，测量算法与未标定边界见 [physiology.md](physiology.md)。

## 十轴IMU：当前是三线接收

当前只接 **5V→5V、GND→GND、TX→IO18**，IMU RX留空。模块经自身USB预配置并保存RSW=0x005E，输出加速度、角速度、角度、磁场和气压/气压高度，目标10 Hz；设置断电后保留已验证。

UART信号交叉连接：模块TX接ESP32 RX（IO18）。IO17是ESP32 TX；历史四线调试中接过模块RX，但未确认有效寄存器回读，不能作为当前已验证接法。即使启动日志打印TX=IO17，也不意味着目前需要补接RX。

单独USB配置时：先断电并断开IMU与ESP32间的线，再将IMU自身USB接电脑；配置完成后拔掉IMU USB，断电恢复三线采集。历史COM7仅为当时端口，不保证下次相同。详细经过见 [imu.md](imu.md)。

## AS6221：独立I²C1

VCC/GND/SDA/SCL/ADD0/ALERT共六处按表连接。当前地址0x48，转换目标4 Hz、读取周期250 ms；报警功能不启用。MAX30102和温度传感器分别独占I²C0/I²C1，不是接在同一组SDA/SCL线上。

模块接触温度不等于核心体温；热源与结构影响另行验证。详见 [temperature.md](temperature.md)。

## LCD与触摸：分开理解两组连接器

实物型号TK013F2261_V1.3，上接口从丝印看为SDA/SCL/3.3V/GND，下接口为CS/SCK/DC/MOSI。上接口只接电源与地，SDA/SCL留空；下接口四根信号线全部按表接。

LCD是ST7789S，SPI显示与方向已验证，MX=1修正水平镜像。触摸是独立CST816D功能，尚未实现。ESP32-S3的两个I²C控制器当前已经用于PPG与温度；触摸后续需共享其中一条总线，并先统一总线生命周期，不能直接创建第三个I²C控制器。

屏幕接线及诊断见 [display.md](display.md)，七页演示界面及BOOT操作见 [watch_ui.md](watch_ui.md)。界面显示样例不代表传感器读取失效。

## GNSS、存储与暂缓模块

GNSS目前仅通过自身USB接电脑完成NMEA通信检查，9600 baud、8N1，约1 Hz。没有当前ESP32 VCC/TX/RX接线；UART2/IO16仅是后续计划，不要求现在接线。供电与串口连接将在正式接入时按载板资料确认，详见 [gnss.md](gnss.md)。

PC记录通过ESP32板载USB串口进行，无microSD接线。超声波和microSD暂缓；电池、电源管理和外接按键尚未接入。备用ESP32-C6-Zero不参与当前原型，所有表格均针对ESP32-S3主板。

## 资料位置

厂家资料保留在上级项目的`数据手册/10轴IMU惯导模块`、`数据手册/野火小智心率血氧模块资料_20241220`、`数据手册/亚博智能GPS模块`及温度/屏幕对应目录。硬件总表作为当前接线入口，修改引脚时同步board_config、总表与对应模块文档。
