# 同一C算法的PC回放

需要能编译主机程序的gcc，不使用ESP32交叉编译器。在工程根目录运行：

```powershell
gcc -std=c11 -O2 -I components/physiology/include components/physiology/physiology.c tools/physiology_replay/replay.c -lm -o build/physiology_replay.exe
./build/physiology_replay.exe data/raw/记录目录/ppg.csv
python -m unittest discover -s tests -v
```

回放输出CSV指标；Python测试会编译同一份固件C代码，验证已知频率、无接触、饱和、缺口、噪声和公式越界。编译器缺失时测试标为跳过，不表示通过。
