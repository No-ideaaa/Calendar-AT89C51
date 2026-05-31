#include <reg51.h>

// ================== 类型定义 ==================
typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

// ================== 宏定义与引脚 ==================
// LCD 引脚
#define LCD_DATA P0
sbit LCD_RS = P2^0;
sbit LCD_RW = P2^1;
sbit LCD_EN = P2^2;

// 按键引脚
sbit KEY_MODE = P3^0; // 模式切换/确认
sbit KEY_UP   = P3^1; // 加
sbit KEY_DOWN = P3^2; // 减

// ================== 全局变量 ==================
// 时间日期变量
u16 year = 2024;  // 年份
u8 month = 3;   // 月
u8 day   = 6;   // 日
u8 week  = 0;   // 星期 (0-6, 对应 日-六)
u8 hour  = 0;
u8 min   = 42;
u8 sec   = 0;

// 设置模式: 0=正常, 1=年, 2=月, 3=日, 4=时, 5=分, 6=秒
u8 set_mode = 0; 

// 每月天数表 (平年)
u8 code month_days[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// 星期字符串表
u8 code *week_str[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// ================== 辅助函数 ==================

// 延时
void delay_ms(u16 ms) {
    u16 i, j;
    for(i = 0; i < ms; i++)
        for(j = 0; j < 110; j++);
}

// 判断闰年
bit is_leap_year(u16 y) {
    // y 为 4位数年份，如 2024
    if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
        return 1;
    }
    return 0;
}

// 获取某年某月的天数
u8 get_max_day(u16 y, u8 m) {
    if (m == 2 && is_leap_year(y)) {
        return 29;
    }
    return month_days[m];
}

// 计算星期几 (基姆拉尔森计算公式)
// 返回值: 0=周日, 1=周一, ..., 6=周六
u8 get_week(u16 y, u8 m, u8 d) {
    u16 w;
    // 1月2月看作上一年的13、14月
    if (m == 1 || m == 2) {
        m += 12;
        y--;
    }
    // 公式: w = (d + 2*m + 3*(m+1)/5 + y + y/4 - y/100 + y/400 + 1) % 7
    // 注意：C语言整数除法自动取整
    w = (d + 2*m + 3*(m+1)/5 + y + y/4 - y/100 + y/400 + 1) % 7;
    return w; 
}

// 更新星期变量
void update_week_var() {
    week = get_week(year, month, day);
}

// ================== LCD 驱动 ==================

void lcd_write_cmd(u8 cmd) {
    LCD_RS = 0;
    LCD_RW = 0;
    LCD_DATA = cmd;
    delay_ms(1);
    LCD_EN = 1;
    delay_ms(1);
    LCD_EN = 0;
}

void lcd_write_data(u8 dat) {
    LCD_RS = 1;
    LCD_RW = 0;
    LCD_DATA = dat;
    delay_ms(1);
    LCD_EN = 1;
    delay_ms(1);
    LCD_EN = 0;
}

void lcd_init() {
    lcd_write_cmd(0x38); // 8位, 2行, 5x7点阵
    lcd_write_cmd(0x0c); // 显示开, 光标关
    lcd_write_cmd(0x06); // 地址指针递增
    lcd_write_cmd(0x01); // 清屏
}

void lcd_set_cursor(u8 x, u8 y) {
    if (y == 0)
        lcd_write_cmd(0x80 + x);
    else
        lcd_write_cmd(0xC0 + x);
}

// 显示字符串 (声明为可重入，解决中断调用警告)
void lcd_show_string(u8 x, u8 y, char *str) reentrant {
    lcd_set_cursor(x, y);
    while (*str) {
        lcd_write_data(*str++);
    }
}

// 显示数字 (2位)
void lcd_show_num2(u8 x, u8 y, u8 num) {
    lcd_set_cursor(x, y);
    lcd_write_data(num / 10 + '0');
    lcd_write_data(num % 10 + '0');
}

// 显示数字 (4位)
void lcd_show_num4(u8 x, u8 y, u16 num) {
    lcd_set_cursor(x, y);
    lcd_write_data(num / 1000 + '0');       // 千位
    lcd_write_data(num % 1000 / 100 + '0');  // 百位
    lcd_write_data(num % 100 / 10 + '0');    // 十位
    lcd_write_data(num % 10 + '0');         // 个位
}

// ================== 逻辑处理 ==================

// 刷新显示
void refresh_display() {
    // 第一行: 2024-03-06 Wed
    lcd_show_num4(0, 0, year); 
    lcd_write_data('-');
    lcd_show_num2(5, 0, month);
    lcd_write_data('-');
    lcd_show_num2(8, 0, day);
    lcd_write_data(' ');
    lcd_show_string(11, 0, week_str[week]); // 显示星期

    // 第二行: 12:00:00
    lcd_show_num2(4, 1, hour);
    lcd_write_data(':');
    lcd_show_num2(7, 1, min);
    lcd_write_data(':');
    lcd_show_num2(10, 1, sec);
    lcd_set_cursor(12, 1); // 秒数第三位的位置，使用空格覆盖LCD旧数据残留
    lcd_write_data(' ');
}

// 时间走时逻辑
void time_update() {
    sec++;
    if (sec > 59) {
        sec = 0;
        min++;
        if (min > 59) {
            min = 0;
            hour++;
            if (hour > 23) {
                hour = 0;
                day++;
                // 日期进位处理

                if (day > get_max_day(year, month)) {
                    day = 1;
                    month++;
                    if (month > 12) {
                        month = 1;
                        year++;
                        if (year > 9999) year = 1900;
                    }
                }
                update_week_var(); // 日期变动，更新星期
            }
        }
    }
}

// 光标控制
void blink_cursor() {
    if (set_mode == 0) {
        lcd_write_cmd(0x0C); // 关闭光标
    } else {
        lcd_write_cmd(0x0F); // 开启光标闪烁
        switch(set_mode) {
            case 1: lcd_set_cursor(3, 0); break;  // 年
            case 2: lcd_set_cursor(6, 0); break;  // 月
            case 3: lcd_set_cursor(9, 0); break;  // 日
            case 4: lcd_set_cursor(5, 1); break;  // 时
            case 5: lcd_set_cursor(8, 1); break;  // 分
            case 6: lcd_set_cursor(11,1); break;  // 秒
        }
    }
}

// 按键扫描
void key_scan() {
    // 模式键
    if (KEY_MODE == 0) {
        delay_ms(10);
        if (KEY_MODE == 0) {
            set_mode++;
            if (set_mode > 6) set_mode = 0;
            while(!KEY_MODE);
        }
    }

    if (set_mode != 0) {
        // 加键
        if (KEY_UP == 0) {
            delay_ms(10);
            if (KEY_UP == 0) {
                switch(set_mode) {
                    case 1: year++; if(year > 9999) year = 1900; break;
                    case 2: month++; if(month > 12) month = 1; if(day > get_max_day(year, month)) day = get_max_day(year, month); break; // 月份加1时，检查日期合法性
                    case 3: day++; if(day > get_max_day(year, month)) day = 1; break;
                    case 4: hour++; if(hour>23)hour=0; break;
                    case 5: min++; if(min>59)min=0; break;
                    case 6: sec++; if(sec>59)sec=0; break;
                }
                // 如果修改了年月日，重新计算星期
                if(set_mode <= 3) update_week_var();
                
                refresh_display();
                blink_cursor();
                while(!KEY_UP);
            }
        }
        // 减键
        if (KEY_DOWN == 0) {
            delay_ms(10);
            if (KEY_DOWN ==0 ) {
                switch(set_mode) {
                    case 1: if(year == 1900) year = 9999; else year--; break;
                    case 2: if(month == 1) month = 12; else month--; if(day > get_max_day(year, month)) day = get_max_day(year, month); break; // 月份减1时，检查日期合法性
                    case 3: if(day == 1) day = get_max_day(year, month); else day--; break;
                    case 4: if(hour==0)hour=23; else hour--; break;
                    case 5: if(min==0)min=59; else min--; break;
                    case 6: if(sec==0)sec=59; else sec--; break;
                }
                // 如果修改了年月日，重新计算星期
                if(set_mode <= 3) update_week_var();

                refresh_display();
                blink_cursor();
                while(!KEY_DOWN);
            }
        }
    }
}

// ================== 定时器 ==================
void timer0_init() {
    TMOD |= 0x01; // 模式1 (16位)
    // 11.0592MHz, 50ms
    TH0 = (65536 - 46080) / 256;
    TL0 = (65536 - 46080) % 256;
    EA = 1;
    ET0 = 1;
    TR0 = 1;
}

u8 timer_count = 0;
bit sec_tick = 0; // 秒钟更新标志

void timer0_isr() interrupt 1 {
    // 重装初值
    TH0 = (65536 - 46080) / 256;
    TL0 = (65536 - 46080) % 256;
    
    timer_count++;
    if (timer_count >= 20) {
        timer_count = 0;
        sec_tick = 1; // 仅置标志，不做其他事
    }
}

// ================== 主函数 ==================
void main() {
    lcd_init();
    
    // 初始化计算一次星期
    update_week_var();
    refresh_display();
    
    timer0_init();
    
    while(1) {
        key_scan();
        blink_cursor();

        if (sec_tick) {
            sec_tick = 0; // 清除标志
            if (set_mode == 0) {
                time_update(); // 在主循环更新时间
                refresh_display(); // 在主循环刷新显示
            }
        }
    }
}
