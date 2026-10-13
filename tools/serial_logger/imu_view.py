from collections import deque
import math
import time
import tkinter as tk
from tkinter import ttk
from imu_model import rotate_vector

class IMUView:
    def __init__(self,parent):
        self.frame=ttk.Frame(parent)
        self.latest={}; self.history={}; self.status=None; self.height_zero=None
        self.label=ttk.Label(self.frame,text='等待IMU数据：TX→IO18；开启气压还需要RX→IO17。')
        self.label.pack(fill='x',padx=12,pady=8)
        panel=ttk.Panedwindow(self.frame,orient='horizontal');panel.pack(fill='both',expand=True)
        left=ttk.Frame(panel);right=ttk.Frame(panel)
        panel.add(left,weight=1);panel.add(right,weight=2)
        self.pose_label=ttk.Label(left,text='Roll -- / Pitch -- / Yaw --');self.pose_label.pack(pady=6)
        self.pose=tk.Canvas(left,background='#f6f8fb',highlightthickness=0,width=430,height=400)
        self.pose.pack(fill='both',expand=True,padx=8)
        self.height_label=ttk.Label(left,text='气压 -- Pa\n模块气压高度 -- m（未校准）\n相对起点高度 -- m',justify='left')
        self.height_label.pack(fill='x',padx=12,pady=12)
        ttk.Button(left,text='以当前模块高度作为相对零点',command=self.zero_height).pack(pady=8)
        ttk.Label(left,text='3D示意采用 Z-Y-X 欧拉角旋转；Yaw受磁场与安装影响。',wraplength=380).pack(padx=12,pady=8)
        self.graph=tk.Canvas(right,background='white',highlightthickness=0)
        self.graph.pack(fill='both',expand=True,padx=8,pady=4)

    def add(self,row):
        kind=row['kind']; previous=self.latest.get(kind)
        if previous and row['session']!=previous[0]['session']:
            self.latest.clear();self.history.clear();self.height_zero=None
        self.latest[kind]=(row,time.monotonic())
        self.history.setdefault(kind,deque(maxlen=1000)).append(row)
        if kind=='PRESS' and row['valid_mask'] & 3==3 and self.height_zero is None:
            self.height_zero=row['v2']

    def fresh(self,kind):
        item=self.latest.get(kind)
        return item[0] if item and time.monotonic()-item[1]<2 else None

    def zero_height(self):
        row=self.fresh('PRESS')
        if row and row['valid_mask'] & 3==3: self.height_zero=row['v2']

    def project(self,point,width,height):
        x,y,z=point;scale=min(width,height)*.23
        return width/2+scale*(.8*x-.6*y),height/2+scale*(.35*x+.45*y-.9*z)

    def draw_pose(self,angle):
        c=self.pose;c.delete('all');w=max(300,c.winfo_width());h=max(260,c.winfo_height())
        roll,pitch,yaw=(angle['v1'],angle['v2'],angle['v3']) if angle else (0,0,0)
        self.pose_label.config(text=f'Roll {roll:.1f}° / Pitch {pitch:.1f}° / Yaw {yaw:.1f}°' if angle else '姿态 --（未接收或数据过期）')
        vertices=[(-1.2,-.7,-.18),(1.2,-.7,-.18),(1.2,.7,-.18),(-1.2,.7,-.18),
                  (-1.2,-.7,.18),(1.2,-.7,.18),(1.2,.7,.18),(-1.2,.7,.18)]
        rotated=[rotate_vector(v,roll,pitch,yaw) for v in vertices]
        faces=[(0,1,2,3),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
        faces.sort(key=lambda f:sum(.6*rotated[j][0]+.8*rotated[j][1]+.2*rotated[j][2] for j in f))
        colors=['#b4cce9','#759fd5','#426da7','#abc6e8','#5885be','#8caedb']
        for index,face in enumerate(faces):
            points=[]
            for j in face: points.extend(self.project(rotated[j],w,h))
            c.create_polygon(*points,fill=colors[index] if angle else '#d5d9df',outline='#3a506b')
        origin=self.project((0,0,0),w,h)
        if angle:
            for vector,label,color in [((1.7,0,0),'X / Roll','#c43b3b'),((0,1.7,0),'Y / Pitch','#26934b'),((0,0,1.7),'Z / Yaw','#306ac1')]:
                end=self.project(rotate_vector(vector,roll,pitch,yaw),w,h)
                c.create_line(*origin,*end,fill=color,width=3,arrow='last')
                c.create_text(end[0],end[1]-12,text=label,fill=color)
        else: c.create_text(w/2,h-30,text='姿态不可用：不显示旧角度',fill='#687584')

    def draw(self):
        angle=self.fresh('ANGLE');angle=angle if angle and angle['valid_mask'] & 7==7 else None
        self.draw_pose(angle)
        acc=self.fresh('ACC');gyro=self.fresh('GYRO');press=self.fresh('PRESS')
        def triple(row): return ' / '.join(f'{row[k]:.3f}' for k in ['v1','v2','v3']) if row and row['valid_mask'] & 7==7 else '--'
        status=self.status
        errors=f"校验错误 {status['checksum_errors']} / UART错误 {status['uart_errors']} / 气压帧 {status['pressure_frames']}" if status else '等待统计'
        self.label.config(text=f"加速度 X/Y/Z：{triple(acc)} g | 角速度：{triple(gyro)} °/s | {errors}")
        if press and press['valid_mask'] & 3==3:
            relative=press['v2']-self.height_zero if self.height_zero is not None else 0
            self.height_label.config(text=f"气压 {press['v1']:.1f} Pa\n模块气压高度 {press['v2']:.2f} m（未校准）\n相对起点高度 {relative:+.2f} m")
        else:
            self.height_label.config(text='气压 -- Pa\n模块气压高度 -- m\n尚未收到有效气压帧：检查RX→IO17或模块配置')
        c=self.graph;c.delete('all');w=max(400,c.winfo_width());h=max(450,c.winfo_height())
        specs=[('ACC','加速度 (g)',('v1','v2','v3')),('GYRO','角速度 (°/s)',('v1','v2','v3')),
               ('ANGLE','Roll / Pitch / Yaw (°)',('v1','v2','v3')),
               ('PRESS','气压 (Pa)',('v1',)),('PRESS','模块气压高度 (m)',('v2',))]
        for index,(kind,label,fields) in enumerate(specs):
            top=index*h/5+24;bottom=(index+1)*h/5-15;left=70;right=w-15
            c.create_text(left,top-13,anchor='w',text=label,fill='#324354')
            c.create_rectangle(left,top,right,bottom,outline='#d7dee8')
            row=self.fresh(kind)
            if not row or row['valid_mask']==0:
                c.create_text((left+right)/2,(top+bottom)/2,text='等待有效数据');continue
            history=[r for r in self.history[kind] if r['rx_us']>=row['rx_us']-6_000_000 and r['valid_mask']]
            values=[r[f] for r in history for f in fields]
            if not values:continue
            lo=min(values);hi=max(values);padding=max(.01,(hi-lo)*.1);lo-=padding;hi+=padding
            c.create_text(left-5,top,anchor='e',text=f'{hi:.2f}',fill='#6d7a8a')
            c.create_text(left-5,bottom,anchor='e',text=f'{lo:.2f}',fill='#6d7a8a')
            start=history[0]['rx_us'];span=max(1,row['rx_us']-start)
            for field,color in zip(fields,['#ce3e40','#2b9a57','#326ac5']):
                points=[];prev=None
                for r in history:
                    # Do not connect across a long per-kind receive gap or yaw wrap.
                    if prev and (r['rx_us']-prev['rx_us']>500000 or (kind=='ANGLE' and abs(r[field]-prev[field])>180)):
                        if len(points)>=4:c.create_line(*points,fill=color,width=1.4)
                        points=[]
                    points.extend([left+(right-left)*(r['rx_us']-start)/span,bottom-(bottom-top)*(r[field]-lo)/(hi-lo)])
                    prev=r
                if len(points)>=4:c.create_line(*points,fill=color,width=1.4)
