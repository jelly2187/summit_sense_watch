#include "lcd_test_pattern.h"

static bool letter_pixel(char letter,int x,int y)
{
    static const uint8_t T[]={31,4,4,4,4,4,4},O[]={14,17,17,17,17,17,14};
    static const uint8_t P[]={30,17,17,30,16,16,16},L[]={16,16,16,16,16,16,31};
    static const uint8_t R[]={30,17,17,30,20,18,17},G[]={14,17,16,23,17,17,14};
    static const uint8_t B[]={30,17,17,30,17,17,30};
    const uint8_t *rows=NULL;
    switch(letter) {case 'T':rows=T;break;case 'O':rows=O;break;case 'P':rows=P;break;
        case 'L':rows=L;break;case 'R':rows=R;break;case 'G':rows=G;break;case 'B':rows=B;break;}
    return rows && x>=0 && x<5 && y>=0 && y<7 && (rows[y] & (1U<<(4-x)));
}
static bool text_pixel(const char *text,int origin_x,int origin_y,int scale,int x,int y)
{
    if(x<origin_x || y<origin_y || y>=origin_y+7*scale)return false;
    int index=(x-origin_x)/(6*scale);
    for(int i=0;text[i];++i) {
        if(i==index)return letter_pixel(text[i],((x-origin_x)/scale)%6,(y-origin_y)/scale);
    }
    return false;
}
uint16_t lcd_test_pattern_pixel(int x,int y)
{
    if(x<0 || y<0 || x>=240 || y>=240)return 0;
    if(x==0 || x==239 || y==0 || y==239)return 0xFFFF;
    uint16_t color=0x1082;
    if(y>=50 && y<170 && x>=3 && x<237) {
        static const uint16_t colors[]={0xF800,0x07E0,0x001F};
        color=colors[(x-3)/78];
    }
    if(y>=185 && y<205 && x>=4 && x<236) {
        unsigned value=(unsigned)(x-4)*255/231;
        color=((value>>3)<<11)|((value>>2)<<5)|(value>>3);
    }
    if(text_pixel("TOP",102,12,2,x,y) || text_pixel("TL",6,32,1,x,y) ||
       text_pixel("TR",222,32,1,x,y) || text_pixel("BL",6,223,1,x,y) ||
       text_pixel("BR",222,223,1,x,y) || text_pixel("R",30,95,3,x,y) ||
       text_pixel("G",109,95,3,x,y) || text_pixel("B",187,95,3,x,y))color=0xFFFF;
    return color;
}
