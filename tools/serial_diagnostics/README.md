# 自动串口诊断

先关闭Monitor Device和实时采集工具，在工程根目录执行：

```powershell
python tools/serial_diagnostics/diagnose.py --port COM6 --seconds 15
python tools/serial_diagnostics/diagnose.py --port COM6 --seconds 15 --reset
python tools/serial_diagnostics/diagnose.py --input data/raw/记录目录/raw.txt
```

--reset通过CH343的RTS控制EN进行正常重启，GPIO0保持释放，不烧录固件。重启会使固件再次运行既有IMU初始化。未指定--reset时工具不主动切换复位控制线。不要同时打开另一个串口程序。

保存raw.txt、imu_filtered.txt和diagnosis.json到data/raw/时间_diagnostic目录。诊断区分有效气压、无效压力值、配置确认但无气压、回读未确认以及无IMU数据；没有回读不单独证明接线错误。若端口被占用工具明确报错，不强行结束其他程序。

## 直接连接IMU自身USB

先断开IMU与ESP32之间的四根线，只通过IMU自身Type-C接电脑；程序可自动选择唯一CP210x串口。关闭厂家上位机后运行：

```powershell
python tools/serial_diagnostics/imu_usb.py --port COM7 --enable-pressure
python tools/serial_diagnostics/imu_usb.py --port COM7 --enable-pressure --save
```

第一条临时开启，第二条仅在实际有效气压和RSW回读通过后发送保存。工具不改波特率、不重启或校准IMU。保存后拔掉IMU USB、断电恢复三线接收，再用diagnose.py验证。不同电脑COM号需重新识别。
