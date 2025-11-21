/* bootpack */

#include "bootpack.h"
#include <stdio.h>
#include <string.h>


struct FIFO32 sysfifo;
#define KEYCMD_LED 0xed
int app_running = 0;

void HariMain(void)
{
	struct MEM_CONTROL *memman = (struct MEM_CONTROL *) MEMMAN_ADDR;
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	char s[40];
	unsigned int sysbuf[128], keycmd_buf[32];
	struct MOUSE_DEC mdec;
	int mx, my, x, y, j, cursor_x = 8, cursor_y = 28, cursor_c = COL8_000000;
	int mmx = -1, mmy = -1;
	int time_hours = 0, time_minutes = 0, time_seconds = 0;
	unsigned int i;
	int key_shift = 0, key_leds =(binfo->leds >> 4) & 7, keycmd_wait = -1;
	struct SHEET_CONTROL *shtctl;
	struct SHEET *sht_back, *sht_mouse, *sht_window, *sht_command;
	struct SHEET *key_win, *sht_move = 0;
	struct FIFO32 keycmd;
	unsigned char *buf_back, buf_mouse[16 *16], *buf_window, *buf_command;
	struct TASK *task_main, *task_command;
	struct CONSOLE *cons;
	static char keytable0[0x80] = {
		0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,   0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', 0,   0,   'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', 0,   0,   '\\', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   '`',   0x5c, 0,  0,   0,   0,   0,   0,   0,   0,   0,   0x5c, 0,  0
	};
	static char keytable1[0x80] = {
		0,   0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,   0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0,   0,   'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', 0,   0,   '|', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,  '`', '~',  0,   0,   0,   0,   0,   0,   0,   0,   0,   '|', 0,   0
	};

	// ====================================================================
	// 系统初始化
	init_gdtidt();
	init_pic();
	io_sti(); /* IDT/PIC的初始化已经完成，于是开放CPU的中断 */
	fifo32_init(&sysfifo, 128, sysbuf, 0);
	fifo32_init(&keycmd, 32, keycmd_buf, 0);
	init_pit();// 初始化PIT 定时器
	io_out8(PIC0_IMR, 0xf8); /* 开放PIT和PIC1和键盘中断(11111000) */
	io_out8(PIC1_IMR, 0xef); /* 开放鼠标中断(11101111) */
	memory_init(memman);// 初始化内存管理
	init_keyboard();// 初始化键盘
	enable_mouse(&mdec);// 初始化鼠标
	timer_control_init(&sysfifo);// 初始化定时器
	task_main = mtask_init(memman, &sysfifo);// 初始化任务管理
	init_palette();// 初始化调色板
	shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);//12k 初始化图层管理
	*((int *)0x0fe4) = (int)shtctl;

	// ====================================================================
	// 定时器
	struct TIMER *timer_cursor = timer_alloc();// 申请光标闪烁定时器
	timer_settime(timer_cursor, &sysfifo, 1);
	timer_login(timer_cursor, 50);

	struct TIMER *timer_time = timer_alloc();// 申请时间定时器
	timer_settime(timer_time, &sysfifo, 3);
	timer_login(timer_time, 100);// 1s

	// ====================================================================
	// 背景图层和鼠标图层
	sht_back = sheet_alloc(shtctl);// 申请sheet
	sht_mouse = sheet_alloc(shtctl);// 申请sheet
	buf_back = (unsigned char *)memory_alloc_4k(memman, (binfo->scrnx * binfo->scrny), 0x00)->addr;//4k
	sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1);// 不透明
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99);// 透明色号99
	init_screen8(buf_back, binfo->scrnx, binfo->scrny);// 初始化背景图层
	init_mouse_cursor8(buf_mouse, 99);

	// ====================================================================
	// 任务main
	sht_window = sheet_alloc(shtctl);// 申请sheet
	key_win = sht_window;
	buf_window = (unsigned char *)memory_alloc_4k(memman, (144 * 52), 0x00)->addr;//4k
	sheet_setbuf(sht_window, buf_window, 144, 52, -1);// 不透明
	make_window8(buf_window, 144, 52, "task_main", 1);// 活动窗口
	make_textbox8(sht_window, 8, 28, 128, 16, COL8_FFFFFF);

	// 命令窗口任务
	sht_command = sheet_alloc(shtctl);// 申请sheet
	buf_command = (unsigned char *)memory_alloc_4k(memman, (256 * 165), 0x00)->addr;//4k
	sheet_setbuf(sht_command, buf_command, 256, 165, -1);// 不透明
	sht_command->flags |= 0x20;// 命令窗口
	make_window8(buf_command, 256, 165, "command", 0);// 活动窗口
	make_textbox8(sht_command, 8, 28, 240, 128, COL8_000000);

	task_command = mtask_alloc();// 申请任务命令
	task_command->tss.eip = (int) &task_command_main;
	task_command->tss.esp = ((int) memory_alloc_4k(memman, 64 *1024, 0x00)->addr) + 64 * 1024 - 8;// 空闲任务栈指针
	task_command->tss.es = 1 * 8;
	task_command->tss.cs = 2 * 8;
	task_command->tss.ss = 1 * 8;
	task_command->tss.ds = 1 * 8;
	task_command->tss.fs = 1 * 8;
	task_command->tss.gs = 1 * 8;
	*((int *) (task_command->tss.esp + 4)) = (int) sht_command;// 任务命令的栈指针 64k 指向命令sheet
	sht_command->task = task_command;
	mtask_run(task_command, 2, 2);// 运行空闲任务
	
	// ====================================================================
	// 图层调整
	sheet_slide(sht_back, 0, 0);// 显示背景sheet
	mx = (binfo->scrnx - 16) / 2; /* 计算画面的中心坐标*/
	my = (binfo->scrny - 28 - 16) / 2;
	sheet_slide(sht_mouse, mx, my);// 显示鼠标sheet
	sheet_slide(sht_window, 8, 56);// 显示窗口sheet
	sheet_slide(sht_command, binfo->scrnx/2 - 128, binfo->scrny/2 - 82);// 显示命令sheet

	sheet_updown(sht_back, 0);// 背景sheet置底
	sheet_updown(sht_window, 1);// 窗口sheet置顶
	sheet_updown(sht_command, 2);// 命令sheet置顶
	sheet_updown(sht_mouse, shtctl->top+1);// 鼠标sheet置顶

	// ====================================================================
	// 重要信息打印
	sprintf(s, "(%d, %d)", mx, my);
	putfonts8_asc(buf_back, binfo->scrnx, 0, 0, COL8_FFFFFF, s);

	// ====================================================================
	// 刷新图层和运行任务
	sheet_refresh(sht_back, 0, 0, binfo->scrnx, 48);// 刷新背景sheets
	show_time(sht_back, binfo, COL8_000000, COL8_C6C6C6, time_hours, time_minutes, time_seconds);// 显示时间

	// ====================================================================
	// 为了避免和键盘当前状态冲突，在一开始先进行设置
	fifo32_put(&keycmd, KEYCMD_LED);
	fifo32_put(&keycmd, key_leds);

	for (;;) {
		if (fifo32_status(&keycmd) > 0 && keycmd_wait < 0) {
			//如果存在向键盘控制器发送的数据，则发送它
			keycmd_wait = fifo32_get(&keycmd);
			wait_kbc();
			io_out8(PORT_KEYDAT, keycmd_wait);
		}
		io_cli();
		if(fifo32_status(&sysfifo) == 0){
			mtask_sleep(task_main);
			io_stihlt();
		}else{
			i = fifo32_get(&sysfifo);
			io_sti();
			if (key_win->flags == 0) { /*输入窗口被关闭*/
				key_win = shtctl->sheets[shtctl->top - 1];
				cursor_c = keywin_on(key_win, sht_window, cursor_c);
			}
			if (256 <= i && i <= 511){// 键盘中断
				if (i < 0x80 + 256) { //将按键编码转换为字符编码
					if (key_shift == 0) {
						s[0] = keytable0[i - 256];
					} else {
						s[0] = keytable1[i - 256];
					}
				} else {
					s[0] = 0;
				}
				if ('A' <= s[0] && s[0] <= 'Z') { //当输入字符为英文字母时
					if (((key_leds & 4) == 0 && key_shift == 0) ||((key_leds & 4) != 0 && key_shift != 0)) {
						s[0] += 0x20; //将大写字母转换为小写字母
					}
				}
				if (s[0] != 0) {// 一般字符
					if (key_win == sht_window) {
						if (cursor_x < 128) {
							s[1] = 0;
							putfonts8_asc_sht(sht_window, cursor_x, cursor_y, COL8_000000, COL8_FFFFFF, s, 1);
							cursor_x += 8;
						}
					}else// 未运行应用程序
					{
						fifo32_put(key_win->task->fifo, s[0] + 256);
					}
				}
				if (i == 0x0e + 256){// 退格键
					if (key_win == sht_window) {
						if (cursor_x > 8){
							cursor_x -= 8;
							putfonts8_asc_sht(sht_window, cursor_x, cursor_y, COL8_000000, COL8_FFFFFF, " ", 2);// 两个字符，防止光标残留						
						}
					}else{
						fifo32_put(key_win->task->fifo, 8 + 256);// ASCLL
					}
				}
				if (i == 0x1c + 256) { //回车键
					if (key_win != sht_window) { //发送至命令行窗口
						fifo32_put(key_win->task->fifo, 10 + 256);
					}
				}
				if (i == 0x0f + 256) { // Tab键
					cursor_c = keywin_off(key_win, sht_window, cursor_c, cursor_x);
					j = key_win->height - 1;
					if (j == 0) {
						j = shtctl->top - 1;
					}
					key_win = shtctl->sheets[j];
					cursor_c = keywin_on(key_win, sht_window, cursor_c);
				}
				if (i == 0x2a + 256) {// 左shift键 ON
					key_shift |= 1;
				}
				if (i == 0x36 + 256) { //右Shift ON
					key_shift |= 2;
				}
				if (i == 0xaa + 256) { //左Shift OFF
					key_shift &= ~1;
				}
				if (i == 0xb6 + 256) { //右Shift OFF
					key_shift &= ~2;
				}
				if (i == 0x3a + 256) { // CapsLock
					key_leds ^= 4;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				if (i == 0x45 + 256) { // NumLock
					key_leds ^= 2;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				if (i == 0x46 + 256) { // ScrollLock
					key_leds ^= 1;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				if (i == 0x3b + 256 && key_shift != 0 && app_running != 0 && key_win == sht_command) { // F1 + Shift结束程序
					cons = (struct CONSOLE *) *((int *) 0x0fec);
					cmd_putstr0(cons, "\nBreak For Key.\n");
					io_cli();// 关闭中断，不能在改变寄存器时切换任务
					task_command->tss.eax = (int) &(task_command->tss.esp0);
					task_command->tss.eip = (int) asm_end_app;
					io_sti();// 开启中断
				}
				if (i == 0x57 + 256 && shtctl->top > 2)// f11键调整图层
				{
					sheet_updown(shtctl->sheets[1], shtctl->top - 1);// 窗口sheet上移
				}
				if (i == 0xfa + 256) { //键盘成功接收到数据
					keycmd_wait = -1;
				}
				if (i == 0xfe + 256) { //键盘没有成功接收到数据
					wait_kbc();
					io_out8(PORT_KEYDAT, keycmd_wait);
				}
			}else if(512 <= i && i <= 767){// 鼠标中断
				if (mouse_decode(&mdec, i-512) == 1){
					// 三字节都齐了
					sprintf(s, "[lcr %4d %4d]", mdec.x, mdec.y);
					if ((mdec.btn & 0x01) != 0) {
						s[1] = 'L';
					}
					if ((mdec.btn & 0x02) != 0) {
						s[3] = 'R';
					}
					if ((mdec.btn & 0x04) != 0) {
						s[2] = 'C';
					}
					boxfill8(buf_back, binfo->scrnx, COL8_008484, 32, 16, 32 + 15 * 8 - 1, 31);
					putfonts8_asc(buf_back, binfo->scrnx, 32, 16, COL8_FFFFFF, s);
					sheet_refresh(sht_back, 32, 16, 32 + 15 * 8, 32);// 刷新背景sheet
					mx += mdec.x;
					my += mdec.y;
					if (mx < 0) {
						mx = 0;
					}
					if (my < 0) {
						my = 0;
					}
					if (mx > binfo->scrnx - 1) {
						mx = binfo->scrnx - 1;
					}
					if (my > binfo->scrny - 1) {
						my = binfo->scrny - 1;
					}
					sprintf(s, "(%3d, %3d)", mx, my);
					boxfill8(buf_back, binfo->scrnx, COL8_008484, 0, 0, 79, 15); /* 隐藏坐标 */
					putfonts8_asc(buf_back, binfo->scrnx, 0, 0, COL8_FFFFFF, s); /* 显示坐标 */
					sheet_refresh(sht_back, 0, 0, 80, 16);// 刷新背景sheet
					sheet_slide(sht_mouse, mx, my);// 移动鼠标sheet
					if ((mdec.btn & 0x01) != 0) { /* 按下左键 */
						if (mmx < 0)// 处于通常模式下
						{
							for (j = shtctl->top - 1; j > 0; j--)// 从下往上遍历sheet
							{
								sht_move = shtctl->sheets[j];
								x = mx - sht_move->vx0;
								y = my - sht_move->vy0;
								if (x >= 0 && x < sht_move->bxsize && y >= 0 && y < sht_move->bysize)// 点击在sheet范围内
								{
									if (sht_move->buf[y * sht_move->bxsize + x] != sht_move->col_inv){// 点击在sheet上的字符
										sheet_updown(sht_move, shtctl->top - 1);// 置顶sheet
										if (sht_move != key_win)
										{
											cursor_c = keywin_off(key_win, sht_window, cursor_c, cursor_x);
											key_win = sht_move;
											cursor_c = keywin_on(key_win, sht_window, cursor_c);
										}
										if (x >=3 && x< sht_move->bxsize -3 && y >=3 && y< 21)// 处于窗口栏，进入移动模式
										{
											mmx = mx; /*更新为移动后的坐标*/
											mmy = my;
										}
										if (sht_move->bxsize - 21 <= x && x < sht_move->bxsize - 5 && 5 <=y && y < 19)
										{
											/*点击“×”按钮*/
											if ((sht_move->flags & 0x10) != 0) { /*该窗口是否为应用程序窗口？*/
												if (key_win != sht_command)
												{
													fifo32_put(sht_command->task->fifo, 3);// 发送关闭光标闪烁
												}
												cons = (struct CONSOLE *) *((int *) 0x0fec);
												cmd_putstr0(cons, "\nProgram closed.\n");
												io_cli(); /*强制结束处理中禁止切换任务*/
												task_command->tss.eax = (int)&(task_command->tss.esp0);
												task_command->tss.eip = (int) asm_end_app;
												io_sti();
											}
										}
										break;
									}
								}
							}
						}else// 处于拖动模式下
						{
							/*如果处于窗口移动模式*/
							x = mx - mmx; /*计算鼠标的移动距离*/
							y = my - mmy;
							sheet_slide(sht_move, sht_move->vx0 + x, sht_move->vy0 + y);
							mmx = mx; /*更新为移动后的坐标*/
							mmy = my;
						}
					}else// 松开左键
					{
						mmx = -1; /* 重置拖动模式标志 */
					}
				}
			}else if(i == 1){// 光标闪烁
				timer_settime(timer_cursor, &sysfifo, 0);
				timer_login(timer_cursor, 50);
				cursor_c = COL8_000000;
			}else if (i == 0){// 光标不闪烁
				timer_settime(timer_cursor, &sysfifo, 1);
				timer_login(timer_cursor, 50);
				cursor_c = COL8_FFFFFF;
			}else if (i == 3){// 系统时间定时器
				time_seconds++;
				if(time_seconds >= 60){
					time_seconds = 0;
					time_minutes++;
					if(time_minutes >= 60){
						time_minutes = 0;
						time_hours++;
						}
					}
				show_time(sht_back, binfo, COL8_000000, COL8_C6C6C6, time_hours, time_minutes, time_seconds);
				timer_login(timer_time, 100);
			}else if (i == 0xff){// 定时器重置
				int count = timerctl.count;
				timerctl.count = 0;
				for (j = 0; j < MAX_TIMERS; j++)
				{
					timerctl.timers[j].time_out -= count;
				}
				timer_control_init(&sysfifo);
			}
			// 光标显示
			if (key_win == sht_window) {
				boxfill8(sht_window->buf, sht_window->bxsize, cursor_c, cursor_x, cursor_y, cursor_x + 7, cursor_y + 15);
				sheet_refresh(sht_window, cursor_x, cursor_y, cursor_x + 8, cursor_y + 16);// 刷新窗口sheet
			}
		}
	}
}
