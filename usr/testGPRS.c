#include "testGPRS.h"
#include "GPRS.h"
#include "SysTimer.h"
#include "HTTPRequest.h"
#include "YumairPrivate.h"

static bool g_connectOk = false;
static HTTPRequest_t *g_http;
static GPRSTcpSocket_t *g_socket = NULL;



#define FIRMWARE_START_ADDR 0x08020000

static void httpRequestCallback(HTTPRequest_t *request, const uint8_t *data, uint16_t len, HTTPRequestError_t error)
{
    static uint32_t flashOffset = 0;
    if(error == HTTP_REQ_ERROR_NONE)
    {
        HalFlashWrite(FIRMWARE_START_ADDR + flashOffset, data, len);
        flashOffset += len;
    }
    else if(error == HTTP_REQ_ERROR_SUCCESS)
    {
        HTTPRequestDestroy(g_http);
    }
}


//?20000?
static void eraseFlashFirst(void)
{
    uint8_t i = 0;

    for(i = 0; i < 50; i++) //50 pages
    {
        HalFlashErase(FIRMWARE_START_ADDR + i * HAL_FLASH_BLOCK_SIZE);
    }
}

static void startOta(void *args)
{
    SysLog("");
    eraseFlashFirst();
    g_http = HTTPRequestCreate("http://yumair.machtalk.net/v1.1/yumair/upgrade/YUM/100/E06/1.0.1.19/0", \
                        HTTP_REQ_METHOD_GET);
    g_http->dataRecvCb = httpRequestCallback;
    HTTPRequestStart(g_http);
}

static void connectCb(uint8_t id, bool success)
{
    SysLog("%d, connect success %d", id, success);
    g_connectOk = success;
    YPriLoginNotice(60);

    uint8_t errcode[5] = {3,4,6,8,9};
    YPriErrorReport(errcode, sizeof(errcode));

    YPriRequestTiming();
}

static void disconnetCb(uint8_t id)
{
    SysLog("%d", id);
    g_connectOk = false;
}

static void recvCb(uint8_t id, uint8_t *data, uint16_t len)
{
    SysLog("%d", id);
}

static void connectServer(void)
{
    SysLog("");
    if(g_socket)
    {
        
        GPRSTcpConnect(g_socket, "yaolj.nat123.cc", 37056);
    }
}

static void gprsEventHandle(GPRSEvent_t event, void *args)
{
    GPRSStatus_t status;
    GNSSLocation_t *nmea;
    
    switch(event)
    {
        case GEVENT_POWER_ON:
            break;
        case GEVENT_POWER_OFF:
            break;
        //case GEVENT_GPRS_GOT_IP:
        //    GPRSTcpConnect("yaolj.nat123.cc", 37056, tcpRecvHandle);
        //    break;
        #if 0
        case GEVENT_TCP_CONNECT_OK:
            g_connectOk = true;
            break;
        case GEVENT_TCP_CONNECT_FAIL:
            g_connectOk = false;
            break;
        case GEVENT_TCP_CLOSED:
            g_connectOk = false;
            break;
        #endif
        case GEVENT_GNSS_START:
            break;
        case GEVENT_GNSS_STOP:
            break;
        case GEVENT_GNSS_FIXED:
            nmea = (GNSSLocation_t *)args;
            SysPrintf("%d-%d-%d %d:%d:%d.%d\n", nmea->time.year, \
                                             nmea->time.month, \
                                             nmea->time.day, \
                                             nmea->time.hour, \
                                             nmea->time.minute, \
                                             nmea->time.second, \
                                             nmea->time.msec);
            SysPrintf("latitude = %f, longitude = %f\n", nmea->location.latitude, \
                                                        nmea->location.longitude);
            GNSSStop();
            break;
        case GEVENT_GPRS_STATUS_CHANGED:
            status = (GPRSStatus_t)(int)args;
            SysPrintf("Status changed -> %d\n", status);
            if(status == GPRS_STATUS_GPRS_DONE)
            {
                SysStatusLedSet(1);
                //GPRSTcpConnect("yaolj.nat123.cc", 37056, tcpRecvHandle);
                connectServer();
                //SysTimerSet(startOta, 8000, 0, NULL);
            }
            else if(status == GPRS_STATUS_GSM_DONE)
            {
                SysStatusLedSet(2);
            }
            break;
        default:
            break;
    }
}

static void confirmPowerOn(void *args)
{
    if(GPRSGetStatus() == GPRS_STATUS_NONE)
    {
        GPRSStart();
    }
    
}

static void setPropertyValue(void)
{
    static float pvalue = 1.111;
    char valstr[8];

    valstr[0] = '\0';
    sprintf(valstr, "%.3f", pvalue);
    pvalue += 1.111;
    YPriPropertySet("WP-Rtd", valstr, 0);

    valstr[0] = '\0';
    sprintf(valstr, "%.3f", pvalue);
    pvalue += 1.111;
    YPriPropertySet("TP-Rtd", valstr, 0);

    valstr[0] = '\0';
    sprintf(valstr, "%.3f", pvalue);
    pvalue += 1.111;
    YPriPropertySet("TD-Rtd", valstr, 0);

    valstr[0] = '\0';
    sprintf(valstr, "%.3f", pvalue);
    pvalue += 1.111;
    YPriPropertySet("WD-Rtd", valstr, 0);

    valstr[0] = '\0';
    sprintf(valstr, "%.3f", pvalue);
    pvalue += 1.111;
    YPriPropertySet("WS-Rtd", valstr, 0);

    valstr[0] = '\0';
    sprintf(valstr, "%.3f", pvalue);
    pvalue += 1.111;
    YPriPropertySet("PM2.5-Rtd", valstr, 0);

    valstr[0] = '\0';
    sprintf(valstr, "%.3f", pvalue);
    pvalue += 1.111;
    YPriPropertySet("PM10-Rtd", valstr, 0);

    valstr[0] = '\0';
    sprintf(valstr, "%.3f", pvalue);
    pvalue += 1.111;
    YPriPropertySet("CO-Rtd", valstr, 1);

    valstr[0] = '\0';
    sprintf(valstr, "%.3f", pvalue);
    pvalue += 1.111;
    YPriPropertySet("NO2-Rtd", valstr, 1);

    valstr[0] = '\0';
    sprintf(valstr, "%.3f", pvalue);
    pvalue += 1.111;
    YPriPropertySet("SO2-Rtd", valstr, 1);

    valstr[0] = '\0';
    sprintf(valstr, "%.3f", pvalue);
    pvalue += 1.111;
    YPriPropertySet("O3-Rtd", valstr, 1);
}


void testGPRSPoll(void)
{

#if 1
    static SysTime_t lastSendTime = 0;
    static SysTime_t lastGNSSTime = 0;
//    GNSSLocation_t *location;
//    char *testCmd = "QN=20040516010101001;ST=22;CN=3021;PW=16944725;MN=YA00100000000043;CP=&&RtdInterval=60;FMW-Type=yumair01;FMW-Version=1.0.0.1&&";

    GPRSPoll();

#if 0
    if(SysTimeHasPast(lastGNSSTime, 180000))
    {
        GNSSStart();
        lastGNSSTime = SysTime();
    }
#endif
    if(GPRSGetStatus() == GPRS_STATUS_GPRS_DONE && g_connectOk)
    {
        if(SysTimeHasPast(lastSendTime, 20000))
        {
            setPropertyValue();
            YPriPropertiesPost();
            //GPRSTcpSend(g_socket, (uint8_t *)testCmd, strlen(testCmd));
            lastSendTime = SysTime();
            #if 0
            location = GNSSGetLocation();
            SysPrintf("year:%d, month:%d, day:%d, hour:%d, min:%d, sec:%d, msec:%d\n", \
                                                                 location->time.year, \
                                                                 location->time.month, \
                                                                 location->time.day, \
                                                                 location->time.hour, \
                                                                 location->time.minute, \
                                                                 location->time.second, \
                                                                 location->time.msec);
            SysPrintf("latitude = %f, longitude = %f\n", location->location.latitude, \
                                                            location->location.longitude);
            #endif
        }
    }
#endif
}

static void registerProperty(void)
{
//    WP-Rtd=1.111,WP-Flag=0;TP-Rtd=2.222,TP-Flag=0;TD-Rtd=3.333,TD-Flag=0;WD-Rtd=4.444,WD-Flag=0;WS-Rtd=5.555,WS-Flag=0;PM2.5-Rtd=6.666,PM2.5-Flag=0;PM10-Rtd=7.777,PM10-Flag=0;CO-Rtd=8.888,CO-Flag=0;
//    NO2-Rtd=9.999,NO@-Flag=0;SO2-Rtd=10.000,SO2-Flag=0;O3-Rtd=11.000,O3-Flag=0
    YPriPropertyRegister("WP-Rtd", "WP-Flag");
    YPriPropertyRegister("TP-Rtd", "TP-Flag");
    YPriPropertyRegister("TD-Rtd", "TD-Flag");
    YPriPropertyRegister("WD-Rtd", "WD-Flag");
    YPriPropertyRegister("WS-Rtd", "WS-Flag");
    YPriPropertyRegister("PM2.5-Rtd", "PM2.5-Flag");
    YPriPropertyRegister("PM10-Rtd", "PM10-Flag");
    YPriPropertyRegister("CO-Rtd", "CO-Flag");
    YPriPropertyRegister("NO2-Rtd", "NO2-Flag");
    YPriPropertyRegister("SO2-Rtd", "SO2-Flag");
    YPriPropertyRegister("O3-Rtd", "O3-Flag");

    
}

static void testSendData(const char *data, uint16_t len)
{
    GPRSTcpSend(g_socket, (uint8_t *)data, len);
}

void testGPRSInit(void)
{
    GPRSInitialize();

    GPRSEventHandleRegister(gprsEventHandle);
    GPRSStart();
    SysTimerSet(confirmPowerOn, 3000, 0, NULL);
    g_socket = GPRSTcpCreate();
    g_socket->connectCb = connectCb;
    g_socket->disconnetCb = disconnetCb;
    g_socket->recvCb = recvCb;
    SysStatusLedSet(3);
    registerProperty();
    SysSetDevType("YMRFP001");
    YPriCallbackRegister(testSendData, NULL);
}

