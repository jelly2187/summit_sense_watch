# PC端工具

serial_logger/record.py保存串口原始流、PPG CSV、系统CSV、统计JSON与最后10秒波形SVG。实时模式依赖pyserial，离线模式只用标准库。运行示例：

```powershell
python tools/serial_logger/record.py --port COM6 --seconds 60
python tools/serial_logger/record.py --input path/to/raw.txt --output data/raw/replay
```

在工程根目录运行，先关闭VS Code Monitor，替换端口为实际COM。输出目录必须不存在，避免覆盖已有实验；默认自动生成时间目录。独立Python环境用requirements.txt安装依赖。数据不提交Git，单独备份；后续分析工具放在analysis/。

## 实时波形与采样质量

关闭VS Code Monitor后，在工程根目录的ESP-IDF终端运行：

```powershell
python tools/serial_logger/record.py --port COM6 --seconds 60 --live
```

同一个程序同时读取、显示和保存，避免多个程序抢占串口。窗口显示最近6秒RED/IR原始值，各自自动缩放；关闭窗口或Ctrl+C结束记录并保存。当前本机IDF Python已检查可用Tk 8.6和pyserial 3.5。其他环境若不支持Tk，去掉--live仍可采集并生成SVG。

记录还会生成quality.json：按重启会话分别统计有效接收速率、序号缺口、估算时间间隔、采集epoch变化、ADC饱和和错误计数增量。计数起止值区分历史错误与本次记录期间增加的错误；不能把硬件丢样下限当精确值。曲线自动缩放会放大噪声，不把曲线周期性或数据变化直接当成心率/血氧可靠性的证明。

第一轮测试：空载10秒、指腹静止30秒、轻微移动10秒、恢复静止10秒。重点查看静止时能否出现重复脉搏波、两路是否饱和，以及质量统计中的错误是否增加。空载与运动是对照条件，不需要在这些阶段得到稳定脉搏波。

截至当前，用户已反馈放手指后RED/IR数值变化，确认光学响应；完整原始记录、波形与采样质量尚待验证。该工具不输出心率或血氧。

## 心率/实验血氧版本

烧录后仍输出100 Hz原始PPG，同时每秒输出METRIC指标。静止放好手指，等待至少8秒连续有效数据。实时窗口会显示心率、未标定血氧估算和状态；不可用及超过3秒没有新指标时显示--，不保留旧数值。记录增加metrics.csv。

采样完整并不等于整个窗口信号稳定。保持指腹、压力和环境光稳定；先对照参考设备，记录同一时段数值，不把实验公式当已验证血氧测量。算法参数与输出含义见physiology.md。准确性对照与真实设备运行验证待完成。
