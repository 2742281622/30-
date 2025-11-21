#include "bootpack.h"

void wait_kbc(void)
{
	for (;;) {
		if ((io_in8(PORT_KEYSTA) & KEYSTA_SEND_NOTREADY) == 0) {
			break;
		}
	}
    return;
}

void init_keyboard(void)
{
	wait_kbc();
	io_out8(PORT_KEYCMD, KEYCMD_WRITE_MODE);
	wait_kbc();
	io_out8(PORT_KEYDAT, KBC_MODE);
    return;
}

void enable_mouse(struct MOUSE_DEC *mdec)
{
    /* 启用鼠标 */
	wait_kbc();
	io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
	wait_kbc();
	io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
	mdec->phase = 0;// 鼠标状态0：等待字节FA
    return;
}

int mouse_decode(struct MOUSE_DEC *mdec, unsigned char dat)
{
	if (mdec->phase == 0) {
		if (dat == 0xfa) {
			mdec->phase = 1;
		}
		return 0;
	}
	if (mdec->phase == 1) {
		if ((dat & 0xc8) == 0x08) {
			mdec->buf[0] = dat;
			mdec->phase = 2;
		}
		return 0;
	}
	if (mdec->phase == 2) {
		mdec->buf[1] = dat;
		mdec->phase = 3;
		return 0;
	}
	if (mdec->phase == 3) {
		mdec->buf[2] = dat;
		mdec->phase = 1;
		mdec->btn = mdec->buf[0] & 0x07;// 取出低三位的鼠标按钮状态
		mdec->x = mdec->buf[1];
		mdec->y = mdec->buf[2];
		if ((mdec->buf[0] & 0x10) != 0) {
			mdec->x |= 0xffffff00;// 将8位以上置为1，用于表示鼠标坐标的符号位
		}
		if ((mdec->buf[0] & 0x20) != 0) {
			mdec->y |= 0xffffff00;
		}
		mdec->y = - mdec->y;// 鼠标坐标原点在左上角
		return 1;
	}
	return -1;
}
