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
#include "gui.h"
#include "main.h"

//LCD的画笔颜色和背景色	   
u16 POINT_COLOR=0x0000;	//画笔颜色
u16 BACK_COLOR=0xFFFF;  //背景色 
//管理LCD重要参数
//默认为竖屏
_lcd_dev lcddev;

/*void LCD_write(u8 HVAL,u8 LVAL)
{
	LCD_CS = 0;
	LCD_WR = 0;
	LCD_DataPortH = HVAL;
	LCD_DataPortL = LVAL;
	LCD_WR = 1;
	LCD_CS = 1;
}

u16 LCD_read(void)
{
	u16 d;
	LCD_CS = 0;
	LCD_RD = 0;
	delay_us(1); //delay 1 us
	d = LCD_DataPortH;
	d = (d<<8)|LCD_DataPortL;
	LCD_RD = 1;
  LCD_CS = 1;
	return d;
}
*/
/*****************************************************************************
 */
void LCD_WR_REG(u16 value) __z88dk_fastcall __naked {
 #asm
 mov a,l
 out LCD_CMD_PORT
 ret
 #endasm
} 

/*****************************************************************************
 */
void LCD_WR_DATA(u16 value) __z88dk_fastcall __naked {
 #asm
 mov a,l
 out LCD_DATA_PORT
 ret
 #endasm
}

/*****************************************************************************
 * @name       :u16 LCD_RD_DATA(void)
 * @date       :2018-11-13 
 * @function   :Read an 16-bit value from the LCD screen
 * @parameters :None
 * @retvalue   :read value
******************************************************************************/


/*****************************************************************************
 */
void LCD_WR_DATA_16Bit(u16 value) __z88dk_fastcall __naked {
 #asm
 mov a,h
 out LCD_DATA_PORT
 mov a,l
 out LCD_DATA_PORT
 ret
 #endasm
}

/*****************************************************************************
 * @name       :u16 Lcd_ReadData_16Bit(void)
 * @date       :2018-11-13 
 * @function   :Read an 16-bit value from the LCD screen
 * @parameters :None
 * @retvalue   :read value
******************************************************************************/	


/*****************************************************************************
 * @name       :void LCD_WriteReg(u16 LCD_Reg, u16 LCD_RegValue)
 * @date       :2018-08-09 
 * @function   :Write data into registers
 * @parameters :LCD_Reg:Register address
                LCD_RegValue:Data to be written
 * @retvalue   :None
******************************************************************************/
void LCD_WriteReg(u16 LCD_Reg, u16 LCD_RegValue)
{
  LCD_WR_REG(LCD_Reg);
	LCD_WR_DATA(LCD_RegValue);
}

/*****************************************************************************
 * @name       :u16 LCD_ReadReg(u16 LCD_Reg)
 * @date       :2018-11-13 
 * @function   :read value from specially registers
 * @parameters :LCD_Reg:Register address
 * @retvalue   :read value
******************************************************************************/


/*****************************************************************************
 * @name       :void LCD_WriteRAM_Prepare(void)
 * @date       :2018-08-09 
 * @function   :Write GRAM
 * @parameters :None
 * @retvalue   :None
******************************************************************************/	
void LCD_WriteRAM_Prepare(void)
{
 	LCD_WR_REG(lcddev.wramcmd);	  
}

/*****************************************************************************
 * @name       :void LCD_ReadRAM_Prepare(void)
 * @date       :2018-11-13 
 * @function   :Read GRAM
 * @parameters :None
 * @retvalue   :None
******************************************************************************/	 


/*****************************************************************************
 */
void LCD_Clear(u16 color) {
    LCD_Fill(0,0,lcddev.width-1,lcddev.height-1,color);
}


/*****************************************************************************
 * @name       :void LCD_DrawPoint(u16 x,u16 y)
 * @date       :2018-08-09 
 * @function   :Write a pixel data at a specified location
 * @parameters :x:the x coordinate of the pixel
                y:the y coordinate of the pixel
 * @retvalue   :None
******************************************************************************/	
 	 


/*****************************************************************************
 * @name       :u16 LCD_ReadPoint(u16 x,u16 y)
 * @date       :2018-11-13 
 * @function   :Read a pixel color value at a specified location
 * @parameters :x:the x coordinate of the pixel
                y:the y coordinate of the pixel
 * @retvalue   :the read color value
******************************************************************************/	


/*****************************************************************************
 * @name       :void LCD_Set_GPIO(void)
 * @date       :2018-11-13 
 * @function   :Set the gpio to push-pull mode
 * @parameters :None
 * @retvalue   :None
******************************************************************************/	


/*****************************************************************************
 * @name       :void LCD_Reset(void)
 * @date       :2018-08-09 
 * @function   :Reset LCD screen
 * @parameters :None
 * @retvalue   :None
******************************************************************************/	


/*****************************************************************************
 * @name       :void LCD_Init(void)
 * @date       :2018-08-09 
 * @function   :Initialization LCD screen
 * @parameters :None
 * @retvalue   :None
******************************************************************************/	 	 
void LCD_Init(void)
{
#if 0
	/* Board has no software LCD reset line. */ //初始化之前复位
    LCD_WR_REG(0xF1);
    LCD_WR_DATA(0x36);
    LCD_WR_DATA(0x04);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x3C);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x8F);


    LCD_WR_REG(0xF2);
    LCD_WR_DATA(0x18);
    LCD_WR_DATA(0xA3);
    LCD_WR_DATA(0x12);
    LCD_WR_DATA(0x02);
    LCD_WR_DATA(0xb2);
    LCD_WR_DATA(0x12);
    LCD_WR_DATA(0xFF);
    LCD_WR_DATA(0x10);
    LCD_WR_DATA(0x00);

    LCD_WR_REG(0xF8);
    LCD_WR_DATA(0x21);
    LCD_WR_DATA(0x04);

    LCD_WR_REG(0xF9);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x08);  

    LCD_WR_REG(0xC0);
    LCD_WR_DATA(0x0f); //13
    LCD_WR_DATA(0x0f); //10

    LCD_WR_REG(0xC1);
    LCD_WR_DATA(0x42); //43

    LCD_WR_REG(0xC2);
    LCD_WR_DATA(0x22);

    LCD_WR_REG(0xC5);
    LCD_WR_DATA(0x01); //00
    LCD_WR_DATA(0x29); //4D
    LCD_WR_DATA(0x80);

    LCD_WR_REG(0xB6);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x02); //42
    LCD_WR_DATA(0x3b);

    LCD_WR_REG(0xB1);
    LCD_WR_DATA(0xB0); //C0
    LCD_WR_DATA(0x11);

    LCD_WR_REG(0xB4);
    LCD_WR_DATA(0x02); //01

    LCD_WR_REG(0xE0);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x18);
    LCD_WR_DATA(0x15);
    LCD_WR_DATA(0x09);
    LCD_WR_DATA(0x0B);
    LCD_WR_DATA(0x04);
    LCD_WR_DATA(0x49);
    LCD_WR_DATA(0x64);
    LCD_WR_DATA(0x3D);
    LCD_WR_DATA(0x08);
    LCD_WR_DATA(0x15);
    LCD_WR_DATA(0x06);
    LCD_WR_DATA(0x12);
    LCD_WR_DATA(0x07);
    LCD_WR_DATA(0x00);

    LCD_WR_REG(0xE1);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x38);
    LCD_WR_DATA(0x35);
    LCD_WR_DATA(0x0a);
    LCD_WR_DATA(0x0c);
    LCD_WR_DATA(0x03);
    LCD_WR_DATA(0x4A);
    LCD_WR_DATA(0x42);
    LCD_WR_DATA(0x36);
    LCD_WR_DATA(0x04);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x03);
    LCD_WR_DATA(0x1F);
    LCD_WR_DATA(0x1B);
    LCD_WR_DATA(0x00);

    LCD_WR_REG(0x20);                     // display inversion OFF
  
    LCD_WR_REG(0x36);      // MEMORY_ACCESS_CONTROL (orientation stuff)
    LCD_WR_DATA(0x48);
     
    LCD_WR_REG(0x3A);      // COLMOD_PIXEL_FORMAT_SET
    LCD_WR_DATA(0x55);     // 16 bit pixel 

    LCD_WR_REG(0x13); // Nomal Displaymode
    
    LCD_WR_REG(0x11);                     // sleep out
    msleep(150);
     
    LCD_WR_REG(0x29);                     // display on
    msleep(150);

#endif
#if 0
	// WORKED
	/* Board has no software LCD reset line. */ //初始化之前复位
    LCD_WR_REG(0xF9);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x08);
    LCD_WR_REG(0xC0);
    LCD_WR_DATA(0x19);
    LCD_WR_DATA(0x1A);
    LCD_WR_REG(0xC1);
    LCD_WR_DATA(0x45);
    LCD_WR_DATA(0X00);
    LCD_WR_REG(0xC2);
    LCD_WR_DATA(0x33);
    LCD_WR_REG(0xC5);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x28);
    LCD_WR_REG(0xB1);
    LCD_WR_DATA(0x90);
    LCD_WR_DATA(0x11);
    LCD_WR_REG(0xB4);
    LCD_WR_DATA(0x02);
    LCD_WR_REG(0xB6);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x42);
    LCD_WR_DATA(0x3B);
    LCD_WR_REG(0xB7);
    LCD_WR_DATA(0x07);
    LCD_WR_REG(0xE0);
    LCD_WR_DATA(0x1F);
    LCD_WR_DATA(0x25);
    LCD_WR_DATA(0x22);
    LCD_WR_DATA(0x0B);
    LCD_WR_DATA(0x06);
    LCD_WR_DATA(0x0A);
    LCD_WR_DATA(0x4E);
    LCD_WR_DATA(0xC6);
    LCD_WR_DATA(0x39);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);
    LCD_WR_REG(0xE1);
    LCD_WR_DATA(0x1F);
    LCD_WR_DATA(0x3F);
    LCD_WR_DATA(0x3F);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x1F);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x46);
    LCD_WR_DATA(0x49);
    LCD_WR_DATA(0x31);
    LCD_WR_DATA(0x05);
    LCD_WR_DATA(0x09);
    LCD_WR_DATA(0x03);
    LCD_WR_DATA(0x1C);
    LCD_WR_DATA(0x1A);
    LCD_WR_DATA(0x00);
    LCD_WR_REG(0xF1);
    LCD_WR_DATA(0x36);
    LCD_WR_DATA(0x04);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x3C);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0xA4);
    LCD_WR_DATA(0x02);
    LCD_WR_REG(0xF2);
    LCD_WR_DATA(0x18);
    LCD_WR_DATA(0xA3);
    LCD_WR_DATA(0x12);
    LCD_WR_DATA(0x02);
    LCD_WR_DATA(0x32);
    LCD_WR_DATA(0x12);
    LCD_WR_DATA(0xFF);
    LCD_WR_DATA(0x32);
    LCD_WR_DATA(0x00);
    LCD_WR_REG(0xF4);
    LCD_WR_DATA(0x40);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x08);
    LCD_WR_DATA(0x91);
    LCD_WR_DATA(0x04);
    LCD_WR_REG(0xF8);
    LCD_WR_DATA(0x21);
    LCD_WR_DATA(0x04);
    LCD_WR_REG(0x36);
    LCD_WR_DATA(0x48);
    LCD_WR_REG(0x3A);
    LCD_WR_DATA(0x55);    
    LCD_WR_REG(0x11);    //Exit Sleep
	msleep(150);
    LCD_WR_REG(0x29);    //Display on#endif
#endif
#if 0
	/* Board has no software LCD reset line. */ //初始化之前复位

    LCD_WR_REG(0x01); // SW reset
    msleep(120);

    LCD_WR_REG(0x11); // Sleep out, also SW reset
    msleep(120);

    LCD_WR_REG(0x3A);
      LCD_WR_DATA(0x55);           // 16 bit colour interface

    LCD_WR_REG(0xC0); //                          1100.0000 Power Control 1
    LCD_WR_DATA(0x0E);    //                          0001.0111   ... VRH1
    LCD_WR_DATA(0x0E);    //                          0001.0101   ... VRH2
    LCD_WR_REG(0xC1); //                          1100.0001 Power Control 2
    LCD_WR_DATA(0x41);    //                          0100.0001   . SAP BT
    LCD_WR_DATA(0x00);    //                          0000.0000   ..... VC
    LCD_WR_REG(0xC2); //                          1100.0010 Power Control 3
    LCD_WR_DATA(0x55);    //     nb. was 0x44         0101.0101   . DCA1 . DCA0

    LCD_WR_REG(0xC5);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);

    LCD_WR_REG(0xE0);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x1F);
    LCD_WR_DATA(0x1C);
    LCD_WR_DATA(0x0C);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x08);
    LCD_WR_DATA(0x48);
    LCD_WR_DATA(0x98);
    LCD_WR_DATA(0x37);
    LCD_WR_DATA(0x0A);
    LCD_WR_DATA(0x13);
    LCD_WR_DATA(0x04);
    LCD_WR_DATA(0x11);
    LCD_WR_DATA(0x0D);
    LCD_WR_DATA(0x00);
 
    LCD_WR_REG(0xE1);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x32);
    LCD_WR_DATA(0x2E);
    LCD_WR_DATA(0x0B);
    LCD_WR_DATA(0x0D);
    LCD_WR_DATA(0x05);
    LCD_WR_DATA(0x47);
    LCD_WR_DATA(0x75);
    LCD_WR_DATA(0x37);
    LCD_WR_DATA(0x06);
    LCD_WR_DATA(0x10);
    LCD_WR_DATA(0x03);
    LCD_WR_DATA(0x24);
    LCD_WR_DATA(0x20);
    LCD_WR_DATA(0x00);
 
      LCD_WR_REG(0x20);
 
    LCD_WR_REG(0x36);
    LCD_WR_DATA(0x48);

    LCD_WR_REG(0x29);                     // display on
	msleep(150);
#endif
#if 1

	/* Board has no software LCD reset line. */ //初始化之前复位
//	LCD_WR_REG(0X00);
//	LCD_WR_REG(0X00);
//	LCD_WR_REG(0X00);

	LCD_WR_REG(0XF1);
	LCD_WR_DATA(0x36);
	LCD_WR_DATA(0x04);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x3C);
	LCD_WR_DATA(0X0F);
	LCD_WR_DATA(0x8F);
	LCD_WR_REG(0XF2);
	LCD_WR_DATA(0x18);
	LCD_WR_DATA(0xA3);
	LCD_WR_DATA(0x12);
	LCD_WR_DATA(0x02);
	LCD_WR_DATA(0XB2);
	LCD_WR_DATA(0x12);
	LCD_WR_DATA(0xFF);
	LCD_WR_DATA(0x10);
	LCD_WR_DATA(0x00);
	LCD_WR_REG(0XF8);
	LCD_WR_DATA(0x21);
	LCD_WR_DATA(0x04);
	LCD_WR_REG(0XF9);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x08);
	LCD_WR_REG(0x36);
	LCD_WR_DATA(0x08);
	LCD_WR_REG(0xB4);
	LCD_WR_DATA(0x00);
	LCD_WR_REG(0xC1);
	LCD_WR_DATA(0x41);
	LCD_WR_REG(0xC5);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x91);
	LCD_WR_DATA(0x80);
	LCD_WR_DATA(0x00);
	LCD_WR_REG(0xE0);
	LCD_WR_DATA(0x0F);
	LCD_WR_DATA(0x1F);
	LCD_WR_DATA(0x1C);
	LCD_WR_DATA(0x0C);
	LCD_WR_DATA(0x0F);
	LCD_WR_DATA(0x08);
	LCD_WR_DATA(0x48);
	LCD_WR_DATA(0x98);
	LCD_WR_DATA(0x37);
	LCD_WR_DATA(0x0A);
	LCD_WR_DATA(0x13);
	LCD_WR_DATA(0x04);
	LCD_WR_DATA(0x11);
	LCD_WR_DATA(0x0D);
	LCD_WR_DATA(0x00);
	LCD_WR_REG(0xE1);
	LCD_WR_DATA(0x0F);
	LCD_WR_DATA(0x32);
	LCD_WR_DATA(0x2E);
	LCD_WR_DATA(0x0B);
	LCD_WR_DATA(0x0D);
	LCD_WR_DATA(0x05);
	LCD_WR_DATA(0x47);
	LCD_WR_DATA(0x75);
	LCD_WR_DATA(0x37);
	LCD_WR_DATA(0x06);
	LCD_WR_DATA(0x10);
	LCD_WR_DATA(0x03);
	LCD_WR_DATA(0x24);
	LCD_WR_DATA(0x20);
	LCD_WR_DATA(0x00);
	LCD_WR_REG(0x3A);
	LCD_WR_DATA(0x55);
	LCD_WR_REG(0x11);
	LCD_WR_REG(0x36);
	LCD_WR_DATA(0x28);
	msleep(150);
	//LCD_Delay();
	LCD_WR_REG(0x29);
#endif

	//设置LCD属性参数
	LCD_direction(3);//设置LCD显示方向 
//	LCD_BL=1;//点亮背光	 

}
 
/*****************************************************************************
 * @name       :void LCD_SetWindows(u16 xStar, u16 yStar,u16 xEnd,u16 yEnd)
 * @date       :2018-08-09 
 * @function   :Setting LCD display window
 * @parameters :xStar:the bebinning x coordinate of the LCD display window
								yStar:the bebinning y coordinate of the LCD display window
								xEnd:the endning x coordinate of the LCD display window
								yEnd:the endning y coordinate of the LCD display window
 * @retvalue   :None
******************************************************************************/ 
void LCD_SetWindows(u16 xStar, u16 yStar,u16 xEnd,u16 yEnd)
{	
	LCD_WR_REG(lcddev.setxcmd);	
	LCD_WR_DATA(xStar>>8);
	LCD_WR_DATA(0x00FF&xStar);		
	LCD_WR_DATA(xEnd>>8);
	LCD_WR_DATA(0x00FF&xEnd);

	LCD_WR_REG(lcddev.setycmd);	
	LCD_WR_DATA(yStar>>8);
	LCD_WR_DATA(0x00FF&yStar);		
	LCD_WR_DATA(yEnd>>8);
	LCD_WR_DATA(0x00FF&yEnd);	

	LCD_WriteRAM_Prepare();	//开始写入GRAM				
}   

/*****************************************************************************
 * @name       :void LCD_SetCursor(u16 Xpos, u16 Ypos)
 * @date       :2018-08-09 
 * @function   :Set coordinate value
 * @parameters :Xpos:the  x coordinate of the pixel
								Ypos:the  y coordinate of the pixel
 * @retvalue   :None
******************************************************************************/ 
 

/*****************************************************************************
 * @name       :void LCD_direction(u8 direction)
 * @date       :2018-08-09 
 * @function   :Setting the display direction of LCD screen
 * @parameters :direction:0-0 degree
                          1-90 degree
													2-180 degree
													3-270 degree
 * @retvalue   :None
******************************************************************************/ 
void LCD_direction(u8 direction) {
    lcddev.setxcmd=0x2a;lcddev.setycmd=0x2b;lcddev.wramcmd=0x2c;
    lcddev.width=480;lcddev.height=320;
    LCD_WriteReg(0x36,0xe8); /* original board orientation (direction 3) */
}
