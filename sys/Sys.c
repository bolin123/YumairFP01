#include "Sys.h"
#include "SysTimer.h"
#include "GPRS.h"
//#include "testGPRS.h"
#include "testProtocol.h"


static char g_version[8] = "1.0.0.2";
static char g_devType[10];
static SysDateTime_t g_sysDateTime;
static SysLocation_t g_location;

//redirect "printf()"
int fputc(int ch, FILE *f)
{
	HalUartWrite(SYS_UART_LOGS_PORT, (const uint8_t *)&ch, 1);
	return ch;
}

void SysUpdateOtaInfo(SysOtaInfo_t *ota)
{
    if(ota != NULL)
    {
        HalFlashErase(HAL_DEVICE_OTA_INFO_ADDR);
        HalFlashWrite(HAL_DEVICE_OTA_INFO_ADDR, ota, sizeof(SysOtaInfo_t));
    }
}

void SysSetLocation(float latitude, float longitude)
{
    g_location.fixed = true;
    g_location.latitude = latitude;
    g_location.longitude = longitude;
}

SysLocation_t *SysGetLocation(void)
{
    if(g_location.fixed)
    {
        return &g_location;
    }
    else
    {
        return NULL;
    }
}

void SysSetReportInterval(uint32_t interval)
{
    SysLog("interval = %d", interval);
    HalFlashErase(HAL_DEVICE_SYS_ARGS_ADDR);
    HalFlashWrite(HAL_DEVICE_SYS_ARGS_ADDR, &interval, sizeof(uint32_t));
}

uint32_t SysGetReportInterval(void)
{
    uint32_t interval;
    HalFlashRead(HAL_DEVICE_SYS_ARGS_ADDR, &interval, sizeof(uint32_t));

    if(interval == 0xffffffff)
    {
        return 60; //default report interval, 60s
    }
    return interval;
}

const char *SysGetDevicePwd(void)
{
    static char password[SYS_PASSWORD_LEN + 1] = "12345678";
    //HalFlashRead(HAL_DEVICE_ID_FLASH_ADDR + SYS_DEVICE_ID_LEN, password, sizeof(password));
    return password;
}

const char *SysGetDeviceID(void)
{
    static char dvid[SYS_DEVICE_ID_LEN + 1] = "YA00100000000202";
    //HalFlashRead(HAL_DEVICE_ID_FLASH_ADDR, dvid, sizeof(dvid));
    return dvid;
}

void SysStatusLedSet(uint8_t blink)
{
    HalCommonStatusLedSet(blink);
}

void SysSetDevType(char *type)
{
    strcpy(g_devType, type);
}

const char *SysGetDevType(void)
{
    return g_devType;
}

const char *SysGetVersion(void)
{
    return g_version;
}

const char *SysGetUUID(void)
{
    return GPRSGetICCID();
}

static bool isLeapYear(uint16_t year)
{
    if(year && year % 4 == 0)
    {
        if(year % 100 == 0)
        {
            if(year % 400 != 0)
            {
                return false;
            }
        }
        return true;
    }

    return false;
}

static void dataMonthUpdate(uint8_t day)
{
    uint8_t monthDays;   
    bool isLeap = isLeapYear(g_sysDateTime.year);
    
    switch(g_sysDateTime.month)
    {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
        monthDays = 31;
        break;

    case 2:
        monthDays = isLeap ? 29 : 28;
        break;
    default:
        monthDays = 30;
    }

    if(day > monthDays)
    {
        g_sysDateTime.month++;
        if(g_sysDateTime.month > 12)
        {
            g_sysDateTime.year++;
        }
    }
}

static void dataTimeUpdate(void)
{
    static SysTime_t lastTime;

    if(SysTime() - lastTime >= 1000)
    {
        g_sysDateTime.second++;
        if(g_sysDateTime.second > 59)
        {
            g_sysDateTime.second = 0;
            g_sysDateTime.minute++;
            if(g_sysDateTime.minute > 59)
            {
                g_sysDateTime.minute = 0;
                g_sysDateTime.hour++;
                if(g_sysDateTime.hour > 23)
                {
                    g_sysDateTime.hour = 0;
                    g_sysDateTime.day++;
                    if(g_sysDateTime.day > 28)
                    {
                        dataMonthUpdate(g_sysDateTime.day);
                    }
                }
            }
        }
        lastTime = SysTime();
    }
    g_sysDateTime.msec = SysTime() - lastTime;
}

void SysSetDataTime(SysDateTime_t *dataTime)
{
    if(dataTime)
    {
        g_sysDateTime = *dataTime;
    }
}

SysDateTime_t *SysGetDateTime(void)
{
    dataTimeUpdate();
    return &g_sysDateTime;
}

const char *SysGetDataTimeString(void)
{
    static char dataTimeStr[18];
    SysDateTime_t *time = &g_sysDateTime;
    
    dataTimeUpdate();
    sprintf(dataTimeStr, "%04d%02d%02d%02d%02d%02d%03d", time->year, time->month, time->day, \
                                                  time->hour, time->minute, time->second, time->msec);

    dataTimeStr[17] = '\0';
    return (const char *)dataTimeStr;
}

void SysReboot(void)
{
    SysLog("");
    HalCommonReboot();
}

void SysInterruptSet(bool enable)
{
    static bool irqEnable = true;

    if(irqEnable != enable)
    {
        HalInterruptSet(enable);
    }
    
    irqEnable = enable;
}

static void sysLogOutputInit(void)
{
    HalUartConfig_t config;
    config.baudrate = 115200;
    config.flowControl = 0;
    config.parity = 0;
    config.wordLength = USART_WordLength_8b;
    config.recvCb = NULL;
    HalUartConfig(HAL_UART_PORT_3, &config);
}

static void printfSysInfo(void)
{
    SysPrintf("=========================================\n");
    SysPrintf("Device Type:%s\n", SysGetDevType());
    SysPrintf("Device ID:%s\n", SysGetDeviceID());
    SysPrintf("Version:%s\n", SysGetVersion());
    SysPrintf("Compile Date:%s %s\n", __DATE__, __TIME__);
    SysPrintf("=========================================\n");
}

void SysInitialize(void)
{
    HalCommonInitialize();
    sysLogOutputInit();
    SysTimerInitialize();
    testProtocolInit();
    printfSysInfo();
}

void SysPoll(void)
{
    HalCommonPoll();
    SysTimerPoll();
    dataTimeUpdate();
    testProtocolPoll();
}

