#include "testProtocol.h"
#include "YumairProtocol.h"

static bool g_otaStart = false;

static void eventHandle(YPEvent_t event, void *args)
{
    uint8_t i;
    YPOtaStatus_t otaStatus;
    YPSensorParam_t *param = NULL;
    YPSensorParam_t *paramSend;
    float svalue[10] = {1.1,2.22,3.333,4,5.5555,6.6,7.7777,8.88888,9.9,10.0};
    
    SysLog("event = %d", event);

    if(YP_EVENT_SET_SENSOR_ARGS == event)
    {
        param = (YPSensorParam_t *)args;

        if(param)
        {
            SysPrintf("set sensor :method = %d, target = %d\n", param->method, param->target);
            SysPrintf("Value:[");
            for(i = 0; i < param->valnum; i++)
            {
                SysPrintf("%0.4f,", param->value[i]);
            }
            SysPrintf("]\n");
        }
    }
    else if(YP_EVENT_READ_SENSOR_ARGS == event)
    {
        param = (YPSensorParam_t *)args;
        paramSend = (YPSensorParam_t *)malloc(sizeof(YPSensorParam_t));

        if(paramSend)
        {
            paramSend->method = param->method;
            paramSend->target = param->target;
            paramSend->valnum = 10;
            paramSend->value = svalue;
            YPSensorParamSend(paramSend);
            free(paramSend);
        }
    }
    else if(YP_EVENT_REBOOT == event)
    {
        SysReboot();
    }
    else if(YP_EVENT_OTA == event)
    {
        otaStatus = (YPOtaStatus_t)(int)args;
        if(OTA_STATUS_START == otaStatus)
        {
            SysLog("OTA_STATUS_START");
            g_otaStart = true;
        }
        else if(OTA_STATUS_SUCCESS == otaStatus)
        {
            SysLog("OTA_STATUS_SUCCESS");
            SysReboot();
            g_otaStart = false;
        }
        else
        {
            SysLog("OTA_STATUS_FAILED");
            SysReboot();
            g_otaStart = false;
        }
    }
}

static void setPropertyValue(void)
{
    static float pvalue = 1.111;
    static SysTime_t lastPostTime = 0;

    if(SysTimeHasPast(lastPostTime, 60000))
    {

        pvalue += 1.111;
        YPPropertySet("WP-Rtd", pvalue, 0);
        pvalue += 1.111;
        YPPropertySet("TP-Rtd", pvalue, 0);

        pvalue += 1.111;
        YPPropertySet("TD-Rtd", pvalue, 0);
#if 1

        pvalue += 1.111;
        YPPropertySet("WD-Rtd", pvalue, 0);

        pvalue += 1.111;
        YPPropertySet("WS-Rtd", pvalue, 0);

        pvalue += 1.111;
        YPPropertySet("PM2.5-Rtd", pvalue, 0);

        pvalue += 1.111;
        YPPropertySet("PM10-Rtd", pvalue, 0);

        pvalue += 1.111;
        YPPropertySet("CO-Rtd", pvalue, 1);

        pvalue += 1.111;
        YPPropertySet("NO2-Rtd", pvalue, 1);

        pvalue += 1.111;
        YPPropertySet("SO2-Rtd", pvalue, 1);

        pvalue += 1.111;
        YPPropertySet("O3-Rtd", pvalue, 1);
#endif
        //////////////////////
        //if(!g_otaStart)
        {
            YPPostAllProperties();
        }
        
        lastPostTime = SysTime();
    }
}


static void propertyRegist(void)
{
    YPPropertyRegister("WP-Rtd", "WP-Flag");
    YPPropertyRegister("TP-Rtd", "TP-Flag");
    YPPropertyRegister("TD-Rtd", "TD-Flag");
#if 1
    YPPropertyRegister("WD-Rtd", "WD-Flag");
    YPPropertyRegister("WS-Rtd", "WS-Flag");
    YPPropertyRegister("PM2.5-Rtd", "PM2.5-Flag");
    YPPropertyRegister("PM10-Rtd", "PM10-Flag");
    YPPropertyRegister("CO-Rtd", "CO-Flag");
    YPPropertyRegister("NO2-Rtd", "NO2-Flag");
    YPPropertyRegister("SO2-Rtd", "SO2-Flag");
    YPPropertyRegister("O3-Rtd", "O3-Flag");
    #endif
}

void testProtocolInit(void)
{
    SysSetDevType("YUMAIR_01");
    YPInitialize();
    YPEventHandleRegister(eventHandle);
    YPStart();//test 
    propertyRegist();
}

void testProtocolPoll(void)
{
    YPPoll();
    setPropertyValue();
}

