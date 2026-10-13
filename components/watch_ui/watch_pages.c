#include "watch_pages.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(watch_cn_16);
LV_FONT_DECLARE(watch_cn_20);
#define BG 0x071014
#define CARD 0x142328
#define TEXT 0xEBF4F3
#define MUTED 0x859DA0
#define MINT 0x46E0C1
#define CORAL 0xFF7F89
#define AMBER 0xFFC66D
static lv_obj_t *clock_label;
static const char *names[]={"表盘","健康","运动","环境","定位","设备","设置"};
static lv_point_precise_t pulse_points[39];
const char *watch_pages_name(unsigned page) {return names[page%WATCH_PAGE_COUNT];}

static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color,int radius)
{
    lv_obj_t *o=lv_obj_create(parent);
    lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_radius(o,radius,0);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);return o;
}
static lv_obj_t *label(lv_obj_t *parent,int x,int y,int w,const char *text,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *o=lv_label_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);lv_label_set_text(o,text);
    lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    return o;
}
static void center(lv_obj_t *o) {lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);}
static void caption(lv_obj_t *root,const char *text)
{center(label(root,10,198,220,text,&watch_cn_16,MUTED));}
static void value(lv_obj_t *root,int x,int y,int w,const char *text,uint32_t color)
{label(root,x,y,w,text,&lv_font_montserrat_32,color);}
static void ring(lv_obj_t *root,int x,int y,int size,int percent,uint32_t color)
{
    lv_obj_t *arc=lv_arc_create(root);lv_obj_set_pos(arc,x,y);lv_obj_set_size(arc,size,size);
    lv_arc_set_rotation(arc,135);lv_arc_set_bg_angles(arc,0,270);lv_arc_set_range(arc,0,100);lv_arc_set_value(arc,percent);
    lv_obj_set_style_arc_width(arc,7,LV_PART_MAIN);lv_obj_set_style_arc_width(arc,7,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc,lv_color_hex(CARD),LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc,lv_color_hex(color),LV_PART_INDICATOR);
    lv_obj_remove_style(arc,NULL,LV_PART_KNOB);lv_obj_remove_flag(arc,LV_OBJ_FLAG_CLICKABLE);
}
static void row(lv_obj_t *root,int y,const char *name,const char *text,uint32_t color)
{
    box(root,12,y,216,35,CARD,9);
    label(root,22,y+7,116,name,&watch_cn_16,MUTED);
    lv_obj_t *v=label(root,136,y+7,82,text,&watch_cn_16,color);
    lv_obj_set_style_text_align(v,LV_TEXT_ALIGN_RIGHT,0);
}
void watch_pages_show(unsigned page,bool available,bool autoplay)
{
    page%=WATCH_PAGE_COUNT;clock_label=NULL;
    lv_obj_t *root=lv_screen_active();lv_obj_clean(root);
    lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root,lv_color_hex(BG),0);lv_obj_set_style_bg_opa(root,255,0);
    lv_obj_set_style_text_font(root,&watch_cn_16,0);
    label(root,12,8,140,page==0 ? "山知" : names[page],&watch_cn_20,TEXT);
    box(root,180,8,48,24,0x233B37,8);
    center(label(root,180,10,48,"演示",&watch_cn_16,MINT));
    for(unsigned j=0;j<WATCH_PAGE_COUNT;++j)
        box(root,81+j*12,225, j==page ? 8 : 5,5,j==page ? MINT : 0x3B5054,3);

    switch(page) {
    case 0: {
        center(label(root,12,42,216,"10月14日  周三",&watch_cn_16,MUTED));
        clock_label=label(root,12,64,216,"10:24",&lv_font_montserrat_48,TEXT);center(clock_label);
        box(root,12,127,104,62,CARD,12);box(root,124,127,104,62,CARD,12);
        label(root,22,133,90,"心率 bpm",&watch_cn_16,MUTED);
        label(root,134,133,88,"温度 °C",&watch_cn_16,MUTED);
        value(root,22,152,88,available ? "74" : "--",CORAL);
        value(root,134,152,88,available ? "31.4" : "--",MINT);
        caption(root,autoplay ? "BOOT翻页 · 长按暂停" : "BOOT翻页 · 长按轮播");break;
    }
    case 1: {
        label(root,12,41,90,"心率",&watch_cn_16,MUTED);
        label(root,12,59,125,available ? "74" : "--",&lv_font_montserrat_48,CORAL);
        label(root,111,80,70,"bpm",&watch_cn_16,MUTED);
        static const int shape[]={0,0,1,4,10,17,23,20,15,11,8,10,8,6,4,3,2,1,0};
        for(int j=0;j<39;++j) {pulse_points[j].x=j*5; pulse_points[j].y=25-(available ? shape[j%19] : 0);}
        lv_obj_t *line=lv_line_create(root);lv_obj_set_pos(line,20,110);lv_line_set_points(line,pulse_points,39);
        lv_obj_set_style_line_color(line,lv_color_hex(CORAL),0);lv_obj_set_style_line_width(line,2,0);
        box(root,12,155,132,37,CARD,9);box(root,152,155,76,37,CARD,9);
        label(root,20,164,50,"血氧",&watch_cn_16,MUTED);
        label(root,64,163,78,available ? "98.4%" : "--",&watch_cn_20,MINT);
        center(label(root,152,165,76,available ? "SQI 92" : "SQI --",&watch_cn_16,TEXT));
        caption(root,available ? "示例数值 · 血氧未标定" : "无有效数据 · 请检查接触");break;
    }
    case 2: {
        ring(root,10,43,122,available ? 41 : 0,MINT);
        center(label(root,21,69,100,"步数示例",&watch_cn_16,MUTED));
        lv_obj_t *n=label(root,20,91,102,available ? "3280" : "--",&lv_font_montserrat_32,TEXT);center(n);
        label(root,145,52,83,"步频示例",&watch_cn_16,MUTED);
        value(root,145,76,83,available ? "106" : "--",MINT);
        label(root,145,112,80,"步/分钟",&watch_cn_16,MUTED);
        box(root,12,157,216,35,CARD,9);
        center(label(root,12,165,216,available ? "姿态  5.3° / -2.0° / -2.3°" : "姿态  -- / -- / --",&watch_cn_16,TEXT));
        caption(root,"步数算法待开发");break;
    }
    case 3: {
        label(root,12,43,216,"气压海拔 · 未校准",&watch_cn_16,MUTED);
        label(root,12,66,158,available ? "5.9" : "--",&lv_font_montserrat_48,MINT);
        label(root,144,90,48,"m",&watch_cn_20,MUTED);
        row(root,122,"气压 hPa",available ? "1012.6" : "--",TEXT);
        row(root,161,"接触温度 °C",available ? "31.4" : "--",AMBER);
        caption(root,"气压海拔与GNSS高度独立");break;
    }
    case 4: {
        ring(root,80,42,80,0,MINT);
        center(label(root,85,66,70,"GNSS",&watch_cn_16,MINT));
        center(label(root,12,120,216,"定位未接入",&watch_cn_20,TEXT));
        center(label(root,12,150,216,"经纬度 --  /  高度 --",&watch_cn_16,MUTED));
        center(label(root,12,174,216,"卫星 --  /  速度 --",&watch_cn_16,MUTED));
        caption(root,"户外定位验证待完成");break;
    }
    case 5: {
        row(root,44,"心率 / 血氧","已单测",MINT);
        row(root,83,"IMU / 气压","已单测",MINT);
        row(root,122,"温度 / 屏幕","已单测",MINT);
        row(root,161,"GNSS / 触摸","待接入",AMBER);
        caption(root,"开发进度 · 非实时连接状态");break;
    }
    case 6: {
        box(root,12,44,216,53,CARD,12);
        label(root,22,51,128,"自动轮播",&watch_cn_20,TEXT);
        label(root,22,76,135,"长按BOOT切换",&watch_cn_16,MUTED);
        box(root,174,60,42,23,autoplay ? MINT : 0x3B5054,12);
        box(root,autoplay ? 195 : 177,63,17,17,TEXT,9);
        row(root,105,"页面间隔","8秒",TEXT);
        row(root,144,"显示模式","演示数据",AMBER);
        caption(root,autoplay ? "短按翻页 · 长按关闭轮播" : "短按翻页 · 长按开启轮播");break;
    }
    }
}
void watch_pages_tick(uint32_t elapsed_seconds)
{
    if(clock_label) {
        unsigned total=10*3600+24*60+elapsed_seconds;
        char buffer[12];snprintf(buffer,sizeof(buffer),"%02u:%02u",(total/3600)%24,(total/60)%60);
        if(strcmp(lv_label_get_text(clock_label),buffer)!=0)lv_label_set_text(clock_label,buffer);
    }
}
