# 架构与目录规则

## 分层与依赖

- `main`是应用编排层：调用启动检查、创建后续任务、管理应用模式，不直接实现传感器寄存器操作。
- `board_config`是板级配置：集中维护GPIO、总线初始速率与基础周期；不引用传感器组件。
- `max30102`等硬件组件负责协议、寄存器和设备状态，公开接口放在各自 `include/`中。
- `system_monitor`负责系统运行状态，独立于生理算法与传感器实现。
- 算法、生理、显示驱动与UI各自独立组件；watch_ui当前只读演示样例，后续通过统一有效性快照读取数据，避免直接访问传感器。
- `tools`为PC端代码，不参与固件构建；`docs`保存架构、接线、协议和验证证据。

目前main在调用任务中运行心跳循环；MAX30102组件建立独立FIFO采集与串口输出任务，通过128帧队列连接。共享I²C总线管理尚未实现。

## 组件约定

每个组件使用 `CMakeLists.txt`声明源文件、公开头文件和依赖。公开接口依赖使用 `REQUIRES`，实现细节依赖使用 `PRIV_REQUIRES`。禁止跨组件包含私有源目录中的头文件。

示例：

```text
components/yahboom_imu/
├── include/yahboom_imu.h
├── yahboom_imu.c
└── CMakeLists.txt
```

当单个组件明显变大时，再拆 `src/`或内部私有头文件。组件的名字和接口围绕职责，避免把无关代码统一堆在common、utils或main中。

当前启用ESP-IDF `MINIMAL_BUILD`，因此新增组件必须通过依赖关系纳入构建；缺少依赖可能导致源代码和Menuconfig选项都不出现。PSRAM由system_monitor组件显式依赖 `esp_psram`。

## 后续扩展

- `yahboom_imu`、`as6221`已实现协议与读取；`gnss`待接入固件。
- `data_manager`、`data_logger`：时间戳、状态、缓存和传输。
- `physiology`已实现心率与实验血氧窗口计算；`step_engine`后续实现运动算法。
- `display_panel`：ST7789S初始化与同步SPI DMA传输；`watch_ui`：LVGL七页中文演示界面与BOOT导航，单任务拥有LVGL。`display_test`复用panel实现诊断，默认不启动。

尚未实现的组件按实际工作建立，不提前创建空实现。多个器件共享I²C时，应把总线生命周期提升到板级总线组件，避免每个驱动重复初始化同一个硬件端口。当前MAX30102启动检查是一次性建总线、检查、释放资源；随后采集程序重新建总线，由采集任务独占使用，并在运行错误后尝试恢复配置。

## 工程与工具配置

工程名保持 `summit_sense_watch`，输出固件名不变。保留 `.vscode/`、`.clangd`与 `.devcontainer/`，它们属于开发工具配置；开发容器未在本机验证。模板Hello World的测试已不适用当前输出，因此移除，验证步骤集中在validation文档；后续增加针对数据完整性和协议解析的实际测试。
