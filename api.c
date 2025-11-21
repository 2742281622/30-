#include "bootpack.h"

struct FREEINFO { /* 可用信息 */
	unsigned int addr, size;
};
struct MEMMAN { /* 内存管理 */
	int frees, maxfrees, lostsize, losts;
	struct FREEINFO free[MEMMAN_FREES];
};

void malloc_init(struct MEMMAN *man);
int malloc_free(struct MEMMAN *man, unsigned int addr, unsigned int size);
unsigned int malloc_alloc(struct MEMMAN *man, unsigned int size);
void sys_api_linewin(struct SHEET *sht, int x0, int y0, int x1, int y1, int col);



int *sys_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax)
{
    int data_base = *((int *)0x0fe8); // 获取应用程序的数据段开始地址
    struct TASK *task = task_now();
    struct CONSOLE *cons = (struct CONSOLE *)*((int *)0x0fec);
    struct SHEET_CONTROL *shtctl = (struct SHEET_CONTROL *)*((int *)0x0fe4);
    struct SHEET *sht;
    struct TIMER *timer;
    int i;
    int *reg = &eax + 1; /* eax后面的地址*/
    /*强行改写通过PUSHAD保存的值*/
	/* reg[0] : EDI, reg[1] : ESI, reg[2] : EBP, reg[3] : ESP */
	/* reg[4] : EBX, reg[5] : EDX, reg[6] : ECX, reg[7] : EAX */
    if (edx == 1)// 输出字符
    {
        cmd_putchar(cons, eax & 0xff, 1);
    }
    else if (edx == 2)// 输出字符串
    {
        cmd_putstr0(cons, (char *)ebx + data_base);
    }
    else if (edx == 3)// 输出字符串指定长度
    {
        cmd_putstr1(cons, (char *)ebx + data_base, ecx);
    }
    else if (edx == 4)// 结束程序
    {
        return &(task->tss.esp0);
    }
    else if (edx == 5)// 创建窗口
    {
        /*
        调用：
        EBX：窗口结构体地址
        ESI：窗口宽度
        EDI：窗口高度
        EAX：透明色
        ECX：窗口标题字符串地址

        返回：
        EAX：窗口结构体地址
        */
        sht = sheet_alloc(shtctl);
        sht->task = task;
        sht->flags |= 0x10;// 应用程序窗口
        sheet_setbuf(sht, (char *)ebx + data_base, esi, edi, eax);
        make_window8((char *)ebx + data_base, esi, edi, (char *) ecx + data_base, 1);
        sheet_slide(sht, shtctl->xsize / 2 - esi / 2, shtctl->ysize / 2 - edi / 2);
        sheet_updown(sht, shtctl->top);
        reg[7] = (int)sht;
    }
    else if (edx == 6)// 窗口绘制字符串
    {
        /*
        调用：
        EBX：窗口结构体地址，低地址位为0表示需要刷新窗口
        ESI：窗口绘制字符串的X坐标
        EDI：窗口绘制字符串的Y坐标
        EAX：字符颜色
        ECX：窗口绘制字符串的长度
        EBP：窗口绘制字符串的地址

        返回：
        无
        */
        sht = (struct SHEET *)(ebx & 0xfffffffe);
        putfonts8_asc(sht->buf, sht->bxsize, esi, edi, eax, (char *)ebp + data_base);
        if ((ebx & 1) == 0) {
			sheet_refresh(sht, esi, edi, esi + ecx * 8, edi + 16);
		}
    }
    else if (edx == 7)// 窗口绘制矩形
    {
        /*
        调用：
        EBX：窗口结构体地址，低地址位为0表示需要刷新窗口
        EAX：x0
        ECX：y0
        ESI：x1
        EDI：y1
        EBP：矩形颜色

        返回：
        无
        */
        sht = (struct SHEET *)(ebx & 0xfffffffe);
        boxfill8(sht->buf, sht->bxsize, ebp, eax, ecx, esi, edi);
		if ((ebx & 1) == 0) {
			sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
		}
    }
    else if (edx == 8)// 初始化内存管理
    {
        /*
        调用：
        EBX：内存管理结构体地址
        EAX：初始可用内存地址
        ECX：初始可用内存大小

        返回：
        无
        */
        malloc_init((struct MEMMAN *) (ebx + data_base));
        ecx &= 0xfffffff0; /*以16字节为单位*/
        malloc_free((struct MEMMAN *) (ebx + data_base), eax, ecx);
    }
    else if (edx == 9)// 申请内存
    {
        /*
        调用：
        EBX：内存管理结构体地址
        ECX：内存大小

        返回：
        EAX：内存地址
        */
		ecx = (ecx + 0x0f) & 0xfffffff0; /*以16字节为单位进位取整*/
		reg[7] = malloc_alloc((struct MEMMAN *) (ebx + data_base), ecx);
	}
    else if (edx == 10)// 释放内存
    {
        /*
        调用：
        EBX：内存管理结构体地址
        EAX：内存地址
        ECX：内存大小

        返回：
        无
        */
		ecx = (ecx + 0x0f) & 0xfffffff0; /*以16字节为单位进位取整*/
		malloc_free((struct MEMMAN *) (ebx + data_base), eax, ecx);
	}
    else if (edx == 11)// 画点
    {
        /*
        调用：
        EBX：窗口结构体地址，低地址位为0表示需要刷新窗口
        ESI：X坐标
        EDI：Y坐标
        EAX：点颜色

        返回：
        无
        */
        sht = (struct SHEET *)(ebx & 0xfffffffe);
        sht->buf[edi * sht->bxsize + esi] = eax;
        if ((ebx & 1) == 0) {
			sheet_refresh(sht, esi, edi, esi + 1, edi + 1);
		}
    }
    else if (edx == 12)// 窗口刷新
    {
        /*
        调用：
        EBX：窗口结构体地址
        EAX：刷新区域的X坐标
        ECX：刷新区域的Y坐标
        ESI：刷新区域的宽度
        EDI：刷新区域的高度

        返回：
        无
        */
        sht = (struct SHEET *)ebx;
        sheet_refresh(sht, eax, ecx, esi, edi);
    }
    else if (edx == 13)// 窗口画线
    {
        /*
        调用：
        EBX：窗口结构体地址，低地址位为0表示需要刷新窗口
        EAX：x0
        ECX：y0
        ESI：x1
        EDI：y1
        EBP：线颜色

        返回：
        无
        */
        sht = (struct SHEET *) (ebx & 0xfffffffe);
		sys_api_linewin(sht, eax, ecx, esi, edi, ebp);
		if ((ebx & 1) == 0) {
			sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
		}
    }
    else if (edx == 14)// 关闭窗口
    {
        /*
        调用：
        EBX：窗口结构体地址

        返回：
        无
        */
        sht = (struct SHEET *)ebx;
        sheet_free(sht);
    }
    else if (edx == 15)// 键盘输入
    {
        /*
        调用：
        EAX：0表示不休眠等待键盘输入，1表示休眠等待键盘输入

        返回：
        EAX：键盘输入的ASCII码值
        */
        for(;;)
        {
            io_cli();
            if (fifo32_status(task->fifo) == 0)
            {
                if (eax != 0)// 休眠等待键盘输入
                {
                    mtask_sleep(task);
                }else{
                    io_sti();
                    reg[7] = -1;// 无键盘输入
                    return 0;
                }
            }
            i = fifo32_get(task->fifo);
            io_sti();
            if (i <= 1) { /*光标用定时器*/
				/*应用程序运行时不需要显示光标，因此总是将下次显示用的值置为1*/
				timer_settime(cons->timer, task->fifo, 1);
                timer_login(cons->timer, 50);
			}
			if (i == 2) { /*光标ON */
				cons->cur_c = COL8_FFFFFF;
			}
			if (i == 3) { /*光标OFF */
				cons->cur_c = -1;
			}
			if (i >= 256) { /*键盘数据和定时器数据*/
				reg[7] = i - 256;
				return 0;
			}
        }
    }
    else if (edx == 16)// 获取定时器
    {
        reg[7] = (int) timer_alloc();
    }
    else if (edx == 17)// 设置定时器
    {
        timer_settime((struct TIMER *)ebx, task->fifo, eax + 256);
    }
    else if (edx == 18)// 运行定时器
    {
        timer_login((struct TIMER *)ebx, eax);
    }
    else if (edx == 19)// 注销定时器
    {
        timer_free((struct TIMER *)ebx);
    }
    return 0;
}

void cmd_putstr0(struct CONSOLE *cons, char *s)
{
    for (; *s != 0; s++)
    {
        cmd_putchar(cons, *s, 1);
    }
    return;
}

void cmd_putstr1(struct CONSOLE *cons, char *s, int l)
{
    int i;
    for (i = 0; i < l; i++)
    {
        cmd_putchar(cons, s[i], 1);
    }
    return;
}

void malloc_init(struct MEMMAN *man)
{
    man->frees = 0;    /* 可用信息数目 */
	man->maxfrees = 0; /* 用于观察可用状况：frees的最大值 */
	man->lostsize = 0; /* 释放失败的内存的大小总和 */
	man->losts = 0;    /* 释放失败次数 */
	return;
}

int malloc_free(struct MEMMAN *man, unsigned int addr, unsigned int size)
/* 释放 */
{
	int i, j;
	/* 为便于归纳内存，将free[]按照addr的顺序排列 */
	/* 所以，先决定应该放在哪里 */
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].addr > addr) {
			break;
		}
	}
	/* free[i - 1].addr < addr < free[i].addr */
	if (i > 0) {
		/* 前面有可用内存 */
		if (man->free[i - 1].addr + man->free[i - 1].size == addr) {
			/* 可以与前面的可用内存归纳到一起 */
			man->free[i - 1].size += size;
			if (i < man->frees) {
				/* 后面也有 */
				if (addr + size == man->free[i].addr) {
					/* 也可以与后面的可用内存归纳到一起 */
					man->free[i - 1].size += man->free[i].size;
					/* man->free[i]删除 */
					/* free[i]变成0后归纳到前面去 */
					man->frees--;
					for (; i < man->frees; i++) {
						man->free[i] = man->free[i + 1]; /* 结构体赋值 */
					}
				}
			}
			return 0; /* 成功完成 */
		}
	}
	/* 不能与前面的可用空间归纳到一起 */
	if (i < man->frees) {
		/* 后面还有 */
		if (addr + size == man->free[i].addr) {
			/* 可以与后面的内容归纳到一起 */
			man->free[i].addr = addr;
			man->free[i].size += size;
			return 0; /* 成功完成 */
		}
	}
	/* 既不能与前面归纳到一起，也不能与后面归纳到一起 */
	if (man->frees < MEMMAN_FREES) {
		/* free[i]之后的，向后移动，腾出一点可用空间 */
		for (j = man->frees; j > i; j--) {
			man->free[j] = man->free[j - 1];
		}
		man->frees++;
		if (man->maxfrees < man->frees) {
			man->maxfrees = man->frees; /* 更新最大值 */
		}
		man->free[i].addr = addr;
		man->free[i].size = size;
		return 0; /* 成功完成 */
	}
	/* 不能往后移动 */
	man->losts++;
	man->lostsize += size;
	return -1; /* 失败 */
}

unsigned int malloc_alloc(struct MEMMAN *man, unsigned int size)
/* 分配 */
{
	unsigned int i, a;
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].size >= size) {
			/* 找到了足够大的内存 */
			a = man->free[i].addr;
			man->free[i].addr += size;
			man->free[i].size -= size;
			if (man->free[i].size == 0) {
				/* 如果free[i]变成了0，就减掉一条可用信息 */
				man->frees--;
				for (; i < man->frees; i++) {
					man->free[i] = man->free[i + 1]; /* 代入结构体 */
				}
			}
			return a;
		}
	}
	return 0; /* 没有可用空间 */
}

void sys_api_linewin(struct SHEET *sht, int x0, int y0, int x1, int y1, int col)
{
	int i, x, y, len, dx, dy;

	dx = x1 - x0;
	dy = y1 - y0;
	x = x0 << 10;
	y = y0 << 10;
	if (dx < 0) {
		dx = - dx;
	}
	if (dy < 0) {
		dy = - dy;
	}
	if (dx >= dy) {
		len = dx + 1;
		if (x0 > x1) {
			dx = -1024;
		} else {
			dx =  1024;
		}
		if (y0 <= y1) {
			dy = ((y1 - y0 + 1) << 10) / len;
		} else {
			dy = ((y1 - y0 - 1) << 10) / len;
		}
	} else {
		len = dy + 1;
		if (y0 > y1) {
			dy = -1024;
		} else {
			dy =  1024;
		}
		if (x0 <= x1) {
			dx = ((x1 - x0 + 1) << 10) / len;
		} else {
			dx = ((x1 - x0 - 1) << 10) / len;
		}
	}

	for (i = 0; i < len; i++) {
		sht->buf[(y >> 10) * sht->bxsize + (x >> 10)] = col;
		x += dx;
		y += dy;
	}

	return;
}
