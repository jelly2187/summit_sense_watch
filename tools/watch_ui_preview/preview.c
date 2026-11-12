#include "watch_pages.h"
#include "watch_map.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
static uint16_t frame[320*480],buffer[320*32];
static int width=240,height=240;
static unsigned clipped;
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p)
{
    uint16_t *src=(uint16_t *)p;
    assert(a->x1>=0 && a->y1>=0 && a->x2<width && a->y2<height);
    for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)frame[y*width+x]=*src++;
    lv_display_flush_ready(d);
}
static void check_labels(lv_obj_t *o)
{
    if(lv_obj_check_type(o,&lv_label_class)) {
        lv_point_t size;
        const lv_font_t *font=lv_obj_get_style_text_font(o,LV_PART_MAIN);
        lv_text_get_size(&size,lv_label_get_text(o),font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        lv_area_t area;lv_obj_get_coords(o,&area);
        if(size.x>lv_obj_get_width(o) || area.x1<0 || area.y1<0 || area.x2>=width || area.y2>=height) {
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
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",width,height);
    for(int j=0;j<width*height;j++) {
        uint16_t c=frame[j];unsigned char rgb[]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};
        fwrite(rgb,1,3,f);
    }
    fclose(f);
}
int main(int argc,char **argv)
{
    if(argc!=2 && (argc!=3 || strcmp(argv[2],"--wide")!=0))return 2;
    if(argc==3) {width=320;height=480;}
    lv_init();lv_display_t *d=lv_display_create(width,height);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d,flush);
    for(unsigned state=0;state<2;state++)for(unsigned page=0;page<WATCH_PAGE_COUNT;page++) {
        watch_pages_show(page,state==0,state==0);watch_pages_tick(0);
        lv_refr_now(d);check_labels(lv_screen_active());
        char name[48];snprintf(name,sizeof(name),"%02u_%s",page,state==0 ? "demo" : "empty");save(argv[1],name);
    }
    if(width==320) {
        watch_pages_show(4,true,false);
        assert(watch_map_touch(280,100,280,100));  // zoom in
        assert(watch_map_touch(150,200,190,224));  // pan
        assert(!watch_map_touch(50,411,50,411));   // footer remains navigation
        lv_refr_now(d);check_labels(lv_screen_active());save(argv[1],"map_zoom_pan");
        assert(watch_map_touch(280,177,280,177));  // fit
        watch_map_fix(true,481173000,115166667);
        lv_refr_now(d);check_labels(lv_screen_active());save(argv[1],"map_fix");
        watch_map_fix(false,0,0);
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
