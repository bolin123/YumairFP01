#ifndef SYSTIMER_H
#define SYSTIMER_H
#include "Sys.h"

#define SYS_TIMER_MAX_NUM 10

void SysTimerInitialize(void);
void SysTimerPoll(void);

#define SYS_TIMER_REPEAT        1 //�ظ�ִ��
#define SYS_TIMER_IMME          2 //����ִ��
#define SYS_TIMER_NEED_UNSET    4 //��Ҫ�ֶ�unset

typedef void (*SysTimerRun_t)(void *arg);

typedef struct
{
    uint8_t alloced : 1;        //�ѷ���
    uint8_t flag : 7;           //����
    uint16_t span;         //���
    SysTime_t startTime;    //�ϴ�ִ��ʱ��
    SysTimerRun_t run;      //ִ�к���
    void *arg;              //����
}SysTimer_t;

//���ö�ʱִ��
SysTimer_t *SysTimerSet(SysTimerRun_t run, uint32_t span, uint8_t flag, void *arg);

//ȡ����ʱִ��
void SysTimerUnSet(SysTimer_t *timer);

#endif // SYSTIMER_H





