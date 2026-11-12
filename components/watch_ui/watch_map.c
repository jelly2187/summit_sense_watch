#include "watch_map.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAP_X = 16, MAP_Y = 76, MAP_W = 288, MAP_H = 253 };
enum { CONTOUR, TRAIL, WATER, ROUTE };
typedef struct { int32_t x, y; } point_t;
typedef struct { const point_t *points; uint16_t count; uint8_t kind; } path_t;

#if __has_include("map_data/imported_route.h")
#include "map_data/imported_route.h"
#else
#define MAP_GEOREF 0
static const point_t ridge1[] = {{0,710},{110,780},{230,795},{346,752},{473,760},{584,871},{751,902},{928,832}};
static const point_t ridge2[] = {{0,638},{130,696},{234,709},{364,670},{476,676},{619,790},{752,820},{928,752}};
static const point_t ridge3[] = {{3,567},{122,614},{258,630},{392,579},{497,590},{642,711},{790,731},{928,662}};
static const point_t ridge4[] = {{24,490},{127,531},{267,553},{416,503},{539,516},{689,627},{807,643},{920,587}};
static const point_t ridge5[] = {{35,401},{149,461},{292,470},{412,427},{549,443},{722,550},{838,551},{921,510}};
static const point_t ridge6[] = {{28,316},{171,380},{304,390},{448,355},{575,374},{728,464},{854,463},{921,436}};
static const point_t ridge7[] = {{23,239},{147,293},{322,320},{439,282},{607,306},{758,387},{857,372},{921,360}};
static const point_t ridge8[] = {{9,149},{152,211},{309,235},{438,202},{620,223},{751,303},{851,298},{926,272}};
static const point_t river[] = {{0,128},{104,107},{229,151},{322,143},{408,89},{523,76},{635,123},{766,133},{925,70}};
static const point_t track1[] = {{0,328},{165,341},{269,427},{398,457},{476,490},{646,453},{756,488},{925,438}};
static const point_t track2[] = {{429,0},{458,166},{398,272},{476,490},{565,559},{576,691},{681,788},{727,923}};
static const point_t route[] = {{116,256},{165,341},{269,427},{398,457},{476,490},{565,559},{576,691},{681,788},{766,831}};
#define PATH(p,k) {p, sizeof(p)/sizeof(*(p)), k}
static const path_t paths[] = {
    PATH(ridge1,CONTOUR),PATH(ridge2,CONTOUR),PATH(ridge3,CONTOUR),PATH(ridge4,CONTOUR),
    PATH(ridge5,CONTOUR),PATH(ridge6,CONTOUR),PATH(ridge7,CONTOUR),PATH(ridge8,CONTOUR),
    PATH(river,WATER),PATH(track1,TRAIL),PATH(track2,TRAIL),PATH(route,ROUTE),
};
#endif

static lv_obj_t *view, *fix_text, *info_text;
static bool animate, valid_fix, have_marker, outside;
static int32_t fix_lat, fix_lon;
static unsigned zoom;
static int pan_x, pan_y;
static float center_x, center_y, base_scale, marker_x, marker_y;

static void pixel(float x, float y, float *px, float *py)
{
    float s = base_scale * (1U << zoom);
    *px = MAP_W/2.0f + pan_x + (x-center_x)*s;
    *py = MAP_H/2.0f + pan_y - (y-center_y)*s;
}

static void line(lv_layer_t *layer, const lv_area_t *bounds, float x, float y, float X, float Y,
                 unsigned color, int width)
{
    // Clip in map-local coordinates, before handing the endpoints to LVGL.
    float dx = X-x, dy = Y-y, a = 0, b = 1;
    const float p[] = {-dx,dx,-dy,dy};
    const float q[] = {x,MAP_W-1-x,y,MAP_H-1-y};
    for (int i=0; i<4; ++i) {
        if (p[i] == 0) {if (q[i] < 0) return;}
        else {
            float t = q[i]/p[i];
            if (p[i] < 0) {if (t > b) return; if (t > a) a=t;}
            else {if (t < a) return; if (t < b) b=t;}
        }
    }
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = lv_color_hex(color);
    d.width = width;
    d.round_start = d.round_end = 1;
    d.p1.x = bounds->x1 + lroundf(x+a*dx);
    d.p1.y = bounds->y1 + lroundf(y+a*dy);
    d.p2.x = bounds->x1 + lroundf(x+b*dx);
    d.p2.y = bounds->y1 + lroundf(y+b*dy);
    lv_draw_line(layer,&d);
}

static void dot(lv_layer_t *layer, const lv_area_t *area, float x, float y, int radius, unsigned color)
{
    float px,py;
    pixel(x,y,&px,&py);
    if (px < radius || py < radius || px > MAP_W-1-radius || py > MAP_H-1-radius) return;
    int X=area->x1+lroundf(px), Y=area->y1+lroundf(py);
    lv_area_t a={X-radius,Y-radius,X+radius,Y+radius};
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa=LV_OPA_COVER; d.bg_color=lv_color_hex(color); d.radius=LV_RADIUS_CIRCLE;
    lv_draw_rect(layer,&d,&a);
}

static void paint(lv_event_t *ev)
{
    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target(ev),&area);
    lv_layer_t *layer=lv_event_get_layer(ev);
    for (int i=32;i<MAP_W;i+=32) line(layer,&area,i,0,i,MAP_H-1,0x203840,1);
    for (int i=32;i<MAP_H;i+=32) line(layer,&area,0,i,MAP_W-1,i,0x203840,1);
    const unsigned colors[]={0x335D49,0x819694,0x2582AE,0x49E3BE};
    const int widths[]={2,2,4,4};
    for (unsigned k=0;k<sizeof(paths)/sizeof(*paths);++k) {
        const path_t *p=&paths[k];
        for (unsigned j=1;j<p->count;++j) {
            float x,y,X,Y;
            pixel(p->points[j-1].x,p->points[j-1].y,&x,&y);
            pixel(p->points[j].x,p->points[j].y,&X,&Y);
            if(p->kind==ROUTE) line(layer,&area,x,y,X,Y,0x102320,7);
            line(layer,&area,x,y,X,Y,colors[p->kind],widths[p->kind]);
        }
        if (p->kind==ROUTE) {
            dot(layer,&area,p->points[0].x,p->points[0].y,4,0xEAF4E8);
            dot(layer,&area,p->points[p->count-1].x,p->points[p->count-1].y,5,0xFFB966);
        }
    }
    if (have_marker) {
        dot(layer,&area,marker_x,marker_y,10,0x123B62);
        dot(layer,&area,marker_x,marker_y,6,0xDBF4FF);
        dot(layer,&area,marker_x,marker_y,4,0x408DFF);
    }
}

static lv_obj_t *label(lv_obj_t *root,int x,int y,int width,const char *s,unsigned color)
{
    lv_obj_t *o=lv_label_create(root);
    lv_obj_set_pos(o,x,y);lv_obj_set_width(o,width);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(o,&lv_font_montserrat_14,0);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_label_set_text(o,s);
    return o;
}

static void button(lv_obj_t *root,int y,const char *text)
{
    lv_obj_t *b=lv_obj_create(root);
    lv_obj_remove_style_all(b);
    lv_obj_remove_flag(b,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(b,MAP_X+MAP_W-43,y);lv_obj_set_size(b,35,34);
    lv_obj_set_style_radius(b,7,0);
    lv_obj_set_style_bg_opa(b,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(b,lv_color_hex(0x233F46),0);
    lv_obj_t *l=label(b,0,8,35,text,0xE4F3F2);
    lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
}

static void update_info(void)
{
    if (!view) return;
    char status[64];
    if (valid_fix) {
        snprintf(status,sizeof(status),"FIX  %.5f, %.5f",fix_lat / 1e7,fix_lon / 1e7);
        lv_label_set_text(fix_text,status);
    } else lv_label_set_text(fix_text,"GNSS: NO FIX");
#if MAP_GEOREF
    lv_label_set_text(info_text,valid_fix && outside ? "GPX TRACK  |  OFF MAP" : "GPX TRACK  |  OFFLINE");
#else
    lv_label_set_text(info_text,"DEMO ONLY  |  NO NAVIGATION");
#endif
}

static void marker_from_fix(void)
{
#if MAP_GEOREF
    have_marker = valid_fix;
    if (valid_fix) {
        marker_x=(float)(((double)fix_lon - MAP_ORIGIN_LON_E7) * MAP_LON_M_PER_DEG / 1e7);
        marker_y=(float)(((double)fix_lat - MAP_ORIGIN_LAT_E7) * 111320.0 / 1e7);
    }
#endif
    float x,y;
    pixel(marker_x,marker_y,&x,&y);
    outside=x<0 || x>=MAP_W || y<0 || y>=MAP_H;
}

void watch_map_close(void) { view=fix_text=info_text=NULL; }

void watch_map_show(lv_obj_t *root,bool animate_demo)
{
    animate=animate_demo;
    int32_t minx=paths[0].points[0].x,maxx=minx,miny=paths[0].points[0].y,maxy=miny;
    for (unsigned i=0;i<sizeof(paths)/sizeof(*paths);++i) for (unsigned j=0;j<paths[i].count;++j) {
        point_t p=paths[i].points[j];
        if(p.x<minx)minx=p.x;
        if(p.x>maxx)maxx=p.x;
        if(p.y<miny)miny=p.y;
        if(p.y>maxy)maxy=p.y;
    }
    center_x=((float)minx+maxx)/2;center_y=((float)miny+maxy)/2;
    base_scale=fminf((MAP_W-32)/fmaxf(40,maxx-minx),(MAP_H-28)/fmaxf(40,maxy-miny));
    zoom=0;pan_x=pan_y=0;
    marker_x=route[0].x;marker_y=route[0].y;
    have_marker=!MAP_GEOREF && animate;
    view=lv_obj_create(root);
    lv_obj_remove_style_all(view);lv_obj_remove_flag(view,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(view,MAP_X,MAP_Y);lv_obj_set_size(view,MAP_W,MAP_H);
    lv_obj_set_style_bg_opa(view,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(view,lv_color_hex(0x14292B),0);
    lv_obj_set_style_clip_corner(view,true,0);
    lv_obj_add_event_cb(view,paint,LV_EVENT_DRAW_MAIN,NULL);
    label(root,MAP_X+8,MAP_Y+8,128,MAP_GEOREF ? "OFFLINE TRACK" : "DEMO MAP",0xC6E6DD);
    label(root,MAP_X+12,MAP_Y+28,28,"N ^",0xC6E6DD);
    button(root,MAP_Y+8,"+");button(root,MAP_Y+47,"-");button(root,MAP_Y+86,"FIT");
    fix_text=label(root,MAP_X,338,MAP_W,"",0xDCF2EE);
    info_text=label(root,MAP_X,358,MAP_W,"",0x8CB4AF);
    marker_from_fix();update_info();
}

void watch_map_fix(bool valid,int32_t lat_e7,int32_t lon_e7)
{
    if (valid==valid_fix && (!valid || (lat_e7==fix_lat && lon_e7==fix_lon))) return;
    valid_fix=valid;fix_lat=lat_e7;fix_lon=lon_e7;
    if (view) { marker_from_fix();update_info();lv_obj_invalidate(view); }
}

void watch_map_tick(uint32_t seconds)
{
#if !MAP_GEOREF
    static uint32_t last=UINT32_MAX;
    if(!view || !animate || seconds==last) return;
    last=seconds;
    unsigned i=(seconds/3)%(sizeof(route)/sizeof(*route)-1);
    float f=(seconds%3)/3.0f;
    marker_x=route[i].x+f*(route[i+1].x-route[i].x);
    marker_y=route[i].y+f*(route[i+1].y-route[i].y);
    lv_obj_invalidate(view);
#else
    (void)seconds;
#endif
}

bool watch_map_touch(int x0,int y0,int x1,int y1)
{
    if (!view || x0<MAP_X || x0>=MAP_X+MAP_W || y0<MAP_Y || y0>=MAP_Y+MAP_H) return false;
    int dx=x1-x0,dy=y1-y0;
    if (abs(dx)+abs(dy)>22) {
        pan_x+=dx;pan_y+=dy;
        if(pan_x<-3*MAP_W)pan_x=-3*MAP_W;
        if(pan_x>3*MAP_W)pan_x=3*MAP_W;
        if(pan_y<-3*MAP_H)pan_y=-3*MAP_H;
        if(pan_y>3*MAP_H)pan_y=3*MAP_H;
    } else if (x0>=MAP_X+MAP_W-43 && x0<MAP_X+MAP_W-8) {
        int r=y0-MAP_Y;
        if(r>=8 && r<42 && zoom<2) ++zoom;
        if(r>=47 && r<81 && zoom>0) --zoom;
        if(r>=86 && r<120) {zoom=0;pan_x=pan_y=0;}
    }
    marker_from_fix();update_info();lv_obj_invalidate(view);
    return true;
}
