"""Standard-library Tk live preview; acquisition owns the serial port."""
from collections import deque
import time
import tkinter as tk
from tkinter import ttk
from imu_view import IMUView
from temperature_view import TemperatureView

class LiveView:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title('SummitSense - 生理与运动同步采集')
        self.root.geometry('1280x820')
        self.root.minsize(950,650)
        self.closed = False
        self.rows = deque(maxlen=1200)
        self.last_draw = 0
        self.session = None
        self.metric = None
        self.metric_received = 0
        self.label = tk.Label(self.root, text='等待PPG数据；轻盖传感器，保持静止。', anchor='w')
        self.label.pack(fill='x', padx=12, pady=8)
        self.metric_label=tk.Label(self.root,text='心率 -- | 血氧估算 --（未标定）',anchor='w')
        self.metric_label.pack(fill='x',padx=12,pady=4)
        self.notebook=ttk.Notebook(self.root)
        self.notebook.pack(fill='both',expand=True,padx=8)
        ppg_frame=ttk.Frame(self.notebook)
        self.notebook.add(ppg_frame,text='PPG / 生理估算')
        self.imu=IMUView(self.notebook)
        self.notebook.add(self.imu.frame,text='IMU / 三维姿态 / 气压高度')
        self.temperature=TemperatureView(self.notebook)
        self.notebook.add(self.temperature.frame,text='AS6221 / 温度')
        self.canvas = tk.Canvas(ppg_frame, background='white', highlightthickness=0)
        self.canvas.pack(fill='both', expand=True, padx=12)
        tk.Label(self.root, text='红光 / 红外为原始ADC值，分别自动缩放。噪声也可能被放大；生理估算由固件计算，血氧未标定。', anchor='w').pack(fill='x', padx=12, pady=8)
        self.root.protocol('WM_DELETE_WINDOW', self.close)

    def close(self):
        self.closed = True
        self.root.destroy()

    def add(self, row):
        if self.session != row['session']:
            self.rows.clear()
            self.metric=None
            self.session = row['session']
        self.rows.append(row)

    def add_temperature(self,row):
        self.temperature.add(row)

    def add_imu(self,row):
        self.imu.add(row)

    def add_imu_status(self,status):
        self.imu.status=status

    def add_metric(self, metric):
        self.metric=metric
        self.metric_received=time.monotonic()

    def update(self, stats):
        if self.closed: return
        now=time.monotonic()
        if now-self.last_draw < .1: return
        self.last_draw=now
        try:
            self.root.update_idletasks()
            self.draw(stats)
            self.imu.draw()
            self.temperature.draw()
            self.root.update()
        except tk.TclError:
            self.closed=True

    def draw(self, stats):
        self.canvas.delete('all')
        metric=self.metric
        if metric and time.monotonic()-self.metric_received <= 3:
            hr=f"{metric['hr_bpm']:.1f} bpm" if metric['hr_available'] else '--'
            spo=f"{metric['spo2_est_pct']:.1f}%" if metric['spo2_est_available'] else '--'
            self.metric_label.config(text=f"心率 {hr} | 血氧实验估算 {spo}（未标定） | SQI {metric['sqi']:.0f} | {metric['reason']}")
        else:
            self.metric_label.config(text='心率 -- | 血氧估算 --（未标定） | 等待新指标')
        width=max(400,self.canvas.winfo_width())
        height=max(300,self.canvas.winfo_height())
        if not self.rows:
            self.canvas.create_text(width/2,height/2,text='等待串口PPG数据…')
            return
        end=self.rows[-1]['time_us_est']
        rows=[r for r in self.rows if r['time_us_est'] >= end-6_000_000]
        start=rows[0]['time_us_est']; span=max(1,end-start)
        self.label.config(text=f"样本 {stats['ppg_records']} | 序号缺口 {stats['sequence_gaps']} | 本次启动FIFO丢样下限 {rows[-1]['fifo_lost_min']} | 队列丢样 {rows[-1]['queue_lost']} | I²C错误 {rows[-1]['i2c_errors']}")
        for channel,color,index in [('red','#cf3434',0),('ir','#2469c8',1)]:
            top=15+index*height/2; bottom=top+height/2-55
            lo=min(r[channel] for r in rows); hi=max(r[channel] for r in rows)
            self.canvas.create_text(65,top,anchor='nw',text=f'{channel.upper()}  原始范围 {lo}–{hi}',fill=color)
            self.canvas.create_rectangle(65,top+25,width-25,bottom,outline='#ccc')
            points=[]; previous=None
            for r in rows:
                gap=previous and (r['seq'] != previous['seq']+1 or r['epoch'] != previous['epoch'] or r['flags'] & 11)
                if gap:
                    if len(points)>=4: self.canvas.create_line(*points,fill=color,width=1.4)
                    points=[]
                x=65+(width-90)*(r['time_us_est']-start)/span
                y=bottom-5-(bottom-top-35)*(r[channel]-lo)/max(1,hi-lo)
                points.extend([x,y]); previous=r
            if len(points)>=4: self.canvas.create_line(*points,fill=color,width=1.4)
            self.canvas.create_text(width-25,bottom+8,anchor='ne',text=f'最近 {span/1e6:.1f} 秒；缺口处不连线')
