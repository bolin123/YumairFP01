#ifndef YUMAIR_PROTOCOL_H
#define YUMAIR_PROTOCOL_H

#include "Sys.h"

typedef enum
{
    SENSOR_TYPE_PM25 = 0,
    SENSOR_TYPE_CO,
    SENSOR_TYPE_NO2,
    SENSOR_TYPE_SO2,
    SENSOR_TYPE_O3,
    SENSOR_TYPE_VOC,
    SENSOR_TYPE_NOISE,
    SENSOR_TYPE_COUNT,
}YPSensorType_t;

typedef struct
{
    uint8_t method;
    uint8_t valnum;
    float *value;
    YPSensorType_t target;
}YPSensorParam_t;

typedef enum
{
    OTA_STATUS_START = 0,
    OTA_STATUS_SUCCESS,
    OTA_STATUS_FAILED,
}YPOtaStatus_t;

typedef enum
{
    YP_EVENT_REBOOT = 0,        //重启
    YP_EVENT_READ_SENSOR_ARGS,  //读传感器值
    YP_EVENT_SET_SENSOR_ARGS,   //写传感器值
    YP_EVENT_SET_POST_INTERVAL, //写上报时间间隔
    YP_EVENT_OTA,        //ota
}YPEvent_t;

typedef void (* YPEvent_cb)(YPEvent_t event, void *opt, uint32_t ackid);

int8_t YPFaultsReport(uint8_t faults[], uint8_t num);
void YPSensorParamSend(YPSensorParam_t *param, uint32_t ackid);
void YPOptResultSend(uint32_t ackid, bool success);
int8_t YPPostAllProperties(void);
int8_t YPPropertySet(const char *name, float value, uint8_t flag);
int8_t YPPropertyRegister(const char *name, const char *flagName);
void YPEventHandleRegister(YPEvent_cb handle);
void YPStop(void);
void YPStart(void);
void YPPoll(void);
void YPInitialize(void);

#endif

