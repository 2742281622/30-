
/* asmhead.nas */
struct BOOTINFO { /* 0x0ff0-0x0fff */
	char cyls; /* 启动区读磁盘读到此为止 */
	char leds; /* 启动时键盘的LED的状态 */
	char vmode; /* 显卡模式为多少位彩色 */
	char reserve;
	short scrnx, scrny; /* 画面分辨率 */
	char *vram;
};
#define ADR_BOOTINFO	0x00000ff0
#define ADR_DISKIMG		0x00100000// 磁盘镜像结构体

// **********************************************************************************
/* naskfunc.nas */
void io_hlt(void);
void io_cli(void);
void io_sti(void);
void io_stihlt(void);
void io_out8(int port, int data);
int io_in8(int port);
int io_load_eflags(void);
void io_store_eflags(int eflags);
void load_gdtr(int limit, int addr);
void load_idtr(int limit, int addr);
int load_cr0(void);
void store_cr0(int cr0);
void load_tr(int tr);
void far_jmp(int eip, int cs);
void far_call(int eip, int cs);
void asm_inthandler21(void);
void asm_inthandler27(void);
void asm_inthandler2c(void);
void asm_inthandler20(void);
void asm_inthandler0c(void);
void asm_inthandler0d(void);
void asm_sys_api(void);
void asm_end_app(void);
void start_app(int eip, int cs, int esp, int ds, int *tss_esp0);

unsigned int memtest_sub(unsigned int start, unsigned int end);

// **********************************************************************************
/* graphic.c */
void init_palette(void);
void set_palette(int start, int end, unsigned char *rgb);
void boxfill8(unsigned char *vram, int xsize, unsigned char c, int x0, int y0, int x1, int y1);
void init_screen8(char *vram, int x, int y);
void putfont8(char *vram, int xsize, int x, int y, char c, char *font);
void putfonts8_asc(char *vram, int xsize, int x, int y, char c, unsigned char *s);
void init_mouse_cursor8(char *mouse, char bc);
void putblock8_8(char *vram, int vxsize, int pxsize,int pysize, int px0, int py0, char *buf, int bxsize);
#define COL8_000000		0		// 黑色
#define COL8_FF0000		1		// 亮红
#define COL8_00FF00		2		// 亮绿
#define COL8_FFFF00		3		// 亮黄
#define COL8_0000FF		4		// 亮蓝
#define COL8_FF00FF		5		// 亮紫
#define COL8_00FFFF		6		// 浅亮蓝
#define COL8_FFFFFF		7		// 白
#define COL8_C6C6C6		8		// 亮灰
#define COL8_840000		9		// 暗红
#define COL8_008400		10		// 暗绿
#define COL8_848400		11		// 暗黄
#define COL8_000084		12		// 暗青
#define COL8_840084		13		// 暗紫
#define COL8_008484		14		// 浅暗蓝
#define COL8_848484		15		// 暗灰

// **********************************************************************************
/* dsctbl.c */
struct SEGMENT_DESCRIPTOR {
	short limit_low, base_low;
	char base_mid, access_right;
	char limit_high, base_high;
};
struct GATE_DESCRIPTOR {
	short offset_low, selector;
	char dw_count, access_right;
	short offset_high;
};
void init_gdtidt(void);
void set_segmdesc(struct SEGMENT_DESCRIPTOR *sd, unsigned int limit, int base, int ar);
void set_gatedesc(struct GATE_DESCRIPTOR *gd, int offset, int selector, int ar);
#define ADR_IDT			0x0026f800
#define LIMIT_IDT		0x000007ff
#define ADR_GDT			0x00270000
#define LIMIT_GDT		0x0000ffff
#define ADR_BOTPAK		0x00280000
#define LIMIT_BOTPAK	0x0007ffff
#define AR_DATA32_RW	0x4092
#define AR_CODE32_ER	0x409a
#define AR_INTGATE32	0x008e

// **********************************************************************************
/* int.c */
void init_pic(void);
void inthandler21(int *esp);// 键盘中断
void inthandler27(int *esp);
void inthandler2c(int *esp);// 鼠标中断
void inthandler20(int *esp);// 时间中断
int *inthandler0c(int *esp);// 栈异常
int *inthandler0d(int *esp);// 应用程序异常
#define PIC0_ICW1		0x0020
#define PIC0_OCW2		0x0020
#define PIC0_IMR		0x0021
#define PIC0_ICW2		0x0021
#define PIC0_ICW3		0x0021
#define PIC0_ICW4		0x0021
#define PIC1_ICW1		0x00a0
#define PIC1_OCW2		0x00a0
#define PIC1_IMR		0x00a1
#define PIC1_ICW2		0x00a1
#define PIC1_ICW3		0x00a1
#define PIC1_ICW4		0x00a1

// **********************************************************************************
/* keyboard.c */
struct MOUSE_DEC {
	unsigned char buf[3], phase;
	int x, y, btn;
};

#define PORT_KEYDAT		0x0060 // PS/2键盘数据端口
#define PORT_KEYSTA		0x0064 // PS/2键盘状态端口
#define PORT_KEYCMD		0x0064 // PS/2键盘命令端口
#define KEYCMD_WRITE_MODE	0x60 // 写模式命令
#define KEYSTA_SEND_NOTREADY	0x02 // 发送缓冲区未准备好
#define KBC_MODE		0x47 // 模式命令

#define KEYCMD_SENDTO_MOUSE	0xd4 // 发送到鼠标命令
#define MOUSECMD_ENABLE		0xf4 // 启用鼠标命令

void wait_kbc(void);
void init_keyboard(void);
void enable_mouse(struct MOUSE_DEC *mdec);
int mouse_decode(struct MOUSE_DEC *mdec, unsigned char dat);

// **********************************************************************************
/* fifo.c */
struct FIFO8 {
	unsigned char *data;
	int p, q, size, free, flag;
};
struct FIFO32 {
	unsigned int *data;
	int p, q, size, free, flag;// p:读取指针  q: 写入指针 size: 缓冲区大小 free: 空闲空间 flag: 状态标志
	struct TASK *task;// 指向任务结构体的指针, 用于任务切换时保存当前任务的信息和任务唤醒
};
#define FLAGS_OVERRUN 0X0001
extern struct FIFO32 sysfifo;

void fifo8_init(struct FIFO8 *fifo, int size, unsigned char *buf);
int fifo8_put(struct FIFO8 *fifo, unsigned char data);
int fifo8_get(struct FIFO8 *fifo);
int fifo8_status(struct FIFO8 *fifo);

void fifo32_init(struct FIFO32 *fifo, int size, unsigned int *buf, struct TASK *task);
int fifo32_put(struct FIFO32 *fifo, unsigned int data);
int fifo32_get(struct FIFO32 *fifo);
int fifo32_status(struct FIFO32 *fifo);


// **********************************************************************************
/* memory.c */
#define MEMMAN_FREES 4090 /* 大约是143.8KB 147256字节*/
#define MEMMAN_ADDR 0x003c0000// 内存管理信息结构体地址 3c0000 - 3E3F38

#define MEMORY_BEGIN 0x00400000
#define MEMORY_END 0xbfffffff

struct MEM_FREE
{
    unsigned int addr;
    unsigned int size;
    unsigned int flag;     // 0:未使用 1:正在使用
    struct MEM_FREE *next; // 指向下一个空闲块
};

struct MEM_USE_INFO
{
    unsigned int addr;
    unsigned int size;
    unsigned int flag;         // 0:未使用 1:已使用
    unsigned int id;           // 进程id
    struct MEM_USE_INFO *next; // 指向下一个已使用块
};

struct MEM_CONTROL
{
    unsigned int total_size;                // 总内存大小
    unsigned int free_size;                 // 空闲内存大小
    struct MEM_FREE frees[MEMMAN_FREES];    // 空闲块数组
    struct MEM_USE_INFO uses[MEMMAN_FREES]; // 已使用块数组
    struct MEM_FREE *free;                  // 指向第一个空闲块
    struct MEM_USE_INFO *use;               // 指向第一个已使用块
};
int memory_init(struct MEM_CONTROL *man);
struct MEM_USE_INFO *memory_alloc(struct MEM_CONTROL *man, unsigned int size, unsigned int id);
int memory_free(struct MEM_CONTROL *man, struct MEM_USE_INFO *use);
unsigned int memtest(unsigned int start, unsigned int end);
struct MEM_USE_INFO *memory_alloc_4k(struct MEM_CONTROL *man, unsigned int size, unsigned int id);

// **********************************************************************************
/* sheet.c */
#define MAX_SHEETS 256
struct SHEET{
	unsigned char *buf;
	int bxsize, bysize, vx0, vy0, col_inv, height, flags;
	struct SHEET_CONTROL *shtctl;
	struct TASK *task;
};
struct SHEET_CONTROL{
	unsigned char *vram, *map;
	int xsize, ysize, top;
	struct SHEET *sheets[MAX_SHEETS];
    struct SHEET sheet[MAX_SHEETS];
};
struct SHEET_CONTROL *shtctl_init(struct MEM_CONTROL *memman, char *vram, int xsize, int ysize);
struct SHEET *sheet_alloc(struct SHEET_CONTROL *shtctl);
void sheet_setbuf(struct SHEET *sht, unsigned char *buf, int xsize, int ysize, int col_inv);
void sheet_updown(struct SHEET *sht, int height);
void sheet_refresh(struct SHEET *sht, int bx0, int by0, int bx1, int by1);
void sheet_slide(struct SHEET *sht, int vx0, int vy0);
void sheet_free(struct SHEET *sht);
void sheet_refreshsub(struct SHEET_CONTROL *shtctl, int x0, int y0, int x1, int y1, int h0, int h1);
void sheet_refreshmap(struct SHEET_CONTROL *shtctl, int x0, int y0, int x1, int y1, int h0);

// **********************************************************************************
/* window.c */
void make_window8(unsigned char *buf, int xsize, int ysize, char *title, char act);
void putfonts8_asc_sht(struct SHEET *sht, int x, int y, int c, int b, char *s, int l);
void make_textbox8(struct SHEET *sht, int x0, int y0, int sx, int sy, int c);
void show_time(struct SHEET *sht, struct BOOTINFO *binfo, int c, int b, int h, int m, int s);
void make_wtitle8(unsigned char *buf, int xsize, char *title, char act);
void change_wtitle8(struct SHEET *sht, char act);
int keywin_off(struct SHEET *key_win, struct SHEET *sht_win, int cur_c, int cur_x);
int keywin_on(struct SHEET *key_win, struct SHEET *sht_win, int cur_c);

// **********************************************************************************
/* timer.c */
#define MAX_TIMERS 500
#define TIMER_FLAGS_ALLOC 0x0001// 定时器配置状态
#define TIMER_FLAGS_USING 0x0002// 定时器正在使用
struct TIMER{
	unsigned int time_out, flag;
	struct FIFO32 *fifo;
	unsigned char data;
	struct TIMER *next;
};
struct TIMERCTL{
	unsigned int count;
	struct TIMER timers[MAX_TIMERS];
	struct TIMER *start;
};
extern struct TIMERCTL timerctl;

void init_pit(void);
void timer_control_init(struct FIFO32 *fifo);
struct TIMER *timer_alloc(void);
void timer_login(struct TIMER *timer, unsigned int timeout);
void timer_free(struct TIMER *timer);
void timer_settime(struct TIMER *timer, struct FIFO32 *fifo, unsigned char data);

// **********************************************************************************
/* mtask.c */
#define AR_TSS32		0x0089
#define TASK_MAX 1000
#define TASK_GDT0 3
#define TASK_MAX_LEVEL 100// 同级任务最大数
#define TASK_MAX_LEVELS 10// 任务最大等级数
extern struct TIMER *timers_ts;
extern struct TASK_CONTROL *taskctl;
struct TSS32 {
	int backlink, esp0, ss0, esp1, ss1, esp2, ss2, cr3;
	int eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
	int es, cs, ss, ds, fs, gs;
	int ldtr, iomap;
};
struct TASK{// 任务结构体
    int selector, flags;
	int priority, level;// 优先级
	struct FIFO32 *fifo;// 任务队列
    struct TSS32 tss;
};
struct TASK_LEVEL{
	int running;// 正在运行的任务数
	int now;// 当前正在运行的任务
	struct TASK *tasks[TASK_MAX_LEVEL];// 任务数组
};
struct TASK_CONTROL{// 任务控制结构体
    int now_level;// 当前正在运行的任务等级
	char level_change;// 任务等级是否改变
    struct TASK_LEVEL level[TASK_MAX_LEVELS];// 任务等级数组
    struct TASK task0[TASK_MAX];// 任务数组
};
struct TASK *mtask_init(struct MEM_CONTROL *memman, struct FIFO32 *sysfifo);// 初始化任务管理
void mtask_switch(void);
struct TASK *mtask_alloc(void);
void mtask_run(struct TASK *task, int level, int priority);// 任务运行
void mtask_sleep(struct TASK *task);// 任务睡眠
struct TASK *task_now(void);// 获取当前任务
void task_add(struct TASK *task);// 添加任务
void task_remove(struct TASK *task);// 移除任务
void task_switchsub(void);
void task_idle_main(void);// 空闲休眠任务

// **********************************************************************************
/* file.c */
struct FILEINFO{// 文件信息结构体32字节
	unsigned char name[8], ext[3], type;
	char reserve[10];// 保留字段
	unsigned short time, date, clustno;
	unsigned int size;
};
void file_readfat(int *fat, unsigned char *img);// 读取FAT表，解压缩
void file_loadfile(struct FILEINFO *fileinfo, int *fat, unsigned char *img, char *buf);// 加载文件

// **********************************************************************************
/* command.c */
struct CONSOLE {
	struct SHEET *sht;
	struct TIMER *timer;
	int cur_x, cur_y, cur_c;
};
void task_command_main(struct SHEET *sht_command);// 命令行窗口主函数
void cmd_newline(struct CONSOLE *cons);// 换行
void cmd_runcode(struct CONSOLE *cons, struct FILEINFO *fileinfo, struct MEM_CONTROL *memctl, struct SEGMENT_DESCRIPTOR *gdt, int *fat, char *cmdline);// 运行代码
void cmd_mem(struct CONSOLE *cons, struct MEM_CONTROL *memctl);// 显示内存信息
void cmd_clear(struct CONSOLE *cons);// 清除屏幕
void cmd_ls(struct CONSOLE *cons, struct FILEINFO *fileinfo);// 显示目录
void cmd_cat(struct CONSOLE *cons, struct FILEINFO *fileinfo, struct MEM_CONTROL *memctl, int *fat, char *cmdline);// 显示文件内容
void cmd_app(struct CONSOLE *cons, struct FILEINFO *fileinfo, struct MEM_CONTROL *memctl, struct SEGMENT_DESCRIPTOR *gdt, int *fat, char *cmdline);// 运行应用程序
void cmd_putchar(struct CONSOLE *cons, int chr, char move);// 输出字符

// **********************************************************************************
/* api.c */
int *sys_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax);// 系统调用
void cmd_putstr0(struct CONSOLE *cons, char *s);// 输出字符串
void cmd_putstr1(struct CONSOLE *cons, char *s, int l);// 输出字符串
