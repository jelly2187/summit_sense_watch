#include "watch_pages35.h"
#include "watch_pages.h"
#include "watch_map.h"

LV_FONT_DECLARE(watch_cn_16);
LV_FONT_DECLARE(watch_cn_20);
LV_FONT_DECLARE(watch_title_28);
LV_FONT_DECLARE(watch_clock_84);

enum { BLACK=0x000000, PANEL=0x1C1C1E, WHITE=0xF6F6F8, GRAY=0xA2A2AA,
       PINK=0xFF426A, GREEN=0xB4FA35, BLUE=0x52CDFF, ORANGE=0xFFAD42 };
static lv_point_precise_t wave[23];

static lv_obj_t *surface(lv_obj_t *p,int x,int y,int w,int h,unsigned color,int r)
{
    lv_obj_t *o=lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_radius(o,r,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    return o;
}

static lv_obj_t *txt(lv_obj_t *p,int x,int y,int w,const char *s,const lv_font_t *font,
                     unsigned color,lv_text_align_t align)
{
    lv_obj_t *o=lv_label_create(p);
    lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(o,font,0);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_set_style_text_align(o,align,0);
    lv_label_set_text(o,s);
    return o;
}

static void left(lv_obj_t *p,int x,int y,int w,const char *s,const lv_font_t *font,unsigned color)
{ (void)txt(p,x,y,w,s,font,color,LV_TEXT_ALIGN_LEFT); }

static void center(lv_obj_t *p,int x,int y,int w,const char *s,const lv_font_t *font,unsigned color)
{ (void)txt(p,x,y,w,s,font,color,LV_TEXT_ALIGN_CENTER); }

static void metric(lv_obj_t *p,int x,int y,int w,const char *name,const char *value,
                   const char *unit,unsigned color)
{
    surface(p,x,y,w,100,PANEL,23);
    left(p,x+14,y+10,w-28,name,&watch_cn_16,color);
    left(p,x+14,y+34,w-28,value,&lv_font_montserrat_32,WHITE);
    left(p,x+14,y+72,w-28,unit,&watch_cn_16,GRAY);
}

static void ring(lv_obj_t *p,int x,int y,int size,int value,unsigned color,unsigned track)
{
    lv_obj_t *o=lv_arc_create(p);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_style(o,NULL,LV_PART_KNOB);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,size,size);
    lv_arc_set_rotation(o,270);
    lv_arc_set_bg_angles(o,0,360);lv_arc_set_range(o,0,100);lv_arc_set_value(o,value);
    lv_obj_set_style_pad_all(o,0,LV_PART_MAIN);lv_obj_set_style_pad_all(o,0,LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(o,10,LV_PART_MAIN);lv_obj_set_style_arc_width(o,10,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(o,lv_color_hex(track),LV_PART_MAIN);
    lv_obj_set_style_arc_color(o,lv_color_hex(color),LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(o,true,LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(o,value?LV_OPA_COVER:LV_OPA_TRANSP,LV_PART_INDICATOR);
}

static void status(lv_obj_t *p,int y,const char *name,const char *value,unsigned color)
{
    surface(p,16,y,288,47,PANEL,20);
    left(p,29,y+11,155,name,&watch_cn_20,WHITE);
    (void)txt(p,184,y+12,107,value,&watch_cn_16,color,LV_TEXT_ALIGN_RIGHT);
}

static void chrome(lv_obj_t *p,unsigned page)
{
    const char *name=page==0 ? "山知" : watch_pages_name(page);
    left(p,18,12,250,"SUMMITSENSE",&watch_cn_16,GRAY);
    left(p,16,32,192,name,&watch_title_28,WHITE);
    surface(p,236,36,68,27,page==4 ? 0x142D40 : 0x302217,14);
    center(p,236,39,68,page==4 ? "GNSS" : "演示",&watch_cn_16,page==4 ? BLUE : ORANGE);
    surface(p,20,383,64,64,PANEL,32);
    surface(p,236,383,64,64,PANEL,32);
    center(p,20,397,64,"<",&lv_font_montserrat_32,WHITE);
    center(p,236,397,64,">",&lv_font_montserrat_32,WHITE);
    for(unsigned i=0;i<WATCH_PAGE_COUNT;++i) {
        int x=104+(int)i*17;
        surface(p,x,411,i==page?12:7,7,i==page?WHITE:0x525257,4);
    }
    center(p,16,455,288,page==4 ? "SWIPE OUTSIDE MAP" : "SWIPE / TAP",&watch_cn_16,GRAY);
}

lv_obj_t *watch_pages35_show(lv_obj_t *root,unsigned page,bool data,bool autoplay)
{
    lv_obj_set_style_bg_color(root,lv_color_hex(BLACK),0);
    lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
    chrome(root,page);
    switch(page) {
    case 0: {
        left(root,19,89,282,"10月14日  周三",&watch_cn_20,ORANGE);
        lv_obj_t *clock=txt(root,14,109,292,"10:24",&watch_clock_84,WHITE,LV_TEXT_ALIGN_LEFT);
        metric(root,16,238,138,"心率",data?"74":"--","bpm",PINK);
        metric(root,166,238,138,"温度",data?"31.4":"--","°C",ORANGE);
        left(root,126,248,18,">",&watch_cn_16,GRAY);
        left(root,276,248,18,">",&watch_cn_16,GRAY);
        return clock;
    }
    case 1: {
        surface(root,16,78,288,151,PANEL,23);
        left(root,32,89,256,"心率",&watch_cn_20,PINK);
        left(root,31,116,120,data?"74":"--",&lv_font_montserrat_48,WHITE);
        left(root,139,143,80,"bpm",&watch_cn_16,GRAY);
        static const int profile[]={0,1,0,2,4,17,30,14,5,0,1,0,2,3,16,27,13,3,0,1,0,0,1};
        for(int i=0;i<23;++i){wave[i].x=i*11;wave[i].y=31-(data?profile[i]:0);}
        lv_obj_t *line=lv_line_create(root);
        lv_obj_set_pos(line,38,179);lv_line_set_points(line,wave,23);
        lv_obj_set_style_line_color(line,lv_color_hex(data?PINK:GRAY),0);
        lv_obj_set_style_line_width(line,3,0);lv_obj_set_style_line_rounded(line,true,0);
        metric(root,16,242,158,"血氧",data?"98.4":"--","% / 未标定",BLUE);
        metric(root,186,242,118,"SQI",data?"92":"--","DEMO",GREEN);
        break;
    }
    case 2: {
        surface(root,16,78,288,204,PANEL,24);
        ring(root,25,107,146,data?41:0,PINK,0x532733);
        ring(root,38,120,120,data?68:0,GREEN,0x3B5025);
        ring(root,51,133,94,data?32:0,BLUE,0x284451);
        left(root,186,101,110,"步数",&watch_cn_16,PINK);
        left(root,185,123,111,data?"3280":"--",&lv_font_montserrat_32,WHITE);
        left(root,186,177,110,"步频",&watch_cn_16,GREEN);
        left(root,185,200,111,data?"106":"--",&lv_font_montserrat_32,WHITE);
        surface(root,16,295,288,47,PANEL,20);
        left(root,29,307,65,"姿态",&watch_cn_20,BLUE);
        (void)txt(root,96,308,195,data?"5.3° / -2.0° / -2.3°":"--",
                  &watch_cn_16,WHITE,LV_TEXT_ALIGN_RIGHT);
        left(root,21,348,278,"步数算法待开发",&watch_cn_16,GRAY);
        break;
    }
    case 3: {
        surface(root,16,78,288,145,PANEL,24);
        left(root,32,91,256,"气压海拔",&watch_cn_20,BLUE);
        left(root,31,122,174,data?"5.9":"--",&lv_font_montserrat_48,WHITE);
        left(root,161,148,46,"m",&watch_cn_20,GRAY);
        left(root,32,185,256,"未校准",&watch_cn_16,GRAY);
        metric(root,16,237,138,"气压",data?"1012.6":"--","hPa",BLUE);
        metric(root,166,237,138,"接触温度",data?"31.4":"--","°C",ORANGE);
        break;
    }
    case 4:
        watch_map_show(root,data);
        break;
    case 5:
        status(root,82,"心率 / 血氧","已单测",GREEN);
        status(root,137,"IMU / 气压","已单测",GREEN);
        status(root,192,"温度 / 屏幕","已单测",GREEN);
        status(root,247,"GNSS","待验证",ORANGE);
        left(root,21,315,278,"开发进度 · 非实时连接状态",&watch_cn_16,GRAY);
        break;
    case 6:
        surface(root,16,82,288,89,PANEL,24);
        left(root,31,94,158,"自动轮播",&watch_cn_20,WHITE);
        left(root,31,127,170,"TOUCH / BOOT",&watch_cn_16,GRAY);
        surface(root,228,106,59,31,autoplay?0x2EC76C:0x4A4A4E,16);
        surface(root,autoplay?257:231,109,25,25,WHITE,13);
        status(root,187,"页面间隔","8秒",GRAY);
        status(root,242,"显示模式","演示数据",ORANGE);
        break;
    }
    return NULL;
}
