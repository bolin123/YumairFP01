#ifndef SYS_H
#define SYS_H

#include "HalCommon.h"

#define SYS_GPRS_COMM_PORT HAL_UART_PORT_1
#define SYS_UART_LOGS_PORT HAL_UART_PORT_3

#define SYS_DEVICE_ID_LEN 16
#define SYS_PASSWORD_LEN   8

#define SYS_OTA_DONE_FLAG 0xa3 //升级完成标志位

#define DEBUG_ENABLE 1
#define SysTime_t uint32_t
#define SysTime HalGetSysTimeCount
#define SysTimeHasPast(oldtime, pass) ((SysTime() - (oldtime)) > pass)

#if DEBUG_ENABLE

#define HalPrintf(...) printf(__VA_ARGS__)
#define SysPrintf HalPrintf
#define SysLog(...) SysPrintf("%s %s: ", SysGetDataTimeString(), __FUNCTION__); SysPrintf(__VA_ARGS__); SysPrintf("\n");
#else
#define HalPrintf(...)
#define SysPrintf HalPrintf
#define SysLog(...)
#endif

typedef struct
{
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint16_t msec;
}SysDateTime_t;

typedef struct
{
    bool fixed;
    float latitude;
    float longitude;
}SysLocation_t;

typedef struct
{
    uint8_t flag;
    uint32_t size;
    uint8_t md5[16];
}SysOtaInfo_t;

void SysUpdateOtaInfo(SysOtaInfo_t *ota);
void SysSetLocation(float latitude, float longitude);
SysLocation_t *SysGetLocation(void);

uint32_t SysGetReportInterval(void);
void SysSetReportInterval(uint32_t interval);
void SysStatusLedSet(uint8_t blink);
void SysSetDevType(char *type);
const char *SysGetDevType(void);
const char *SysGetDevicePwd(void);
const char *SysGetDeviceID(void);
const char *SysGetVersion(void);
const char *SysGetUUID(void);
void SysSetDataTime(SysDateTime_t *dataTime);
SysDateTime_t *SysGetDateTime(void);
const char *SysGetDataTimeString(void);
void SysInterruptSet(bool enable);
void SysReboot(void);
void SysInitialize(void);
void SysPoll(void);

#endif

