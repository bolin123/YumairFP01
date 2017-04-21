#include "HalCommon.h"
#include "Sys.h"

static uint32_t g_sysTimeCount = 0;
static uint8_t g_blinkCount = 0;

static void statusLedBlink(void)
{
    static uint8_t blinkCount = 0;
    static SysTime_t lastBlinkTime = 0;

    if(blinkCount < g_blinkCount)
    {
        if(SysTimeHasPast(lastBlinkTime, 200))
        {
            HalGPIOSetLevel(HAL_STATUS_LED_PIN, !HalGPIOGetLevel(HAL_STATUS_LED_PIN));
            lastBlinkTime = SysTime();
            blinkCount++;
        }
    }
    else
    {
        HalGPIOSetLevel(HAL_STATUS_LED_PIN, HAL_STATUS_LED_DISABLE_LEVEL);
        if(SysTimeHasPast(lastBlinkTime, 1500))
        {
            //lastBlinkTime = SysTime();
            blinkCount = 0;
        }
    }
}

static void statusLedInit(void)
{
    HalGPIOConfig(HAL_STATUS_LED_PIN, HAL_IO_OUTPUT);
    HalGPIOSetLevel(HAL_STATUS_LED_PIN, HAL_STATUS_LED_ENABLE_LEVEL);
}

void HalCommonStatusLedSet(uint8_t blink)
{
    g_blinkCount = blink * 2;
}

void HalTimerPast1ms(void)
{
    g_sysTimeCount++;
}

uint32_t HalGetSysTimeCount(void)
{
    return g_sysTimeCount;
}

void HalInterruptSet(bool enable)
{
    if(enable)
    {
        __enable_irq();
    }
    else
    {
        __disable_irq();
    }
}

static void periphClockInit(void)
{
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
}

void HalCommonReboot(void)
{
    __set_FAULTMASK(1); //ÖÕÖ¹ËùÓÐÖÐ¶Ï
    NVIC_SystemReset();
}

void HalCommonInitialize(void)
{
    SystemInit();
    periphClockInit();
    HalGPIOInitialize();
    HalFlashInitialize();
    HalUartInitialize();
    HalTimerInitialize();
    statusLedInit();
}

void HalCommonPoll(void)
{
    HalUartPoll();
    HalGPIOPoll();
    HalTimerPoll();
    HalFlashPoll();
    statusLedBlink();
}

