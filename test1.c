#include <stdio.h>

#define COL8_000000 0  // 黑色
#define COL8_FF0000 1  // 亮红
#define COL8_00FF00 2  // 亮绿
#define COL8_FFFF00 3  // 亮黄
#define COL8_0000FF 4  // 亮蓝
#define COL8_FF00FF 5  // 亮紫
#define COL8_00FFFF 6  // 浅亮蓝
#define COL8_FFFFFF 7  // 白
#define COL8_C6C6C6 8  // 亮灰
#define COL8_840000 9  // 暗红
#define COL8_008400 10 // 暗绿
#define COL8_848400 11 // 暗黄
#define COL8_000084 12 // 暗青
#define COL8_840084 13 // 暗紫
#define COL8_008484 14 // 浅暗蓝
#define COL8_848484 15 // 暗灰

void api_putchar(int c);   // 打印字符（command）
void api_putstr0(char *s); // 打印字符串（command）

void api_initmalloc(void);				   // 初始化内存管理
char *api_malloc(int size);				   // 申请内存
void api_mallocfree(char *addr, int size); // 释放内存

int api_openwin(char *buf, int xsiz, int ysiz, int col_inv, char *title); // 创建窗口
void api_putstrwin(int win, int x, int y, int col, int len, char *str);	  // 窗口打印字符
void api_boxfilwin(int win, int x0, int y0, int x1, int y1, int col);	  // 窗口画矩形
void api_point(int win, int x, int y, int col);							  // 窗口画点
void api_linewin(int win, int x0, int y0, int x1, int y1, int col);		  // 窗口画线
void api_refreshwin(int win, int x0, int y0, int x1, int y1);			  // 窗口刷新
void api_closewin(int win);												  // 关闭窗口

int api_getkey(int mode); // 获取键盘输入，0表示不休眠等待键盘输入，1表示休眠等待键盘输入

int api_alloctimer(void);				 // 申请定时器
void api_inittimer(int timer, int data); // 设置定时器超时数据
void api_settimer(int timer, int time);	 // 设置定时器超时时间
void api_freetimer(int timer);			 // 释放定时器

void api_end(void); // 程序结束

/*
void HariMain(void)
{
	char *buf;
	int win, i, s[30];

	api_initmalloc();
	api_putstr0("Window creation completed!");
	buf = api_malloc(150 * 100);
	win = api_openwin(buf, 150, 100, -1, "testwin");
	api_boxfilwin(win + 1, 6, 26, 142, 92, COL8_000000);			  // 窗口画矩形，不立即刷新
	api_putstrwin(win + 1, 28, 28, COL8_FF0000, 13, "hello world !"); // 窗口打印字符，不立即刷新
	api_point(win + 1, 75, 59, COL8_FFFF00);						  // 窗口画点，不立即刷新
	api_linewin(win + 1, 12, 58, 141, 58, COL8_FFFFFF);				  // 窗口画线，不立即刷新
	api_refreshwin(win, 6, 26, 143, 93);							  // 窗口刷新

	int timer = api_alloctimer();
	api_inittimer(timer, 128);
	api_settimer(timer, 300);  // 只设置一次定时器
	for (;;)
	{
		i = api_getkey(1);  // 等待事件
		if (i <= 256)
		{
			s[0] = i;
			s[1] = 0;
			if (s[0] >= 'a' && s[0] <= 'z')
			{
				api_putstrwin(win, 28, 68, COL8_FF0000, 1, s);
			}
			else if (s[0] == 0x0a) // 按下Enter键
			{
				break;
			}
			else if (s[0] == 0x08) // 按下Backspace键
			{
				api_boxfilwin(win, 28, 66, 37, 83, COL8_000000);
			}
		}
	}
	api_closewin(win); // 关闭窗口
	api_end();
	return;
}
*/

void HariMain(void)
{
	char *buf, s[12];
	int win, timer, sec = 0, min = 0, hou = 0, key;
	api_initmalloc();
	buf = api_malloc(150 * 50);
	win = api_openwin(buf, 150, 50, -1, "noodle");
	
	// 分配并初始化定时器
	timer = api_alloctimer();
	/* 
	 * 关键修改：通过分析api.c，我发现定时器数据处理有特殊逻辑
	 * api_inittimer(timer, data) 会在系统内部将data加上256
	 * 但inthandler20中断处理函数会直接将原始data放入FIFO
	 * 所以这里设置的data值应该与api_getkey中期望接收的值一致
	 */
	api_inittimer(timer, 128); // 设置定时器数据为128
	
	// 初始绘制
	sprintf(s, "%5d:%02d:%02d", hou, min, sec);
	api_boxfilwin(win, 28, 27, 115, 41, 7);/*白色*/
	api_putstrwin(win, 28, 27, 0, 11, s); /*黑色*/
	
	// 启动定时器
	api_settimer(timer, 101); /* 1秒 */
	
	for (;;) {
		// 等待事件
		key = api_getkey(1);
		
		// 检查是否是定时器事件
		// 通过分析api.c和int.c，我发现：
		// 1. api_inittimer中的data会被api.c处理为data+256
		// 2. 但inthandler20中断处理时直接使用原始data值
		// 3. 所以api_getkey实际返回的是原始data值，不是data+256-256
		if (key == 128) {
			// 定时器事件，更新时间并显示
			sec++;
			if (sec == 60) {
				sec = 0;
				min++;
				if (min == 60) {
					min = 0;
					hou++;
				}
			}
			
			// 重新显示时间
			sprintf(s, "%5d:%02d:%02d", hou, min, sec);
			api_boxfilwin(win, 28, 27, 115, 41, 7);/*白色*/
			api_putstrwin(win, 28, 27, 0, 11, s); /*黑色*/
			
			// 重新设置定时器
			api_settimer(timer, 101); /* 1秒 */
		}
		// 检查是否是键盘事件
		else if (key >= 0) {
			// 任何键盘输入都退出循环
			break;
		}
	}
	
	// 释放资源
	api_freetimer(timer);
	api_closewin(win);
	api_end();
}