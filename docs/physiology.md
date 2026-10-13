# 心率与实验血氧算法 v0.1

本组件是原型信号处理，不是经过验证的测量方案。心率输出为PPG脉搏频率；血氧始终标记calibrated=0，实验公式不能替代针对该光路的标定和准确性验证。

## 算法

100 Hz输入，800样本(8秒)滑动窗口，每秒更新。去线性基线并做5点平滑，在中间6秒分析；IR自相关找40–180 bpm范围内的周期，再以IR谷值间隔中位数交叉验证。至少三个谷值，间隔变异、周期一致性、RED/IR相关性和幅度均通过才输出。SQI是启发式质量分数，不是准确率或医学置信度。

按窗口的AC RMS/DC计算R=(ACred/DCred)/(ACir/DCir)，代入实验参考式SpO2=-45.060R²+30.354R+94.845。AC定义与参考实现的逐搏峰谷法不同，不能假定参考系数仍有相同精度；标定系数、光路、接触压力和波形形状都可能影响结果。保留R与AC/DC数据供后续比较，结果越界时不钳位成正常值。

公式参考：[Maxim参考实现](https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library/blob/master/src/spo2_algorithm.cpp)。独立编写本组件，没有复制其源码。标定要求见[ADI SpO2指南](https://www.analog.com/en/resources/technical-articles/guidelines-for-spo2-measurement--maxim-integrated.html)。

## 输出

```text
METRIC,time_us,hr_bpm,hr_available,spo2_est_pct,spo2_est_available,calibrated,sqi,ratio,pi_ir_pct,reason
```

不可用数值为-1，available=0；客户端必须显示--，不继续展示旧值。available只表示本窗口通过原型质量检查，calibrated始终为0。ratio为AC/DC比值，pi_ir_pct为100×IR AC RMS/DC，不与商业设备PI直接等同。

NO_CONTACT：任何通道低于5000原始计数，门限仅适用于当前实验硬件。WARMUP：等待连续8秒。DATA_GAP：丢样/恢复/序号缺口后重建窗口。SATURATED：ADC满量程。TIMING_UNSTABLE：窗口跨度偏离100 Hz约5%。SIGNAL_UNSTABLE：基线变化大或两路不一致。WEAK_SIGNAL、NO_PERIOD、IRREGULAR_PULSE：不能得到可靠重复周期。SPO2_RATIO_RANGE：心率可能可用，但血氧公式越界。

当前只用于静止指腹原型；周期性运动也可能欺骗自相关，不能保证运动场景排除成功。接触改变、饱和或缺口会失效，重新放好后需要8秒以上稳定数据。不要通过人为降低氧供等方式在本项目中自行标定；目前只收集正常静止场景与参考设备的同步对照。

## 回放与测试

tools/physiology_replay/replay.c调用同一C组件，将ppg.csv转成离线指标CSV。原始PPG仍持续保存，METRIC每秒一条，不更改采集配置。结果需在实际记录、合成已知频率、无手指、饱和、缺口和噪声场景验证；参考设备对照尚未完成。

## 本次软件验证结果

固件ESP-IDF v5.5.5编译通过。5组主机测试通过，包括60/72/90/120/150 bpm已知信号、实验公式数值、无接触/平直/饱和、缺口后重新等待窗口、血氧公式越界不钳位与随机噪声拒绝。PC指标解析与metrics.csv保存验证通过。主机测试使用同一C算法，不构成真实传感器准确性验证。

回放20261013_121028_650658：6002对原始样本，约100.03 Hz，记录中无采集丢样；算法仅两个更新通过门限，脉搏频率约73.17–73.62 bpm，其余更新为无接触、等待稳定窗口或信号不稳定。没有参考心率，不能据此给出准确率。血氧输出为未标定公式估算，不认定为实测血氧。

下一轮需烧录新增组件，稳定放置指腹后连续保持60秒，并同步记下参考设备读数；观察METRIC有效标志、SQI、原始波形和采集丢样统计，确认算法不会影响原始采集。IRREGULAR_PULSE只表示本窗口间隔一致性未通过，不表示医学上的心律异常。

## 实机演示更新

开发者反馈心率和实验血氧可正常显示。20261013_204713_180850记录包含5995对PPG、60条METRIC，41条心率更新通过质量门限；无采集丢样或通信错误，2条时间戳修正。上述验证证明采集与算法演示链路运行，不证明测量准确性。当前暂缺参考设备，保持calibrated=0，先推进IMU与显示集成。
