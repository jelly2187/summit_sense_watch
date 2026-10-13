#include "watch_pages.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
static uint16_t frame[240*240],buffer[240*40];
static unsigned clipped;
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p)
{
    uint16_t *src=(uint16_t *)p;
    assert(a->x1>=0 && a->y1>=0 && a->x2<240 && a->y2<240);
    for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)frame[y*240+x]=*src++;
    lv_display_flush_ready(d);
}
static void check_labels(lv_obj_t *o)
{
    if(lv_obj_check_type(o,&lv_label_class)) {
        lv_point_t size;
        const lv_font_t *font=lv_obj_get_style_text_font(o,LV_PART_MAIN);
        lv_text_get_size(&size,lv_label_get_text(o),font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        lv_area_t area;lv_obj_get_coords(o,&area);
        if(size.x>lv_obj_get_width(o) || area.x1<0 || area.y1<0 || area.x2>=240 || area.y2>=240) {
            fprintf(stderr,"CLIPPED: %s text_width=%ld width=%ld bounds=%ld,%ld,%ld,%ld\n",
                lv_label_get_text(o),(long)size.x,(long)lv_obj_get_width(o),
                (long)area.x1,(long)area.y1,(long)area.x2,(long)area.y2);clipped++;
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_count(o);i++)check_labels(lv_obj_get_child(o,i));
}
static void save(const char *directory,const char *name)
{
    char path[1024];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");
    for(unsigned j=0;j<240*240;j++) {
        uint16_t c=frame[j];unsigned char rgb[]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};
        fwrite(rgb,1,3,f);
    }
    fclose(f);
}
int main(int argc,char **argv)
{
    if(argc!=2)return 2;
    lv_init();lv_display_t *d=lv_display_create(240,240);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d,flush);
    for(unsigned state=0;state<2;state++)for(unsigned page=0;page<WATCH_PAGE_COUNT;page++) {
        watch_pages_show(page,state==0,state==0);watch_pages_tick(0);
        lv_refr_now(d);check_labels(lv_screen_active());
        char name[48];snprintf(name,sizeof(name),"%02u_%s",page,state==0 ? "demo" : "empty");save(argv[1],name);
    }
    lv_mem_monitor_t before,after;
    watch_pages_show(0,true,true);lv_refr_now(d);lv_mem_monitor(&before);
    for(unsigned i=0;i<210;i++) {watch_pages_show(i%WATCH_PAGE_COUNT,i%2==0,i%3==0);lv_refr_now(d);}
    watch_pages_show(0,true,true);lv_refr_now(d);lv_mem_monitor(&after);
    assert(after.free_size==before.free_size);
    assert(lv_mem_test()==LV_RESULT_OK);
    printf("14 previews, 210 transitions, no retained allocation; clipped=%u; peak LVGL heap=%lu bytes\n",
       clipped,(unsigned long)after.max_used);
    return clipped ? 1 : 0;
}
