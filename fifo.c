#include "bootpack.h"

void fifo8_init(struct FIFO8 *fifo, int size, unsigned char *buf)
/* 初始化FIFO8 */
{
	fifo->data = buf;
	fifo->p = fifo->q = 0;
	fifo->size = size;
	fifo->free = size;
	fifo->flag = 0;
	return;
}

int fifo8_put(struct FIFO8 *fifo, unsigned char data)
/* 向FIFO8中写入数据 */
{
	if (fifo->free == 0) {
		/* 溢出 */
		fifo->flag |= FLAGS_OVERRUN;
		return -1;
	}
	fifo->data[fifo->q] = data;
	fifo->q++;
	if (fifo->q == fifo->size) {
		fifo->q = 0;
	}
	fifo->free--;
	return 0;
}

int fifo8_get(struct FIFO8 *fifo)
/* 从FIFO8中读取数据 */
{
    int data;
	if (fifo->size == fifo->free) {
		/* 空 */
		return -1;
	}
	data = fifo->data[fifo->p];
	fifo->p++;
	if (fifo->p == fifo->size) {
		fifo->p = 0;
	}
	fifo->free++;
	return data;
}

int fifo8_status(struct FIFO8 *fifo)
/* 获取FIFO8的状态 */
{
	return fifo->size - fifo->free;
}

void fifo32_init(struct FIFO32 *fifo, int size, unsigned int *buf, struct TASK *task)
/* 初始化FIFO32 */
{
	fifo->data = buf;
	fifo->p = fifo->q = 0;
	fifo->size = size;
	fifo->free = size;
	fifo->flag = 0;
	fifo->task = task;
	return;
}

int fifo32_put(struct FIFO32 *fifo, unsigned int data)
/* 向FIFO32中写入数据 */
{
	if (fifo->free == 0) {
		/* 溢出 */
		fifo->flag |= FLAGS_OVERRUN;
		return -1;
	}
	fifo->data[fifo->q] = data;
	fifo->q++;
	if (fifo->q == fifo->size) {
		fifo->q = 0;
	}
	fifo->free--;
	if (fifo->task != 0) {
		if (fifo->task->flags != 2) {
			mtask_run(fifo->task, -1, 0);
		}
	}
	return 0;
}

int fifo32_get(struct FIFO32 *fifo)
/* 从FIFO32中读取数据 */
{
    int data;
	if (fifo->size == fifo->free) {
		/* 空 */
		return -1;
	}
	data = fifo->data[fifo->p];
	fifo->p++;
	if (fifo->p == fifo->size) {
		fifo->p = 0;
	}
	fifo->free++;
	return data;
}

int fifo32_status(struct FIFO32 *fifo)
/* 获取FIFO32的状态 */
{
	return fifo->size - fifo->free;
}
