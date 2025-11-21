#include "bootpack.h"
#include <stdio.h>
#include <string.h>

void task_command_main(struct SHEET *sht_command)
{
	struct FIFO32 *fifo;
	struct TIMER *timer_cursor; // 光标闪烁定时器
	struct TASK *task = task_now();
	int i, fifobuf[128];
	int cursor_flag = 0;
	char s[30], cmdline[30];
	struct MEM_CONTROL *memctl = (struct MEM_CONTROL *)MEMMAN_ADDR;			 // 内存管理结构体
	struct FILEINFO *fileinfo = (struct FILEINFO *)(ADR_DISKIMG + 0x002600); // 磁盘镜像结构体
	struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *)ADR_GDT;
	struct CONSOLE cons;
	cons.sht = sht_command;
	cons.cur_x = 24;
	cons.cur_y = 28;
	cons.cur_c = COL8_000000;
	*((int *)0x0fec) = (int)&cons; // 指向控制台结构体
	int *fat = (int *)memory_alloc_4k(memctl, 4 * 2280, 0x00)->addr;

	fifo32_init(fifo, 128, fifobuf, task);						  // 任务休眠后需要唤醒
	task->fifo = fifo;											  // 任务队列
	file_readfat(fat, (unsigned char *)(ADR_DISKIMG + 0x000200)); // 读取FAT表，解压缩

	timer_cursor = timer_alloc();
	cons.timer = timer_cursor;
	timer_settime(timer_cursor, fifo, 1);
	timer_login(timer_cursor, 50);

	/*显示提示符*/
	putfonts8_asc_sht(sht_command, 8, cons.cur_y, COL8_FFFFFF, COL8_000000, ">>", 2);

	for (;;)
	{
		io_cli();
		if (fifo32_status(task->fifo) == 0)
		{
			mtask_sleep(task);
			io_sti();
		}
		else
		{
			i = fifo32_get(task->fifo);
			io_sti();
			if (i <= 1) /*光标用定时器*/
			{
				if (i != 0)
				{
					timer_settime(timer_cursor, fifo, 0); /*下次置0 */
					cons.cur_c = COL8_FFFFFF;
				}
				else
				{
					timer_settime(timer_cursor, fifo, 1); /*下次置1 */
					cons.cur_c = COL8_000000;
				}
				timer_login(timer_cursor, 50);
			}
			if (i == 2) // 打开光标闪烁
			{
				cursor_flag = 1;
			}
			if (i == 3) // 关闭光标闪烁
			{
				cursor_flag = 0;
				putfonts8_asc_sht(sht_command, cons.cur_x, cons.cur_y, COL8_FFFFFF, COL8_000000, " ", 2); // 两个字符，防止光标残留
			}
			if (256 <= i && i <= 511) // 键盘数据
			{
				if (i == 8 + 256) // 退格键
				{
					if (cons.cur_x > 24)
					{
						cons.cur_x -= 8;
						putfonts8_asc_sht(sht_command, cons.cur_x, cons.cur_y, COL8_FFFFFF, COL8_000000, " ", 2); // 两个字符，防止光标残留
					}
				}
				else if (i == 10 + 256) // 回车键
				{
					putfonts8_asc_sht(sht_command, cons.cur_x, cons.cur_y, COL8_FFFFFF, COL8_000000, " ", 1); // 显示空格，防止光标残留
					cmdline[cons.cur_x / 8 - 3] = 0;
					cmd_newline(&cons);
					// 执行命令
					cmd_runcode(&cons, fileinfo, memctl, gdt, fat, cmdline);
					// 显示提示符
					putfonts8_asc_sht(sht_command, 8, cons.cur_y, COL8_FFFFFF, COL8_000000, ">>", 2);
				}
				else // 一般字符
				{
					if (cons.cur_x < sht_command->bxsize - 16)
					{
						s[0] = i - 256;
						s[1] = 0;
						cmdline[cons.cur_x / 8 - 3] = i - 256;
						putfonts8_asc_sht(sht_command, cons.cur_x, cons.cur_y, COL8_00FF00, COL8_000000, s, 1);
						cons.cur_x += 8;
					}
				}
			}
			if (cursor_flag != 0) // 光标闪烁
			{
				boxfill8(sht_command->buf, sht_command->bxsize, cons.cur_c, cons.cur_x, cons.cur_y, cons.cur_x + 7, cons.cur_y + 15);
				sheet_refresh(sht_command, cons.cur_x, cons.cur_y, cons.cur_x + 8, cons.cur_y + 16); // 刷新命令行sheet
			}
		}
	}
}

void cmd_newline(struct CONSOLE *cons)
{
	int y, x;
	if (cons->cur_y < cons->sht->bysize - 32)
	{
		cons->cur_y += 16;
	}
	else
	{
		// 命令行满了，滚动显示
		for (y = 28; y < 28 + 112; y++)
		{
			for (x = 8; x < 8 + 240; x++)
			{
				cons->sht->buf[x + y * cons->sht->bxsize] = cons->sht->buf[x + (y + 16) * cons->sht->bxsize];
			}
		}
		for (y = 28 + 112; y < 28 + 128; y++)
		{
			for (x = 8; x < 8 + 240; x++)
			{
				cons->sht->buf[x + y * cons->sht->bxsize] = COL8_000000;
			}
		}
		sheet_refresh(cons->sht, 8, 28, 8 + 240, 28 + 128);
	}
	cons->cur_x = 24;
}

void cmd_mem(struct CONSOLE *cons, struct MEM_CONTROL *memctl)
{
	char s[60];
	int total_kb = memctl->total_size / 1024;
	int free_kb = memctl->free_size / 1024;
	int used_kb = total_kb - free_kb;
	int percentage;
	
	// 使用整数运算计算百分比，避免浮点数操作
	sprintf(s, "memory %dkB free: %dKB", total_kb, free_kb);
	putfonts8_asc_sht(cons->sht, 16, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 28);
	cmd_newline(cons);
	
	// 整数运算计算百分比和两位小数
	// 先乘以10000再除，保留两位小数的精度
	percentage = (used_kb * 10000) / total_kb;
	sprintf(s, "use: %d.%02d%%", percentage / 100, percentage % 100);
	putfonts8_asc_sht(cons->sht, 16, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 28);
	cmd_newline(cons);
	return;
}

void cmd_clear(struct CONSOLE *cons)
{
	boxfill8(cons->sht->buf, cons->sht->bxsize, COL8_000000, 8, 28, cons->sht->bxsize - 8, cons->sht->bysize - 9);
	sheet_refresh(cons->sht, 8, 28, cons->sht->bxsize - 8, cons->sht->bysize - 9); // 刷新命令行sheet
	cons->cur_x = 24;
	cons->cur_y = 28;
	return;
}

void cmd_ls(struct CONSOLE *cons, struct FILEINFO *fileinfo)
{
	int x, y;
	char s[30];
	for (x = 0; x < 224; x++)
	{
		if (fileinfo[x].name[0] == 0x00)
		{ // 文件名为空，表示没有文件了
			break;
		}
		if (fileinfo[x].name[0] != 0xe5)
		{ // 不是删除文件
			if ((fileinfo[x].type & 0x18) == 0)
			{														   // 判断是否为普通文件
				sprintf(s, "filename.ext %7d byte", fileinfo[x].size); // 格式化文件名和文件大小
				for (y = 0; y < 8; y++)
				{
					s[y] = fileinfo[x].name[y]; // 复制文件名到s
				}
				s[9] = fileinfo[x].ext[0]; // 复制扩展名到s
				s[10] = fileinfo[x].ext[1];
				s[11] = fileinfo[x].ext[2];
				if (s[9] == 'R' && s[10] == 'F')//判断是否为可执行文件
				{
					putfonts8_asc_sht(cons->sht, 8, cons->cur_y, COL8_FF0000, COL8_000000, s, 30);
				}
				else
				{
					putfonts8_asc_sht(cons->sht, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 30);
				}
				cmd_newline(cons);
			}
		}
	}
}

void cmd_cat(struct CONSOLE *cons, struct FILEINFO *fileinfo, struct MEM_CONTROL *memctl, int *fat, char *cmdline)
{
	int x, y;
	char s[30], *p;
	for (y = 0; y < 11; y++) // 初始化文件名数组
	{
		s[y] = ' ';
	}
	y = 0;
	for (x = 4; y < 11 && cmdline[x] != 0; x++) // 复制文件名到s
	{
		if (cmdline[x] == '.' && y <= 8)
		{
			y = 8;
		}
		else
		{
			s[y] = cmdline[x]; // 复制文件名到s
			if ('a' <= s[y] && s[y] <= 'z')
			{
				s[y] -= 0x20; // 转换为大写字母
			}
			y++;
		}
	}
	for (x = 0; x < 224;) // 查找文件
	{
		if (fileinfo[x].name[0] == 0x00)
		{ // 文件名为空，表示没有文件了
			break;
		}
		if ((fileinfo[x].type & 0x18) == 0)
		{ // 判断是否为普通文件，而非文件夹
			for (y = 0; y < 11; y++)
			{
				if (fileinfo[x].name[y] != s[y])
				{ // 文件名不匹配
					goto next_file;
				}
			}
			break; // 文件名匹配，跳出循环
		}
	next_file:
		x++;
	}
	if (x < 224 && fileinfo[x].name[0] != 0x00) // 找到文件
	{
		y = fileinfo[x].size;														 // 获取文件大小
		struct MEM_USE_INFO *use1 = memory_alloc_4k(memctl, fileinfo[x].size, 0x01); // 分配内存
		p = (char *)(use1->addr);
		file_loadfile(&fileinfo[x], fat, (unsigned char *)(ADR_DISKIMG + 0x003e00), p); // 加载文件
		cons->cur_x = 8;																// 重置光标位置
		for (y = 0; y < fileinfo[x].size; y++)
		{ // 显示文件内容
			s[0] = p[y];
			s[1] = 0;
			if (s[0] == 0x09) // 缩进制表符
			{
				for (;;)
				{
					putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, " ", 1);
					cons->cur_x += 8;
					if (cons->cur_x == cons->sht->bxsize - 8)
					{
						cmd_newline(cons);
						cons->cur_x = 8;
					}
					if (((cons->cur_x - 8) & 0x1f) == 0)
					{ // 被32整除，换行
						break;
					}
				}
			}
			else if (s[0] == 0x0a) // 换行符
			{
				cmd_newline(cons);
				cons->cur_x = 8;
			}
			else if (s[0] == 0x0d) // 回车符
			{
				continue;
			}
			else // 普通字符
			{
				putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 1);
				cons->cur_x += 8;
				if (cons->cur_x == cons->sht->bxsize - 8)
				{
					cmd_newline(cons);
					cons->cur_x = 8;
				}
			}
		}
		cmd_newline(cons);
		memory_free(memctl, use1); // 释放内存
	}
	else // 文件不存在
	{
		putfonts8_asc_sht(cons->sht, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, "File not found.", 14);
		cmd_newline(cons);
	}
	return;
}

void cmd_app(struct CONSOLE *cons, struct FILEINFO *fileinfo, struct MEM_CONTROL *memctl, struct SEGMENT_DESCRIPTOR *gdt, int *fat, char *cmdline)
{
	struct TASK *task = task_now();
	struct SHEET *sht;
	struct SHEET_CONTROL *shtctl = (struct SHEET_CONTROL *)*((int *)0x0fe4);
	extern int app_running;
	char s[30], *p, *q;
	int x, y;
	int i, segsiz, datsiz, esp, dathrb;
	for (y = 0; y < 11; y++) // 初始化文件名数组
	{
		s[y] = ' ';
	}
	y = 0;
	for (x = 4; y < 11 && cmdline[x] != 0; x++) // 复制文件名到s
	{

		s[y] = cmdline[x]; // 复制文件名到s
		if ('a' <= s[y] && s[y] <= 'z')
		{
			s[y] -= 0x20; // 转换为大写字母
		}
		y++;
	}
	if (s[8] != ' ')
	{
		return;
	} // 文件名不能超过8个字符
	s[8] = 'R';
	s[9] = 'F';
	for (x = 0; x < 224;) // 查找文件
	{
		if (fileinfo[x].name[0] == 0x00)
		{ // 文件名为空，表示没有文件了
			break;
		}
		if ((fileinfo[x].type & 0x18) == 0)
		{ // 判断是否为普通文件，而非文件夹
			for (y = 0; y < 11; y++)
			{
				if (fileinfo[x].name[y] != s[y])
				{ // 文件名不匹配
					goto next_file_hlt;
				}
			}
			break; // 文件名匹配，跳出循环
		}
	next_file_hlt:
		x++;
	}
	if (x < 224 && fileinfo[x].name[0] != 0x00) // 找到文件
	{
		struct MEM_USE_INFO *use1 = memory_alloc_4k(memctl, fileinfo[x].size, 0x01); // 分配内存
		p = (char *)(use1->addr);// 代码的地址
		file_loadfile(&fileinfo[x], fat, (unsigned char *)(ADR_DISKIMG + 0x003e00), p); // 加载文件
		cons->cur_x = 8;
		if (fileinfo[x].size >= 8 && strncmp(p + 4, "Hari",4) == 0 && *p == 0x00)// 由c语言编写的应用程序,*p为0x00表示未分配数据段空间
		{// c语言编写的应用程序的入口地址为0x1b
			segsiz = *((int *) (p + 0x0000));// 请求分配段大小
			esp    = *((int *) (p + 0x000c));// 栈指针&数据传送地址
			datsiz = *((int *) (p + 0x0010));// 数据大小
			dathrb = *((int *) (p + 0x0014));// 数据头指针
			struct MEM_USE_INFO *use2 = memory_alloc_4k(memctl, segsiz, 0x01); // 分配内存
			q = (char *)(use2->addr);// 数据的地址
			*((int *)0xfe8) = (int) q;//将数据段开始地址写入0xfe8
			set_segmdesc(gdt + 1003, fileinfo[x].size - 1, (int)p, AR_CODE32_ER + 0x60);
			set_segmdesc(gdt + 1004, segsiz - 1			 , (int)q, AR_DATA32_RW + 0x60);
			for (i = 0; i < datsiz; i++)// 复制数据到数据段
			{
				q[esp + i] = p[dathrb + i];
			}
			app_running ++;// 应用程序正在运行
			change_wtitle8(cons->sht, 0);
			start_app(0x1b, 1003 * 8, esp, 1004 * 8, &(task->tss.esp0));
			change_wtitle8(cons->sht, 1);
			for (i = 0; i < MAX_SHEETS; i++)
			{
				sht = &(shtctl->sheet[i]);
				if (sht->task == task && ((sht->flags & 0x11) == 0x11))// 找到关联任务的sheet
				{
					sheet_free(sht);
				}
			}
			memory_free(memctl, use2); // 释放内存
		}else
		{
			cmd_putstr0(cons, ".rf file format error.\n");
		}
		cmd_newline(cons);
		memory_free(memctl, use1); // 释放内存
		app_running --;// 应用程序不再运行
	}
	else // 文件不存在或不能运行
	{
		putfonts8_asc_sht(cons->sht, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, "File not found or cannot run.", 30);
		cmd_newline(cons);
	}
	return;
}

void cmd_runcode(struct CONSOLE *cons, struct FILEINFO *fileinfo, struct MEM_CONTROL *memctl, struct SEGMENT_DESCRIPTOR *gdt, int *fat, char *cmdline)
{
	if (strcmp(cmdline, "mem") == 0) // 显示内存信息
	{
		cmd_mem(cons, memctl);
	}
	else if (strcmp(cmdline, "clear") == 0) // 清除屏幕
	{
		cmd_clear(cons);
	}
	else if (strcmp(cmdline, "ls") == 0) // 显示目录
	{
		cmd_ls(cons, fileinfo);
	}
	else if (strncmp(cmdline, "cat ", 4) == 0) // 显示文件内容
	{
		cmd_cat(cons, fileinfo, memctl, fat, cmdline);
	}
	else if (strncmp(cmdline, "run ", 4) == 0) // 运行应用程序
	{
		cmd_app(cons, fileinfo, memctl, gdt, fat, cmdline);
	}
	else if (cmdline[0] != 0) // 不是命令也不是空行
	{
		putfonts8_asc_sht(cons->sht, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, "Bad command.", 12);
		cmd_newline(cons);
	}
}

void cmd_putchar(struct CONSOLE *cons, int chr, char move)
{
	char s[2];
	s[0] = chr;
	s[1] = 0;
	if (s[0] == 0x09)
	{ /*制表符*/
		for (;;)
		{
			putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, " ", 1);
			cons->cur_x += 8;
			if (cons->cur_x == 8 + 240)
			{
				cmd_newline(cons);
			}
			if (((cons->cur_x - 8) & 0x1f) == 0)
			{
				break; /*被32整除则break*/
			}
		}
	}
	else if (s[0] == 0x0a)
	{ /*换行*/
		cmd_newline(cons);
	}
	else if (s[0] == 0x0d)
	{	/*回车*/
		/*先不做任何操作*/
	}
	else
	{ /*一般字符*/
		putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 1);
		if (move != 0)
		{
			/* move为0时光标不后移*/
			cons->cur_x += 8;
			if (cons->cur_x == 8 + 240)
			{
				cmd_newline(cons);
			}
		}
	}
	return;
}
