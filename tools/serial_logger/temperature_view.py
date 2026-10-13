from collections import deque
import time
import tkinter as tk
from tkinter import ttk

class TemperatureView:
    def __init__(self,parent):
        self.frame=ttk.Frame(parent);self.rows=deque(maxlen=600)
        self.latest=None;self.received=0;self.session=None
        self.value_label=ttk.Label(self.frame,text='AS6221温度 -- °C',font=('Segoe UI',28))
        self.value_label.pack(anchor='w',padx=24,pady=16)
        self.status_label=ttk.Label(self.frame,text='等待温度数据：I²C1，SDA IO4、SCL IO5、地址0x48')
        self.status_label.pack(anchor='w',padx=24,pady=4)
        self.canvas=tk.Canvas(self.frame,background='white',highlightthickness=0)
        self.canvas.pack(fill='both',expand=True,padx=20,pady=12)
        ttk.Label(self.frame,text='显示模块/接触位置温度，尚未验证贴肤结构与热源影响；不等同核心体温。').pack(anchor='w',padx=24,pady=12)
    def add(self,row):
        if self.session!=row['session']:self.rows.clear();self.session=row['session']
        self.rows.append(row);self.latest=row;self.received=time.monotonic()
    def draw(self):
        row=self.latest;fresh=row and time.monotonic()-self.received<3
        valid=fresh and row['valid']
        self.value_label.config(text=f"AS6221温度 {row['temperature_c']:.2f} °C" if valid else 'AS6221温度 -- °C')
        if fresh:
            self.status_label.config(text=f"状态 {row['status']} | I²C累计错误 {row['i2c_errors']} | 配置 0x{row['config']:04X} | 转换配置 {(0.25,1,4,8)[(row['config'] >> 6) & 3]:g} Hz / 读取目标4 Hz")
        else:self.status_label.config(text='未接收或数据已过期；不继续展示旧温度')
        c=self.canvas;c.delete('all');w=max(500,c.winfo_width());h=max(300,c.winfo_height())
        left=85;right=w-35;top=35;bottom=h-55
        c.create_rectangle(left,top,right,bottom,outline='#c7d1dd')
        c.create_text(left,15,anchor='w',text='最近120秒温度趋势（°C）')
        if not row:return
        rows=[r for r in self.rows if r['rx_us']>=row['rx_us']-120_000_000]
        values=[r['temperature_c'] for r in rows if r['valid']]
        if not values:c.create_text(w/2,h/2,text='没有有效温度');return
        lo=min(values);hi=max(values);margin=max(.2,(hi-lo)*.1);lo-=margin;hi+=margin
        for k in range(5):
            y=top+k*(bottom-top)/4
            c.create_line(left,y,right,y,fill='#edf0f4')
            c.create_text(left-8,y,anchor='e',text=f'{hi-k*(hi-lo)/4:.2f}')
        start=rows[0]['rx_us'];span=max(1,row['rx_us']-start);points=[];previous=None
        for r in rows:
            gap=not r['valid'] or (previous and (r['seq']!=previous['seq']+1 or r['rx_us']-previous['rx_us']>2500000))
            if gap:
                if len(points)>=4:c.create_line(*points,fill='#c27118',width=2)
                points=[]
            if r['valid']:
                points.extend([left+(right-left)*(r['rx_us']-start)/span,bottom-(bottom-top)*(r['temperature_c']-lo)/(hi-lo)])
            previous=r
        if len(points)>=4:c.create_line(*points,fill='#c27118',width=2)
        c.create_text(right,bottom+25,anchor='e',text=f'记录跨度 {span/1e6:.1f} s；错误处断线')
