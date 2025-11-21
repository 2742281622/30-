#include "bootpack.h"

#define PIT_CTRL	0x0043
#define PIT_CNT0	0x0040

struct TIMERCTL timerctl;

void init_pit(void)
{
    int i;
    io_out8(PIT_CTRL, 0x34);
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);
    timerctl.count = 0;
    for (i = 0; i < MAX_TIMERS; i++)
    {
        timerctl.timers[i].flag = 0;// 定时器未分配
    }
    return;
}

void timer_control_init(struct FIFO32 *fifo)
{
    struct TIMER *prev, *timer = &timerctl.timers[0];
    timer->data = 0xff;
    timer->flag = TIMER_FLAGS_USING;
    timer->time_out = 0xffffffff;
    timer->fifo = fifo;
    timer->next = 0;
    if (timerctl.start == 0)
    {
        timerctl.start = timer;
    }else{
        prev = timerctl.start;
        while(prev->next){
            prev = prev->next;
        }
        prev->next = timer;
    }
    return;
}

struct TIMER *timer_alloc(void)
{
    int i;
    for (i = 0; i < MAX_TIMERS; i++)
    {
        if (timerctl.timers[i].flag == 0)
        {
            timerctl.timers[i].flag = TIMER_FLAGS_ALLOC;
            return &timerctl.timers[i];
        }
    }
    return 0;
}

void timer_settime(struct TIMER *timer, struct FIFO32 *fifo, unsigned char data)
{
	timer->fifo = fifo;
	timer->data = data;
    return;
}

void timer_login(struct TIMER *timer, unsigned int timeout)
{
    int e;
    struct TIMER *prev = 0;// 前一个定时器
    struct TIMER *timer1 = timerctl.start;// 当前定时器
    e= io_load_eflags();
    io_cli();
	timer->time_out = timeout + timerctl.count;
    while(timer1)
    {
        if(timer1->time_out > timer->time_out)
        {
            break;
        }
        prev = timer1;
        timer1 = timer1->next;
    }
    if(prev)// 插入到中间
    {
        prev->next = timer;
    }
    else// 插入到头部
    {
        timerctl.start = timer;
    }
    timer->next = timer1;
    timer->flag = TIMER_FLAGS_USING;
    io_store_eflags(e);
    return;
}

void timer_free(struct TIMER *timer)
{
    timer->flag = 0;
    return;
}
