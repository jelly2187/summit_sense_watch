# 3.5 寸定位页：离线地图与 GPX 路线

当前固件已通过 COM5 烧录；启动日志确认 NVS 触摸方向已加载、UART2 已启动、界面及心跳正常。地图显示和交互的电脑端预览通过，LC76G 户外真实定位仍待验证。

## 使用

- 翻到第 5 页「定位」。地图内拖动，松手后平移；右侧 + / - 为 1× / 2× / 4× 缩放，FIT 回到路线全览。底部左右箭头仍可翻页。
- 默认使用合成等高线、河流、小径与路线；蓝点为演示行进点。地图明确标注 DEMO MAP 与 NO NAVIGATION。真实经纬度单独显示在地图下方；默认演示底图没有地理坐标，真实坐标不会被投到演示底图。
- 接 LC76G 后，在开阔处等待有效定位。初测仅接 VCC→稳压 3.3V、GND→GND、TX→GPIO16，并接配套天线；模块 RX 留空。UART2 采用 115200、8N1、只接收。只接受通过校验的 RMC 有效状态，失去有效定位或连续 5 秒无更新会显示 NO FIX；原样例的 9600 不能直接套用。模块与参数见[微雪 LC76G 文档](https://www.waveshare.net/wiki/LC76G_GPS_Module)。

## 导入自己的 GPX 路线

现在支持**电脑转换并随固件烧录**，不需要 microSD。GPX 是路线或轨迹文件，不包含街道底图；导入后显示真实比例的路线与网格，只有有效 GNSS 定位才会显示真实蓝点。若定位在当前视图外，显示 OFF MAP，可用 FIT 或拖动查看路线范围。

在工程根目录运行：

```powershell
python tools/map_assets/import_gpx.py "D:\routes\my_track.gpx" --output components/watch_ui/map_data/imported_route.h
idf.py -p COM5 flash
```

工具读取本地 WGS84 GPX 轨迹段或路线，分段保留，不跨断点连线。默认以约 3 米容差简化；超过 256 点时可增大 `--tolerance-m`。支持最多 16 段、单轴范围不超过 100 km、不跨日期变更线的局部路线。生成文件被 Git 忽略，避免把个人路线带入仓库。移除本地生成文件并重新构建可恢复演示地图。[GPX 1.1 格式](https://www.topografix.com/GPX/1/1/)

后续可在 microSD 接通后增加设备端文件选择、离线地理底图、路线进度与偏离提醒。当前没有道路底图导入、自动规划、逐转弯导航或在线地图请求。选取离线底图时，应使用允许离线分发的数据源，不从 OSM 公共瓦片服务器批量下载。[OSMF 瓦片使用政策](https://operations.osmfoundation.org/policies/tiles/)
