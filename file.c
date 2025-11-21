#include "bootpack.h"

void file_readfat(int *fat, unsigned char *img){
    // 读取FAT表，解压缩
    int i, j = 0;
    for (i = 0; i < 2280; i+=2){// 每个FAT表项占2字节
        fat[i + 0] = (img[j + 0]     | img[j + 1] << 8) & 0xfff;
        fat[i + 1] = (img[j + 1] >> 4| img[j + 2] << 4) & 0xfff;
        j += 3;
    }
    return;
}

void file_loadfile(struct FILEINFO *fileinfo, int *fat, unsigned char *img, char *buf){
    int clustno = fileinfo->clustno;
    int size = fileinfo->size;
    int i;
    for (;;) {
        if (size <= 512) {
            for (i = 0; i < size; i++) {
                buf[i] = img[clustno * 512 + i];
            }
            break;
        }
        for (i = 0; i < 512; i++) {
            buf[i] = img[clustno * 512 + i];
        }
        size -= 512;
        buf += 512;
        clustno = fat[clustno];
    }
    return;
}
