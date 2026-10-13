//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//测试硬件：单片机STC12C5A60S2,晶振30M  单片机工作电压3.3V
//QDtech-LCD液晶驱动 for C51
//xiao冯@ShenZhen QDtech co.,LTD
//公司网站:www.qdtft.com
//淘宝网站：http://qdtech.taobao.com
//wiki技术网站：http://www.lcdwiki.com
//我司提供技术支持，任何技术问题欢迎随时交流学习
//固话(传真) :+86 0755-23594567 
//手机:15989313508（冯工） 
//邮箱:lcdwiki01@gmail.com    support@lcdwiki.com    goodtft@163.com
//技术支持QQ:3002773612  3002778157
//技术交流QQ群:324828016
//创建日期:2018/7/7
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 深圳市全动电子技术有限公司 2018-2028
//All rights reserved
//********************************************************************************
//=========================================电源接线================================================//
//5V接DC 5V电源
//GND接地
//=======================================液晶屏数据线接线==========================================//
//本模块默认数据总线类型为16位并口
//液晶屏模块             C51单片机
// DB0~DB7       接       P00~P07        //数据低8位
// DB8~DB15      接       P20~P27        //数据高8位
//=======================================液晶屏控制线接线==========================================//
//液晶屏模块             C51单片机
// LCD_CS        接        P13           //片选控制信号
// LCD_RST       接        P33           //复位信号
// LCD_RS        接        P12           //数据/命令选择控制信号
// LCD_WR        接        P11           //写控制信号
// LCD_RD        接        P10           //读控制信号
//=========================================触摸屏接线=========================================//
//STC89C52RC单片机ROM太小，无法运行带触摸功能的程序，所以触摸屏不需要接线
//不使用触摸或者模块本身不带触摸，则可不连接
//触摸屏使用的数据总线类型为SPI
//触摸屏模块             C51单片机
//  T_CLK        接        P36          //触摸屏SPI时钟信号
//  T_CS         接        P37          //触摸屏片选控制信号
//  T_DIN        接        P34          //触摸屏SPI写信号
//  T_DO         接        P35          //触摸屏SPI读信号
//  T_IRQ        接        P40          //触摸屏响应检查信号，如单片机无P4组，请自行更改其他可用IO并修改代码定义
//**************************************************************************************************/
	
#include <stdlib.h>
#include "lcd.h"
///* Original complete small fonts remain as generator inputs. */ 
#include "gui.h"
#include "string.h"

/*******************************************************************
 * @name       :void GUI_DrawPoint(u16 x,u16 y,u16 color)
 * @date       :2018-08-09 
 * @function   :draw a point in LCD screen
 * @parameters :x:the x coordinate of the point
                y:the y coordinate of the point
								color:the color value of the point
 * @retvalue   :None
********************************************************************/



extern void app_background_poll(void);


/*******************************************************************
 */
void LCD_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u16 color) {
    uint16_t h,n,width;
    if (sx>ex || sy>ey || sx>=lcddev.width || sy>=lcddev.height) return;
    if (ex>=lcddev.width) ex=lcddev.width-1;
    if (ey>=lcddev.height) ey=lcddev.height-1;
    width=ex-sx+1;h=ey-sy+1;
    LCD_SetWindows(sx,sy,ex,ey);
    while (h) {
        n=h>4?4:h;
        pushBlock(color,width*n);
        h-=n;
        app_background_poll();
    }
}

#if 0
/*******************************************************************
 * @name       :void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2)
 * @date       :2018-08-09 
 * @function   :Draw a line between two points
 * @parameters :x1:the bebinning x coordinate of the line
                y1:the bebinning y coordinate of the line
								x2:the ending x coordinate of the line
								y2:the ending y coordinate of the line
 * @retvalue   :None
********************************************************************/
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2)
{
	u16 t; 
	int xerr=0,yerr=0,delta_x,delta_y,distance; 
	int incx,incy,uRow,uCol; 

	delta_x=x2-x1; //计算坐标增量 
	delta_y=y2-y1; 
	uRow=x1; 
	uCol=y1; 
	if(delta_x>0)incx=1; //设置单步方向 
	else if(delta_x==0)incx=0;//垂直线 
	else {incx=-1;delta_x=-delta_x;} 
	if(delta_y>0)incy=1; 
	else if(delta_y==0)incy=0;//水平线 
	else{incy=-1;delta_y=-delta_y;} 
	if( delta_x>delta_y)distance=delta_x; //选取基本增量坐标轴 
	else distance=delta_y; 
	for(t=0;t<=distance+1;t++ )//画线输出 
	{  
		LCD_DrawPoint(uRow,uCol);//画点 
		xerr+=delta_x ; 
		yerr+=delta_y ; 
		if(xerr>distance) 
		{ 
			xerr-=distance; 
			uRow+=incx; 
		} 
		if(yerr>distance) 
		{ 
			yerr-=distance; 
			uCol+=incy; 
		} 
	}  
} 

/*****************************************************************************
 * @name       :void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2)
 * @date       :2018-08-09 
 * @function   :Draw a rectangle
 * @parameters :x1:the bebinning x coordinate of the rectangle
                y1:the bebinning y coordinate of the rectangle
								x2:the ending x coordinate of the rectangle
								y2:the ending y coordinate of the rectangle
 * @retvalue   :None
******************************************************************************/
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2)
{
	LCD_DrawLine(x1,y1,x2,y1);
	LCD_DrawLine(x1,y1,x1,y2);
	LCD_DrawLine(x1,y2,x2,y2);
	LCD_DrawLine(x2,y1,x2,y2);
}  

#endif
/*****************************************************************************
 * @name       :void LCD_DrawFillRectangle(u16 x1, u16 y1, u16 x2, u16 y2)
 * @date       :2018-08-09 
 * @function   :Filled a rectangle
 * @parameters :x1:the bebinning x coordinate of the filled rectangle
                y1:the bebinning y coordinate of the filled rectangle
								x2:the ending x coordinate of the filled rectangle
								y2:the ending y coordinate of the filled rectangle
 * @retvalue   :None
******************************************************************************/  
void LCD_DrawFillRectangle(u16 x1, u16 y1, u16 x2, u16 y2)
{
	LCD_Fill(x1,y1,x2,y2,POINT_COLOR);

}

#if 0 
/*****************************************************************************
 * @name       :void _draw_circle_8(int xc, int yc, int x, int y, u16 c)
 * @date       :2018-08-09 
 * @function   :8 symmetry circle drawing algorithm (internal call)
 * @parameters :xc:the x coordinate of the Circular center 
                yc:the y coordinate of the Circular center 
								x:the x coordinate relative to the Circular center 
								y:the y coordinate relative to the Circular center 
								c:the color value of the circle
 * @retvalue   :None
******************************************************************************/  
void _draw_circle_8(int xc, int yc, int x, int y, u16 c)
{
	GUI_DrawPoint(xc + x, yc + y, c);

	GUI_DrawPoint(xc - x, yc + y, c);

	GUI_DrawPoint(xc + x, yc - y, c);

	GUI_DrawPoint(xc - x, yc - y, c);

	GUI_DrawPoint(xc + y, yc + x, c);

	GUI_DrawPoint(xc - y, yc + x, c);

	GUI_DrawPoint(xc + y, yc - x, c);

	GUI_DrawPoint(xc - y, yc - x, c);
}

/*****************************************************************************
 * @name       :void gui_circle(int xc, int yc,u16 c,int r, int fill)
 * @date       :2018-08-09 
 * @function   :Draw a circle of specified size at a specified location
 * @parameters :xc:the x coordinate of the Circular center 
                yc:the y coordinate of the Circular center 
								r:Circular radius
								fill:1-filling,0-no filling
 * @retvalue   :None
******************************************************************************/  
void gui_circle(int xc, int yc,u16 c,int r, int fill)
{
	int x = 0, y = r, yi, d;

	d = 3 - 2 * r;


	if (fill) 
	{
		// 如果填充（画实心圆）
		while (x <= y) {
			for (yi = x; yi <= y; yi++)
				_draw_circle_8(xc, yc, x, yi, c);

			if (d < 0) {
				d = d + 4 * x + 6;
			} else {
				d = d + 4 * (x - y) + 10;
				y--;
			}
			x++;
		}
	} else 
	{
		// 如果不填充（画空心圆）
		while (x <= y) {
			_draw_circle_8(xc, yc, x, y, c);
			if (d < 0) {
				d = d + 4 * x + 6;
			} else {
				d = d + 4 * (x - y) + 10;
				y--;
			}
			x++;
		}
	}
}
#endif

#if 1
/* Original complete small fonts remain as generator inputs. */
/*****************************************************************************
 */
#include "small_font.h"
uint16_t small_fg,small_bg;
void small_stream(uint8_t *data) __z88dk_fastcall __naked {
 #asm
 lxi b,16
 ss_row:
 mov e,m
 inx h
 mvi d,8
 ss_pixel:
 mov a,e
 rrc
 mov e,a
 push h
 jnc ss_bg
 lhld _small_fg
 jmp ss_color
 ss_bg:
 lhld _small_bg
 ss_color:
 mov a,h
 out LCD_DATA_PORT
 mov a,l
 out LCD_DATA_PORT
 pop h
 dcr d
 jnz ss_pixel
 dcx b
 mov a,b
 ora c
 jnz ss_row
 ret
 #endasm
}
void LCD_ShowChar(u16 x,u16 y,u16 fc,u16 bc,u8 num,u8 mode) {
    uint8_t *data,pos,bits,t,first;
    if (x+8>lcddev.width || y+16>lcddev.height) return;
    if (num<32 || num>126) return;
    data=small_font[small_index[num-32]];
    if (!mode) {
        small_fg=fc;small_bg=bc;
        LCD_SetWindows(x,y,x+7,y+15);
        small_stream(data);
    } else {
        for (pos=0;pos<16;pos++) {
            bits=data[pos];t=0;
            while (t<8) {
                if (!(bits&1)) {bits>>=1;t++;continue;}
                first=t;
                do {bits>>=1;t++;} while (t<8 && (bits&1));
                LCD_SetWindows(x+first,y+pos,x+t-1,y+pos);
                pushBlock(fc,t-first);
            }
        }
    }
}
void LCD_ShowString(u16 x,u16 y,char *p,u8 mode) {
    while (*p && x+8<=lcddev.width && y+16<=lcddev.height) {
        LCD_ShowChar(x,y,POINT_COLOR,BACK_COLOR,(uint8_t)*p++,mode);x+=8;
        app_background_poll();
    }
}
#endif

#if 0
/*****************************************************************************
 * @name       :u32 mypow(u8 m,u8 n)
 * @date       :2018-08-09 
 * @function   :get the nth power of m (internal call)
 * @parameters :m:the multiplier
                n:the power
 * @retvalue   :the nth power of m
******************************************************************************/ 
u32 mypow(u8 m,u8 n)
{
	u32 result=1;	 
	while(n--)result*=m;    
	return result;
}

/*****************************************************************************
 * @name       :void LCD_ShowNum(u16 x,u16 y,u32 num,u8 len,u8 size)
 * @date       :2018-08-09 
 * @function   :Display number
 * @parameters :x:the bebinning x coordinate of the number
                y:the bebinning y coordinate of the number
								num:the number(0~4294967295)
								len:the length of the display number
								size:the size of display number
 * @retvalue   :None
******************************************************************************/  			 
void LCD_ShowNum(u16 x,u16 y,u32 num,u8 len,u8 size)
{         	
	u8 t,temp;
	u8 enshow=0;						   
	for(t=0;t<len;t++)
	{
		temp=(num/mypow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				LCD_ShowChar(x+(size/2)*t,y,POINT_COLOR,BACK_COLOR,' ',size,0);
				continue;
			}else enshow=1; 
		 	 
		}
	 	LCD_ShowChar(x+(size/2)*t,y,POINT_COLOR,BACK_COLOR,temp+'0',size,0); 
	}
} 

 
/*****************************************************************************
 * @name       :void Gui_Drawbmp16(u16 x,u16 y,const unsigned char *p)
 * @date       :2018-08-09 
 * @function   :Display a 16-bit BMP image
 * @parameters :x:the bebinning x coordinate of the BMP image
                y:the bebinning y coordinate of the BMP image
								p:the start address of image array
 * @retvalue   :None
******************************************************************************/ 
void Gui_Drawbmp16(u16 x,u16 y,const unsigned char *p) //显示40*40 QQ图片
{
  	int i; 
	unsigned char picH,picL; 
	LCD_SetWindows(x,y,x+40-1,y+40-1);//窗口设置
    for(i=0;i<40*40;i++)
	{	
	 	picL=*(p+i*2);	//数据低位在前
		picH=*(p+i*2+1);				
		LCD_WR_DATA_16Bit(picH<<8|picL);  						
	}	
	LCD_SetWindows(0,0,lcddev.width-1,lcddev.height-1);//恢复显示窗口为全屏	

}

#endif
//     ****************   TFT_eSPI here!

/* sccz80 fastcall: count in HL, color at entry SP+2; no compiler frame. */
/* Sixteen pixels per iteration, about 30.94 cycles/pixel.
   Hierarchical DE counter avoids a 16-bit zero test for every batch.
   Handle the remainder independently, including count=0 and 65535. */
void pushBlock(uint16_t color, uint16_t count) __z88dk_fastcall __naked {
 #asm
 mov a,h
 ora l
 rz
 push h
 lxi h,4
 dad sp
 mov c,m
 inx h
 mov b,m
 pop h
 mov d,h
 mov e,l
 mov a,l
 ani 15
 mov l,a
 mvi h,0
 push h
 mov a,d
 ora a
 rar
 mov d,a
 mov a,e
 rar
 mov e,a
 mov a,d
 ora a
 rar
 mov d,a
 mov a,e
 rar
 mov e,a
 mov a,d
 ora a
 rar
 mov d,a
 mov a,e
 rar
 mov e,a
 mov a,d
 ora a
 rar
 mov d,a
 mov a,e
 rar
 mov e,a
 mov a,d
 ora e
 jz pb_tail
 dcx d
 inr d
 inr e
 pb_sixteen:
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 dcr e
 jnz pb_sixteen
 dcr d
 jnz pb_sixteen
 pb_tail:
 pop h
 mov a,h
 ora l
 rz
 pb_one:
 mov a,b
 out LCD_DATA_PORT
 mov a,c
 out LCD_DATA_PORT
 dcx h
 mov a,h
 ora l
 jnz pb_one
 ret
 #endasm
}


/***************************************************************************************
** Function name:           drawFastVLine
** Description:             draw a vertical line
***************************************************************************************/
void drawFastVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color)
{

/*
  // Clipping
  if ((x < _vpX) || (x >= _vpW) || (y >= _vpH)) return;

  if (y < _vpY) { h += y - _vpY; y = _vpY; }

  if ((y + h) > _vpH) h = _vpH - y;
*/

  if (h < 1) return;

  LCD_SetWindows(x, y, x, y + h - 1);

  pushBlock(color, h);

}


/***************************************************************************************
** Function name:           drawFastHLine
** Description:             draw a horizontal line
***************************************************************************************/
void drawFastHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{

/*
  // Clipping
  if ((y < _vpY) || (x >= _vpW) || (y >= _vpH)) return;

  if (x < _vpX) { w += x - _vpX; x = _vpX; }

  if ((x + w) > _vpW) w = _vpW - x;
*/

  if (w < 1) return;

  LCD_SetWindows(x, y, x + w - 1, y);

  pushBlock(color, w);

}

/***************************************************************************************
** Function name:           fillCircleHelper
** Description:             Support function for fillRoundRect()
***************************************************************************************/
// Support drawing roundrects, changed to horizontal lines (faster in sprites)
void fillCircleHelper(uint16_t x0, uint16_t y0, uint16_t r, uint8_t cornername, uint16_t delta, uint16_t color)
{
  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -r - r;
  int16_t y     = 0;

  delta++;

  while (y < r) {
    if (f >= 0) {
      if (cornername & 0x1) drawFastHLine(x0 - y, y0 + r, y + y + delta, color);
      if (cornername & 0x2) drawFastHLine(x0 - y, y0 - r, y + y + delta, color);
      r--;
      ddF_y += 2;
      f     += ddF_y;
    }

    y++;
    ddF_x += 2;
    f     += ddF_x;

    if (cornername & 0x1) drawFastHLine(x0 - r, y0 + y, r + r + delta, color);
    if (cornername & 0x2) drawFastHLine(x0 - r, y0 - y, r + r + delta, color);
  }
}

/***************************************************************************************
** Function name:           fillRoundRect
** Description:             Draw a rounded corner filled rectangle
***************************************************************************************/
// Fill a rounded rectangle, changed to horizontal lines (faster in sprites)
void fillRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t color)
{

  // smarter version
  LCD_Fill(x, y + r, x+w, y+h - r, color);

  // draw four corners
  fillCircleHelper(x + r, y + h - r - 1, r, 1, w - r - r - 1, color);
  fillCircleHelper(x + r    , y + r, r, 2, w - r - r - 1, color);

}

/* Clipped scene coordinates only. Bresenham with contiguous run writes. */
void drawLine(int16_t x0,int16_t y0,int16_t x1,int16_t y1,uint16_t color) {
 int16_t temp,dx,dy,err,step,xs,len;
 uint8_t steep=abs(y1-y0)>abs(x1-x0);
 if(steep) {temp=x0;x0=y0;y0=temp;temp=x1;x1=y1;y1=temp;}
 if(x0>x1) {temp=x0;x0=x1;x1=temp;temp=y0;y0=y1;y1=temp;}
 dx=x1-x0;dy=abs(y1-y0);err=dx/2;step=y0<y1?1:-1;xs=x0;len=0;
 for(;x0<=x1;x0++) {
  ++len;err-=dy;
  if(err<0) {
   if(steep)drawFastVLine(y0,xs,len,color);else drawFastHLine(xs,y0,len,color);
   len=0;y0+=step;xs=x0+1;err+=dx;
  }
 }
 if(len) {if(steep)drawFastVLine(y0,xs,len,color);else drawFastHLine(xs,y0,len,color);}
}
