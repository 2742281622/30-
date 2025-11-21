#include "bootpack.h"

struct TIMER *timer_ts;
struct TASK_CONTROL *taskctl;
extern void task_idle_main(void);
/*
flag
0:未使用 1:已申请未使用 2:活动中
*/

struct TASK *mtask_init(struct MEM_CONTROL *memman, struct FIFO32 *sysfifo){
    int i;
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;
    struct TASK *task_main, *task_idle;
    taskctl = (struct TASK_CONTROL *) memory_alloc_4k(memman, sizeof(struct TASK_CONTROL), 0x00)->addr;
    for (i = 0; i < TASK_MAX; i++){
        taskctl->task0[i].flags = 0;
        taskctl->task0[i].selector = (TASK_GDT0 + i) * 8;
        set_segmdesc(gdt + TASK_GDT0 + i, 103, (int) &taskctl->task0[i].tss, AR_TSS32);
    }
    task_main = mtask_alloc();
    task_main->flags = 2; // 活动中
    task_main->priority = 2;// 最高优先级
    task_main->level = 0;// 最高优先级等级
    task_main->fifo = sysfifo;// 任务队列
    taskctl->now_level = 0;// 当前等级
    taskctl->level_change = 0;// 等级改变标志
    taskctl->level[0].now = 0;// 当前任务队列指针
    taskctl->level[0].running = 0;// 任务队列中任务数
    task_add(task_main);// 添加任务
    task_switchsub();// LEVEL设置
    load_tr(task_main->selector);
    sysfifo->task = task_main;// 任务主进程
    timer_ts = timer_alloc();
    timer_settime(timer_ts, sysfifo, 2);
    timer_login(timer_ts, task_main->priority);

    task_idle = mtask_alloc();// 空闲任务，用于休眠
    task_idle->tss.esp = ((int) memory_alloc_4k(memman, 64 *1024, 0x00)->addr) + 64 * 1024;// 空闲任务栈指针
    task_idle->tss.eip = (int) &task_idle_main;
	task_idle->tss.es = 1 * 8;
	task_idle->tss.cs = 2 * 8;
	task_idle->tss.ss = 1 * 8;
	task_idle->tss.ds = 1 * 8;
	task_idle->tss.fs = 1 * 8;
	task_idle->tss.gs = 1 * 8;
    mtask_run(task_idle, TASK_MAX_LEVELS - 1, 1);// 运行空闲任务
    return task_main;
}

struct TASK *mtask_alloc(void){
    int i;
    struct TASK *task;
    for (i = 0; i < TASK_MAX; i++){
        if(taskctl->task0[i].flags == 0){
            task = &taskctl->task0[i];
            task->flags = 1;   /*已申请未使用*/
			task->tss.eflags = 0x00000202; /* IF = 1; */
			task->tss.eax = 0; /*这里先置为0*/
			task->tss.ecx = 0;
			task->tss.edx = 0;
			task->tss.ebx = 0;
			task->tss.ebp = 0;
			task->tss.esi = 0;
			task->tss.edi = 0;
			task->tss.es = 0;
			task->tss.ds = 0;
			task->tss.fs = 0;
			task->tss.gs = 0;
			task->tss.ldtr = 0;
			task->tss.iomap = 0x40000000;
            task->tss.ss0 = 0;
            return task;
        }
    }
    return 0;
}

void mtask_run(struct TASK *task, int level, int priority){
    if(level < 0){// 等级错误
        level = task->level; /*不改变LEVEL */
    }
    if(priority > 0){
        task->priority = priority;
    }
    if (task->flags == 2 && task->level != level){// 任务已在该等级运行中
        task_remove(task);// 切换子任务，这里执行之后flag的值会变为1，于是下面的if语句块也会被执行
    }
    if(task->flags != 2){// 从休眠状态唤醒的情形
        task->level = level;// 设置任务等级
        task_add(task);// 添加任务
    }
    taskctl->level_change = 1;// 等级改变,下次任务切换时检查LEVEL
    return;
}

void mtask_switch(void){
    struct TASK_LEVEL *tl = &taskctl->level[taskctl->now_level];// 当前等级任务队列
	struct TASK *new_task, *now_task = tl->tasks[tl->now];
	tl->now++;
	if (tl->now == tl->running) {
		tl->now = 0;
	}
	if (taskctl->level_change != 0) {
		task_switchsub();
		tl = &taskctl->level[taskctl->now_level];// 更新当前等级任务队列
	}
	new_task = tl->tasks[tl->now];
	timer_login(timer_ts, new_task->priority);
	if (new_task != now_task) {
		far_jmp(0, new_task->selector);
	}
    return;
}

void mtask_sleep(struct TASK *task){
    struct TASK *now_task;
    if(task->flags == 2){
        // 任务运行中
        now_task = task_now();
        task_remove(task);// 移除任务
        if (task == now_task) {
			/*如果是让自己休眠，则需要进行任务切换*/
			task_switchsub();
			now_task = task_now(); /*在设定后获取当前任务的值*/
			far_jmp(0, now_task->selector);
		}
    }
    return;
}

struct TASK *task_now(void){
    struct TASK_LEVEL *level = &taskctl->level[taskctl->now_level];
    return level->tasks[level->now];
}

void task_add(struct TASK *task){
    struct TASK_LEVEL *level = &taskctl->level[task->level];
    if(level->running < TASK_MAX_LEVEL){
        level->tasks[level->running] = task;
        level->running++;
        task->flags = 2; // 活动中
    }
    return;
}

void task_remove(struct TASK *task){
    struct TASK_LEVEL *level = &taskctl->level[task->level];
    int i;
    for(i = 0; i < level->running; i++){
        if(level->tasks[i] == task){
            break;
        }
    }
    level->running--;
    if(i < level->now){
        level->now--;// 任务被移除，当前任务索引减一
    }
    if(level->now >= level->running){
        level->now = 0;// 切换到最后一个任务
    }
    task->flags = 1; // 休眠中
    for(; i < level->running; i++){
        level->tasks[i] = level->tasks[i+1];
    }
    return;
}

void task_switchsub(void){
        int i;
    for(i = 0; i < TASK_MAX_LEVELS; i++){// 找到最高优先级任务组
        if(taskctl->level[i].running > 0){// 有任务在运行
            break;
        }
    }
    taskctl->now_level = i;// 更新当前任务等级
    taskctl->level_change = 0;// 任务等级改变
    return;
}


void task_idle_main(void)// 空闲休眠任务
{
	for (;;){
		io_hlt();
	}
}

