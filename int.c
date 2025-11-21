/*初始化关系 */

#include "bootpack.h"
#include <stdio.h>
#include <string.h>
void init_pic(void)
/* PIC初始化 */
{
	io_out8(PIC0_IMR,  0xff  ); /* 禁止所有中断 */
	io_out8(PIC1_IMR,  0xff  ); /* 禁止所有中断 */

	io_out8(PIC0_ICW1, 0x11  ); /* 边缘触发模式（edge trigger mode） */
	io_out8(PIC0_ICW2, 0x20  ); /* IRQ0-7由INT20-27接收 */
	io_out8(PIC0_ICW3, 1 << 2); /* PIC1由IRQ2相连 */
	io_out8(PIC0_ICW4, 0x01  ); /* 无缓冲区模式 */

	io_out8(PIC1_ICW1, 0x11  ); /* 边缘触发模式（edge trigger mode） */
	io_out8(PIC1_ICW2, 0x28  ); /* IRQ8-15由INT28-2f接收 */
	io_out8(PIC1_ICW3, 2     ); /* PIC1由IRQ2连接 */
	io_out8(PIC1_ICW4, 0x01  ); /* 无缓冲区模式 */

	io_out8(PIC0_IMR,  0xfb  ); /* 11111011 PIC1以外全部禁止 */
	io_out8(PIC1_IMR,  0xff  ); /* 11111111 禁止所有中断 */

	return;
}

void inthandler21(int *esp)
/* 来自PS/2键盘的中断 */
{
	unsigned char data;
	io_out8(PIC0_OCW2, 0x61);	/* 通知PIC IRQ-01 已经受理完毕 */
	data = io_in8(PORT_KEYDAT);
	fifo32_put(&sysfifo, data + 256);
	return;
}

void inthandler2c(int *esp)
/* 来自PS/2鼠标的中断 */
{
	unsigned char data;
	io_out8(PIC1_OCW2, 0x64);	/* 通知PIC1 IRQ-12 已经受理完毕 */
	io_out8(PIC0_OCW2, 0x62);	/* 通知PIC0 IRQ-02 已经受理完毕 */
	data = io_in8(PORT_KEYDAT);
	fifo32_put(&sysfifo, data + 512);
	return;
}

void inthandler27(int *esp)
/* PIC0中断的不完整策略 */
/* 这个中断在Athlon64X2上通过芯片组提供的便利，只需执行一次 */
/* 这个中断只是接收，不执行任何操作 */
/* 为什么不处理？
	→  因为这个中断可能是电气噪声引发的、只是处理一些重要的情况。*/
{
	io_out8(PIC0_OCW2, 0x67); /* 通知PIC的IRQ-07（参考7-1） */
	return;
}

void inthandler20(int *esp)
/* PIT 定时器中断 */
{
	io_out8(PIC0_OCW2, 0x60); /* 通知PIC的IRQ-00（参考7-1） */
	timerctl.count++;
	for(;;){
		if(timerctl.start->time_out <= timerctl.count){
			if(timerctl.start->data == 2)// 任务切换
			{
				timerctl.start->flag = TIMER_FLAGS_ALLOC;
				timerctl.start = timerctl.start->next;
				mtask_switch();
			}else{
				fifo32_put(timerctl.start->fifo, timerctl.start->data);
				timerctl.start->flag = TIMER_FLAGS_ALLOC;
				timerctl.start = timerctl.start->next;
			}
		}else{
			break;
		}
	}
	return;
}

int *inthandler0d(int *esp)// 应用程序异常
{
	struct CONSOLE *cons = (struct CONSOLE *) *((int *)0x0fec);
	struct TASK *task = task_now();
	char s[30];
	cmd_putstr0(cons, "\nINT 0D :\n General Protected Exception.\n");// 应用程序异常结束
	sprintf(s, "EIP = %08X\n", esp[11]);
	cmd_putstr0(cons, s);
	return &(task->tss.esp0);// 强制结束任务
}

int *inthandler0c(int *esp)// 栈异常
{
	struct CONSOLE *cons = (struct CONSOLE *) *((int *) 0x0fec);
	struct TASK *task = task_now();
	char s[30];
	cmd_putstr0(cons, "\nINT 0C :\n Stack Exception.\n");
	sprintf(s, "EIP = %08X\n", esp[11]);
	cmd_putstr0(cons, s);
	return &(task->tss.esp0); /*强制结束程序*/
}
