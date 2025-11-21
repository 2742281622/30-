#include "bootpack.h"

#define SHEET_USE 1

struct SHEET_CONTROL *shtctl_init(struct MEM_CONTROL *memman, char *vram, int xsize, int ysize)
{
    int i;
	struct MEM_USE_INFO *mem_ctl, *mem_map;
	mem_ctl = memory_alloc_4k(memman, sizeof(struct SHEET_CONTROL), 0x00);// 不回收内存
	mem_map = memory_alloc_4k(memman, (xsize * ysize), 0x00);// 不回收内存
    if (mem_ctl == 0 || mem_map == 0){return 0;}
	struct SHEET_CONTROL *shtctl = (struct SHEET_CONTROL *)mem_ctl->addr;
	shtctl->map = (unsigned char *)mem_map->addr;
	shtctl->vram = vram;
	shtctl->xsize = xsize;
	shtctl->ysize = ysize;
	shtctl->top = -1;
    for (i = 0; i < MAX_SHEETS; i++){
        shtctl->sheet[i].flags = 0;// 初始化所有sheet为0 表示未使用
		shtctl->sheet[i].shtctl = shtctl;// 记录sheet所属的sheetctl
    }
	for (i = 0; i < xsize * ysize; i++){
        shtctl->map[i] = 0xff;
    }
	return shtctl;
}

struct SHEET *sheet_alloc(struct SHEET_CONTROL *shtctl)
{
    int i;
	struct SHEET *sht;
    for (i = 0; i < MAX_SHEETS; i++){
        if (shtctl->sheet[i].flags == 0){
			sht = &shtctl->sheet[i];
            sht->flags = SHEET_USE;// 表示已使用
			sht->height = -1;// 高度为-1表示隐藏
			sht->task = 0;// 未关联任务
            return sht;
        }
    }
    return 0;// 未找到可用sheet
}

void sheet_setbuf(struct SHEET *sht, unsigned char *buf, int xsize, int ysize, int col_inv)
{
	sht->buf = buf;// 图像缓冲区地址
	sht->bxsize = xsize;// 图像缓冲区宽度
	sht->bysize = ysize;// 图像缓冲区大小
	sht->col_inv = col_inv;// 透明色
	return;
}

void sheet_updown(struct SHEET *sht, int height)
{
	struct SHEET_CONTROL *shtctl = sht->shtctl;// 获取sheet所属的sheetctl
	int h, old= sht->height;//保存旧高度

	// 修正高度
	if(height > shtctl->top+1){height = shtctl->top+1;}
	if(height < -1){height = -1;}
	// 设定新高度
	sht->height = height;

	// 对sheets重新排列
	if (old > height){
		if (height >= 0){// 新高度大于等于0 表示显示sheet
			// 旧高度大于新高度 需将旧高度以下的sheet向上移动
			for (h = old; h > height; h--){
				shtctl->sheets[h] = shtctl->sheets[h-1];
				shtctl->sheets[h]->height = h;// 更新sheet高度
			}
			shtctl->sheets[height] = sht;// 将新sheet放在新高度位置
			sheet_refreshmap(shtctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height + 1);
			sheet_refreshsub(shtctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height + 1, old);
		}else{// 新高度为-1 表示隐藏sheet
			// 旧高度大于新高度 需将旧高度以上的sheet向下移动
			for (h = old; h < shtctl->top; h++){
				shtctl->sheets[h] = shtctl->sheets[h+1];
				shtctl->sheets[h]->height = h;// 更新sheet高度
			}
			shtctl->top--;// 更新top
			// 刷新显示
			sheet_refreshmap(shtctl, sht->vx0, sht->vy0, sht->vx0+sht->bxsize, sht->vy0+sht->bysize,0);// 刷新map
			sheet_refreshsub(shtctl, sht->vx0, sht->vy0, sht->vx0+sht->bxsize, sht->vy0+sht->bysize, 0, old - 1);// 刷新显示
		}
	}else if(old < height){
		if (old >= 0){// 旧高度大于等于0 表示显示sheet
			// 新高度大于旧高度 需将新高度以下的sheet向上移动
			for (h = old; h < height; h++){
				shtctl->sheets[h] = shtctl->sheets[h+1];
				shtctl->sheets[h]->height = h;// 更新sheet高度
			}
			shtctl->sheets[height] = sht;// 将新sheet放在新高度位置
		}else{// 旧高度为-1 表示由隐藏转为显示sheet
			// 新高度大于旧高度 需将新高度以上的sheet向上移动
			for (h = shtctl->top; h >= height; h--){
				shtctl->sheets[h+1] = shtctl->sheets[h];
				shtctl->sheets[h+1]->height = h+1;// 更新sheet高度
			}
			shtctl->sheets[height] = sht;// 将新sheet放在新高度位置
			shtctl->top++;// 更新top
		}
		// 刷新显示
		sheet_refreshmap(shtctl, sht->vx0, sht->vy0, sht->vx0+sht->bxsize, sht->vy0+sht->bysize, height);// 刷新map
		sheet_refreshsub(shtctl, sht->vx0, sht->vy0, sht->vx0+sht->bxsize, sht->vy0+sht->bysize, height, height);// 刷新显示
	}
	return;
}

void sheet_refresh(struct SHEET *sht, int bx0, int by0, int bx1, int by1)
{
	struct SHEET_CONTROL *shtctl = sht->shtctl;// 获取sheet所属的sheetctl
    if (sht->height >= 0){
        sheet_refreshsub(shtctl, sht->vx0+bx0, sht->vy0+by0, sht->vx0+bx1, sht->vy0+by1, sht->height, sht->height);// 刷新显示
    }
    return;
}

void sheet_slide(struct SHEET *sht, int vx0, int vy0)
{
	struct SHEET_CONTROL *shtctl = sht->shtctl;// 获取sheet所属的sheetctl
	int oldvx = sht->vx0, oldvy = sht->vy0;// 保存旧坐标
    sht->vx0 = vx0;// 更新sheet的x坐标
    sht->vy0 = vy0;// 更新sheet的y坐标
	if (sht->height >= 0){// 如果sheet显示
		sheet_refreshmap(shtctl, oldvx, oldvy, oldvx+sht->bxsize, oldvy+sht->bysize, 0);// 刷新map
		sheet_refreshsub(shtctl, oldvx, oldvy, oldvx+sht->bxsize, oldvy+sht->bysize, 0, sht->height - 1);// 刷新显示
		sheet_refreshmap(shtctl, vx0, vy0, vx0+sht->bxsize, vy0+sht->bysize, sht->height);// 刷新map
		sheet_refreshsub(shtctl, vx0, vy0, vx0+sht->bxsize, vy0+sht->bysize, sht->height, shtctl->top);// 刷新显示
	}
	return;
}

void sheet_free(struct SHEET *sht)
{
    if (sht->height >= 0){// 如果sheet显示
        sheet_updown(sht, -1);// 隐藏sheet
    }
    sht->flags = 0;// 表示未使用
    return;
}

void sheet_refreshsub(struct SHEET_CONTROL *shtctl, int x0, int y0, int x1, int y1, int h0, int h1)
{
	int h, bx, by, vx, vy, bx0, by0, bx1, by1;
	unsigned char *buf, c, *vram = shtctl->vram, *map = shtctl->map, sid;
	struct SHEET *sht;// 当前sheet
	if (x0 < 0){x0 = 0;}// 修正x坐标
	if (y0 < 0){y0 = 0;}// 修正y坐标
	if (x1 > shtctl->xsize){x1 = shtctl->xsize;}// 修正x坐标
	if (y1 > shtctl->ysize){y1 = shtctl->ysize;}// 修正y坐标
	if (h0 < 0){h0 = 0;}// 修正高度
	if (h1 > shtctl->top){h1 = shtctl->top;}// 修正高度
	
	// 重新构建map数组，从高到低优先级
	for (h = h1; h >= h0; h--) {
		sht = shtctl->sheets[h];// 获取当前sheet
		buf = sht->buf;// 获取当前sheet的图像缓冲区
		sid = sht - shtctl->sheet;// 计算sheet的索引

		bx0 = x0 - sht->vx0;// 计算sheet中的x坐标
		by0 = y0 - sht->vy0;// 计算sheet中的y坐标
		bx1 = x1 - sht->vx0;// 计算sheet中的x坐标
		by1 = y1 - sht->vy0;// 计算sheet中的y坐标
		if (bx0 < 0){bx0 = 0;}// 修正x坐标
		if (by0 < 0){by0 = 0;}// 修正y坐标
		if (bx1 > sht->bxsize){bx1 = sht->bxsize;}// 修正x坐标
		if (by1 > sht->bysize){by1 = sht->bysize;}// 修正y坐标

		for (by = by0; by < by1; by++) {
			vy = sht->vy0 + by;// 计算显存中的y坐标
			for (bx = bx0; bx < bx1; bx++) {
				vx = sht->vx0 + bx;// 计算显存中的x坐标
				// 如果不是透明色且map中当前位置还没有被设置
				if (buf[by * sht->bxsize + bx] != sht->col_inv && map[vy * shtctl->xsize + vx] == 0xff) {
					map[vy * shtctl->xsize + vx] = sid;
				}
			}
		}
	}
	
	// 最后根据map数组绘制像素
	for (h = h0; h <= h1; h++) {
		sht = shtctl->sheets[h];// 获取当前sheet
		buf = sht->buf;// 获取当前sheet的图像缓冲区
		sid = sht - shtctl->sheet;// 计算sheet的索引

		bx0 = x0 - sht->vx0;// 计算sheet中的x坐标
		by0 = y0 - sht->vy0;// 计算sheet中的y坐标
		bx1 = x1 - sht->vx0;// 计算sheet中的x坐标
		by1 = y1 - sht->vy0;// 计算sheet中的y坐标
		if (bx0 < 0){bx0 = 0;}// 修正x坐标
		if (by0 < 0){by0 = 0;}// 修正y坐标
		if (bx1 > sht->bxsize){bx1 = sht->bxsize;}// 修正x坐标
		if (by1 > sht->bysize){by1 = sht->bysize;}// 修正y坐标

		for (by = by0; by < by1; by++) {
			vy = sht->vy0 + by;// 计算显存中的y坐标
			for (bx = bx0; bx < bx1; bx++) {
				vx = sht->vx0 + bx;// 计算显存中的x坐标
				// 如果当前位置属于这个sheet
				if (map[vy * shtctl->xsize + vx] == sid) {
					c = buf[by * sht->bxsize + bx];// 获取当前像素
					vram[vy * shtctl->xsize + vx] = c;// 绘制像素
				}
			}
		}
	}
    return;
}

void sheet_refreshmap(struct SHEET_CONTROL *shtctl, int x0, int y0, int x1, int y1, int h0)
{
	int h, bx, by, vx, vy, bx0, by0, bx1, by1;
	unsigned char *buf, sid, *map = shtctl->map;
	struct SHEET *sht;// 当前sheet
	
	if (x0 < 0) { x0 = 0; }
	if (y0 < 0) { y0 = 0; }
	if (x1 > shtctl->xsize) { x1 = shtctl->xsize; }
	if (y1 > shtctl->ysize) { y1 = shtctl->ysize; }

	for (h = h0; h <= shtctl->top; h++){
		sht = shtctl->sheets[h];// 获取当前sheet
		sid = sht - shtctl->sheet;// 计算sheet的索引
		buf = sht->buf;// 获取当前sheet的图像缓冲区
		bx0 = x0 - sht->vx0;// 计算sheet中的x坐标
		by0 = y0 - sht->vy0;// 计算sheet中的y坐标
		bx1 = x1 - sht->vx0;// 计算sheet中的x坐标
		by1 = y1 - sht->vy0;// 计算sheet中的y坐标
		if (bx0 < 0){bx0 = 0;}// 修正x坐标
		if (by0 < 0){by0 = 0;}// 修正y坐标
		if (bx1 > sht->bxsize){bx1 = sht->bxsize;}// 修正x坐标
		if (by1 > sht->bysize){by1 = sht->bysize;}// 修正y坐标

		for (by =by0; by < by1; by++){
			vy = sht->vy0 + by;// 计算显存中的y坐标
			for (bx =bx0; bx < bx1; bx++){
				vx = sht->vx0 + bx;// 计算显存中的x坐标
				if (buf[by*sht->bxsize+bx] != sht->col_inv){// 如果不是透明色
					map[vy*shtctl->xsize+vx] = sid;// 绘制像素
				}
			}
		}
	}
}
