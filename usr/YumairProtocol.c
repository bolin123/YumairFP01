#include "YumairProtocol.h"
#include "YumairPrivate.h"
#include "SysTimer.h"
#include "VTList.h"
#include "HTTPRequest.h"
#include "GPRS.h"
#include "MD5.h"

#define YP_SERVER_HOST "test.machtalk.net"
#define YP_SERVER_PORT 10066
#define YP_SERVER_RECONNECT_COUNT_MAX 10
//#define YP_SERVER_HOST "192.168.0.67"
//#define YP_SERVER_PORT 10066

#define YP_NEED_MAINTAIN_LINK 0 //是否发送心跳维护链接
#define YP_HEARTBEAT_TIME  60000 //1min
#define YP_TIME_ADJUST_TIME (20*60*1000) //20分钟 校时时间

#define YP_QN_NUM_LEN  18
#define YP_QN_TIMEOUT  60000 //1min 应答超时时间

typedef enum
{
    SERVER_DISCONNECT = 0,
    SERVER_CONNECTED,
    SERVER_LOGIN,
}YPServerStatus_t;

typedef struct YPReplyQn_st
{
    uint32_t id;
    char *num;
    SysTime_t timeout;
    VTLIST_ENTRY(struct YPReplyQn_st);
}YPReplyQn_t;

static uint8_t g_reconnectCount = 0;
static YPServerStatus_t g_serverStatus = SERVER_DISCONNECT;
static GPRSTcpSocket_t *g_socket = NULL;
static SysTime_t g_serverOnlineTime;
static uint32_t g_reportInterval = 60; //default 1min
static bool g_start = false;
static uint64_t g_faultNum = 0;
static YPEvent_cb g_eventHandle = NULL;
static char *g_sensorName[SENSOR_TYPE_COUNT] = {"PM25", "CO", "NO2", "SO2", "O3", "VOC", "NOISE"};
//static char g_qnNum[18] = {0};
static YPReplyQn_t g_replyQN;

static uint32_t g_otaReplyID;
static SysOtaInfo_t g_otaInfo;
static HTTPRequest_t *g_otaHttp = NULL;

int8_t YPFaultsReport(uint8_t faults[], uint8_t num)
{
    uint8_t i;
    uint64_t newFaultNum = 0;
    uint8_t errNum[64], count = 0;

    SysLog("");
    for(i = 0; i < num; i++)
    {
        if((faults[i] - 1) < 64)
        {
            newFaultNum |= (uint64_t)0x1 << (faults[i] - 1);
        }
    }
    if(newFaultNum != g_faultNum)
    {
        g_faultNum = newFaultNum;
        for(i = 0; i < 64; i++)
        {
            if(g_faultNum & ((uint64_t)0x1 << i))
            {
                errNum[count++] = i + 1;
            }
        }
        YPriErrorReport(errNum, count);
    }
    return 0;
}

int8_t YPPostAllProperties(void)
{
    SysLog("");
    if(g_serverStatus == SERVER_LOGIN)
    {
        YPriPropertiesPost();
        return 0;
    }
    return -1;
}

int8_t YPPropertySet(const char *name, float value, uint8_t flag)
{
    char valstr[16] = "";

    SysLog("%s, value:%f, flag:%d", name, value, flag);
    sprintf(valstr, "%f", value);
    YPriPropertySet(name, valstr, flag);
    return 0;
}

int8_t YPPropertyRegister(const char *name, const char *flagName)
{
    SysLog("name: %s, flag: %s", name, flagName);
    YPriPropertyRegister(name, flagName);
    return 0;
}

void YPEventHandleRegister(YPEvent_cb handle)
{
    g_eventHandle = handle;
}

static void tcpDisconnetCb(uint8_t id)
{
    if(id != g_socket->socketId)
    {
        return;
    }
    g_serverStatus = SERVER_DISCONNECT;
    g_reconnectCount = 0;
}

static void tcpConnectCb(uint8_t id, bool success)
{
    uint8_t i, count = 0;
    uint8_t errNum[64] = {0};
    
    if(id != g_socket->socketId)
    {
        return;
    }
    
    if(success)
    {
        for(i = 0; i < 64; i++)
        {
            if(g_faultNum & ((uint64_t)0x1 << i))
            {
                errNum[count++] = i + 1;
            }
        }
        YPriLoginNotice(g_reportInterval, errNum, count);
        g_serverStatus = SERVER_CONNECTED;
        g_serverOnlineTime = SysTime();
        g_reconnectCount = 0;
    }
    else
    {
        g_serverStatus = SERVER_DISCONNECT;
    }
}

static void tcpRecvCb(uint8_t id, uint8_t *data, uint16_t len)
{
    if(id != g_socket->socketId)
    {
        return;
    }
    YPriMessageRecv((char *)data, len);
}

static void tcpDataSend(const char *data, uint16_t len)
{
    if(g_serverStatus != SERVER_DISCONNECT)
    {
        SysPrintf("Msg send: %s\n", data);
        GPRSTcpSend(g_socket, (uint8_t *)data, len);
    }
}

static void gprsDelayPowerSwitch(void *args)
{
    int power = (int)args;

    if(power)
    {
        GPRSStart();
    }
    else
    {
        GPRSStop();
    }
}

static void reportGPSLocation(float latitude, float longitude)
{
    char buff[80];
    SysDateTime_t *date = SysGetDateTime();

    SysLog("latitude = %f, longitude = %f", latitude, longitude);
    //DataTime=20040516020111;LON-Rtd=116.604630;LAT-Rtd=35.442291
    sprintf(buff, "DataTime=%04d%02d%02d%02d%02d%02d;LON-Rtd=%f;LAT-Rtd=%f", date->year, \
                                                                        date->month, date->day, \
                                                                        date->hour, date->minute, date->second,\
                                                                        longitude, latitude);
    
    YPriPostData(YP_CMD_DATA_REPORT, NULL, buff);
}

void gprsEventHandle(GPRSEvent_t event, void *args)
{
    GPRSStatus_t status;
    GNSSLocation_t *gnss;

    SysLog("event = %d", event);
    switch(event)
    {
        case GEVENT_POWER_ON:    //GPRS上电
            if(!g_start)
            {
                SysTimerSet(gprsDelayPowerSwitch, 2000, 0, (void *)0);
            }
            break;
        case GEVENT_POWER_OFF:       //GPRS掉电
            if(g_start)
            {
                SysTimerSet(gprsDelayPowerSwitch, 2000, 0, (void *)1);
            }
            break;
        case GEVENT_GNSS_START:      //GPS开启
            break;
        case GEVENT_GNSS_STOP:       //GPS关闭
            break;
        case GEVENT_GNSS_FIXED:      //GPS锁定信息
            gnss = GNSSGetLocation();
            SysSetLocation(gnss->location.latitude, gnss->location.longitude);
            GNSSStop();
            reportGPSLocation(gnss->location.latitude, gnss->location.longitude);
            break;
        case GEVENT_GPRS_STATUS_CHANGED: //GPRS状态改变
            status = (GPRSStatus_t)(int)args;
            if(status == GPRS_STATUS_GPRS_DONE)
            {
                SysStatusLedSet(1);
            }
            else if(status == GPRS_STATUS_GSM_DONE)
            {
                SysStatusLedSet(2);
                GNSSStart();
            }
            else
            {
                SysStatusLedSet(3);
            }
            break;
        default:
            break;
    }
}

static void serverConnect(void)
{
    if(g_socket == NULL)
    {
        g_socket = GPRSTcpCreate();
    }
    g_socket->connectCb   = tcpConnectCb;
    g_socket->disconnetCb = tcpDisconnetCb;
    g_socket->recvCb      = tcpRecvCb;
    GPRSTcpConnect(g_socket, YP_SERVER_HOST, YP_SERVER_PORT);
}

static void serverDisConnect(void)
{
    GPRSTcpClose(g_socket);
}

static void checkGprsStart(void *args)
{
    if(GPRSGetStatus() == GPRS_STATUS_NONE) //仍未启动
    {
        GPRSStart();
    }
}

static void gprsRestart(void)
{
    int power = 1;

    SysLog("");
    GPRSStop();
    SysTimerSet(checkGprsStart, 15000, 0, &power); //15秒之后检查gprs是否启动
}

static void serverLinkHandle(void)
{
    static SysTime_t lastConnectTime = 0;
    
    if(GPRSConnected())
    {
        if(SERVER_DISCONNECT == g_serverStatus)
        {
            if(lastConnectTime == 0 || SysTimeHasPast(lastConnectTime, 20000))
            {
                if(g_reconnectCount > YP_SERVER_RECONNECT_COUNT_MAX)
                {
                    gprsRestart();
                    g_reconnectCount = 0;
                }
                else
                {
                    serverConnect();
                    lastConnectTime = SysTime();
                    g_reconnectCount++;
                }
            }
        }
        else
        {
        #if YP_NEED_MAINTAIN_LINK
            if(SysTimeHasPast(g_serverOnlineTime, YP_HEARTBEAT_TIME * 3))
            {
                serverDisConnect();
                g_serverStatus = SERVER_DISCONNECT;
            }
        #endif
        }
    }
}

static void heartbeatSend(void)
{
#if YP_NEED_MAINTAIN_LINK

    static SysTime_t lastHbTime = 0;
    
    if(g_serverStatus == SERVER_LOGIN)
    {
        if(SysTimeHasPast(lastHbTime, YP_HEARTBEAT_TIME))
        {
            YPriHeatbeatSend();
            lastHbTime = SysTime();
        }
    }
#endif
}

//返回上位机请求的结果
static YPriRequestReturn_t deviceStatusQuery(uint16_t msgID)
{
    //todo: 
    return REQUEST_RETURN_READY;
}

SysDateTime_t *dataTimeConvert(const char *timestr)
{
    static SysDateTime_t dataTime;
    char *keyName = strstr(timestr, "SystemTime=");
    char *time;
    char data[5] = {0};

    if(keyName)
    {
        time = strchr(keyName, '=');
        if(time)
        {
            time += 1;
            memcpy(data, time, 4); //year
            dataTime.year = atoi(data);
            
            time += 4;
            memcpy(data, time, 2); //month
            data[2] = '\0';
            dataTime.month = atoi(data);

            time += 2;
            memcpy(data, time, 2); //day
            data[2] = '\0';
            dataTime.day = atoi(data);

            time += 2;
            memcpy(data, time, 2); //hour
            data[2] = '\0';
            dataTime.hour = atoi(data);

            time += 2;
            memcpy(data, time, 2); //min
            data[2] = '\0';
            dataTime.minute = atoi(data);

            time += 2;
            memcpy(data, time, 2); //sec
            data[2] = '\0';
            dataTime.second = atoi(data);
            
            return &dataTime;
        }
    }
    return NULL;
}

static void replyQnTimeoutHandle(void)
{
    YPReplyQn_t *qn;

    VTListForeach(&g_replyQN, qn)
    {
        if(SysTimeHasPast(qn->timeout, YP_QN_TIMEOUT))
        {
            YPriOptResultSend(qn->num, OPT_RESULT_FAILED);
            VTListDel(qn);
            if(qn->num)
            {
                free(qn->num);
            }
            free(qn);
        }
    }
}

static uint32_t replyQnInsert(const char *qnNum)
{
    YPReplyQn_t *qn;
    
    qn = (YPReplyQn_t *)malloc(sizeof(YPReplyQn_t));
    if(qn)
    {
        qn->num = (char *)malloc(YP_QN_NUM_LEN);
        if(qn->num)
        {
            strncpy(qn->num, qnNum, YP_QN_NUM_LEN);
        }
        qn->id = SysTime();
        qn->timeout = SysTime();
        VTListAdd(&g_replyQN, qn);
        return qn->id;
    }
    return 0;
}

YPReplyQn_t *getReplyQn(uint32_t qnID)
{
    YPReplyQn_t *qn = NULL;
    
    VTListForeach(&g_replyQN, qn)
    {
        if(qn->id == qnID)
        {
            return qn;
        }
    }

    return NULL;
}

void YPOptResultSend(uint32_t ackid, bool success)
{
    YPReplyQn_t *qn = getReplyQn(ackid);

    if(qn)
    {
        if(success)
        {
            YPriOptResultSend(qn->num, OPT_RESULT_SUCCESS);
        }
        else
        {
            YPriOptResultSend(qn->num, OPT_RESULT_FAILED);
        }
        
        VTListDel(qn);
        if(qn->num)
        {
            free(qn->num);
        }
        free(qn);
    }
}

void YPSensorParamSend(YPSensorParam_t *param, uint32_t ackid)
{
    uint8_t i;
    uint16_t len = 0;
    char *buff = NULL;
    YPReplyQn_t *qn = getReplyQn(ackid);

    if(param == NULL)
    {
        return;
    }
    SysLog("");

    if(qn)
    {
        if(param->value != NULL)
        {
            buff = malloc(param->valnum * 15 + 50); //15-float value length, 50 others
        
            if(buff)
            {
                //Calib-Method=2,Calib-Target=PM2.5,Calib-Param=[1,2]
                len += sprintf(buff, "Calib-Method=%d,Calib-Target=%s,Calib-Param=[", param->method, g_sensorName[param->target]);
                for(i = 0; i < param->valnum - 1; i++)
                {
                    len += sprintf(buff + len, "%f,", param->value[i]);
                }
                len += sprintf(buff + len, "%f]", param->value[param->valnum - 1]);
                YPriPostData(YP_CMD_GET_ARGS, qn->num, buff);
                free(buff);
                
                YPriOptResultSend(qn->num, OPT_RESULT_SUCCESS);
            }
        }
        else
        {
            YPriOptResultSend(qn->num, OPT_RESULT_FAILED);
        }

        VTListDel(qn);
        if(qn->num)
        {
            free(qn->num);
        }
        free(qn);
    }
    
}

static int8_t parseSensorParam(const char *text, YPSensorParam_t *param)
{
    uint8_t i;
    char *ptr = NULL;
    char *paramBegin, *paramEnd;
//    char *paramValue;
    uint8_t count = 0;

    SysLog("%s", text);
    if(param && text)
    {
        ptr = strstr(text, "Calib-Method=");
        if(ptr)
        {
            ptr = &ptr[strlen("Calib-Method=")];
            param->method = (uint8_t)atoi(ptr);
        }
        else
        {
            SysLog("not found method!");
            return -1;
        }
        
        ptr = strstr(text, "Calib-Target=");
        if(ptr)
        {
            ptr = &ptr[strlen("Calib-Target=")];

            for(i = 0; i < SENSOR_TYPE_COUNT; i++)
            {
                if(strstr(ptr, g_sensorName[i]))
                {
                    param->target = (YPSensorType_t)i;
                    break;
                }
            }
        }
        else
        {
            SysLog("not found target!");
            return -2;
        }

        ptr = strstr(text, "Calib-Param=");
        if(ptr)
        {
            ptr = &ptr[strlen("Calib-Param=")];
            paramBegin = strchr(ptr, '[');
            paramEnd = strchr(ptr, ']');
            count = 0;
            if(paramBegin && paramEnd)
            {
                ptr = paramBegin;
                while(ptr != NULL)
                {
                    count++;
                    ptr++;
                    ptr = strchr(ptr, ',');
                }
                param->valnum = count;
                param->value = malloc(sizeof(float) * count);

                ptr = paramBegin + 1;
                for(i = 0; i < count; i++)
                {
                    param->value[i] = atof(ptr);
                    ptr = strchr(ptr, ',') + 1;
                }
            }
        }
        else
        {
            param->value = NULL;
            param->valnum = 0;
        }
    }
    return 0;
}

static bool checkFirmwareMd5(void)
{
    MD5_CTX ctx;
    uint8_t data[512];
    uint16_t count, i, lastSize;
    uint32_t flashOffset = 0;
    uint8_t md5[16];
    
    SysMD5Init(&ctx);

    count = g_otaInfo.size / sizeof(data);
    lastSize = g_otaInfo.size % sizeof(data);

    for(i = 0; i < count; i++)
    {
        HalFlashRead(HAL_OTA_FLASH_ADDR + flashOffset, data, sizeof(data));
        SysMD5Update(&ctx, data, sizeof(data));
        flashOffset += sizeof(data);
    }
    if(lastSize)
    {
        HalFlashRead(HAL_OTA_FLASH_ADDR + flashOffset, data, lastSize);
        SysMD5Update(&ctx, data, lastSize);
        flashOffset += lastSize;
    }

    SysMD5Final(&ctx, md5);

    if(memcmp(md5, g_otaInfo.md5, 16) == 0)
    {
        SysLog("md5 check ok");
        return true;
    }

    SysLog("md5 check error!");
    return false;
}

static void httpRequestCallback(HTTPRequest_t *request, const uint8_t *data, uint16_t len, HTTPRequestError_t error)
{
    static uint32_t flashOffset = 0;
    
    if(error == HTTP_REQ_ERROR_NONE)
    {
        HalFlashWrite(HAL_OTA_FLASH_ADDR + flashOffset, data, len);
        flashOffset += len;
    }
    else if(error == HTTP_REQ_ERROR_SUCCESS)
    {
        if(g_otaInfo.size == request->respContentLength)
        {
            if(checkFirmwareMd5())
            {
                g_otaInfo.flag = SYS_OTA_DONE_FLAG;
                SysUpdateOtaInfo(&g_otaInfo);
                g_eventHandle(YP_EVENT_OTA, (void *)OTA_STATUS_SUCCESS, 0);
                YPOptResultSend(g_otaReplyID, true);
                
            }
            else
            {
                g_eventHandle(YP_EVENT_OTA, (void *)OTA_STATUS_FAILED, 0);
                YPOptResultSend(g_otaReplyID, false);
            }
        }
        else
        {
            g_eventHandle(YP_EVENT_OTA, (void *)OTA_STATUS_FAILED, 0);
            YPOptResultSend(g_otaReplyID, false);
        }
        HTTPRequestDestroy(g_otaHttp);
        g_otaHttp = NULL;
    }
    else
    {
        HTTPRequestDestroy(g_otaHttp);
        g_otaHttp = NULL;
        g_eventHandle(YP_EVENT_OTA, (void *)OTA_STATUS_FAILED, 0);
        YPOptResultSend(g_otaReplyID, false);
    }
}

static void eraseOtaFlashSections(void)
{
    int i;
    int count = HAL_FLASH_OTA_SIZE / HAL_FLASH_PAGE_SIZE;

    for(i = 0; i < count; i++)
    {
        HalFlashErase(HAL_OTA_FLASH_ADDR + i * HAL_FLASH_PAGE_SIZE);
    }
}

static void otaHandle(char *msg)
{
    uint8_t i;
    uint16_t urlLen = 0;
    char *url;
    char *ptr, *urlstart;
    char md5str[3] = {0};

    SysLog("%s", msg);
    //FMW-Type=yumair01,FMW-Version=1.0.0.1,FMW-Size=12345,FMW-MD5=E2FC714C4727EE9395F324CD2E7F331F,URL=xxx.xxx
    ptr = strstr(msg, "FMW-Size=");
    if(ptr)
    {
        g_otaInfo.size = strtol(&ptr[strlen("FMW-Size=")], NULL, 10);
    }

    ptr = strstr(msg, "FMW-MD5=");
    if(ptr)
    {
        ptr = &ptr[strlen("FMW-MD5=")];
        for(i = 0; i < 16; i++)
        {
            memcpy(md5str, ptr, 2);
            g_otaInfo.md5[i] = (uint8_t)strtol(md5str, NULL, 16);
            ptr += 2;
        }
    }
    
    ptr = strstr(msg, "URL=");
    if(ptr)
    {
        urlstart = &ptr[strlen("URL=")];
        
        if(strchr(urlstart, ';'))
        {
            urlLen = strchr(urlstart, ';') - urlstart;
        }
        else
        {
            urlLen = strlen(urlstart);
        }
        
        if(urlLen)
        {

            url = (char *)malloc(urlLen + 1);
            memset(url, 0, urlLen + 1);
            if(url)
            {
                memcpy(url, urlstart, urlLen);
                eraseOtaFlashSections();
                if(g_otaHttp != NULL)
                {
                    HTTPRequestDestroy(g_otaHttp);
                    g_otaHttp = NULL;
                }
                g_otaHttp = HTTPRequestCreate(url, HTTP_REQ_METHOD_GET);
                g_otaHttp->dataRecvCb = httpRequestCallback;
                HTTPRequestStart(g_otaHttp);
                free(url);

                g_eventHandle(YP_EVENT_OTA, (void *)OTA_STATUS_START, 0);
            }
        }
    }
}

static void serverRequestHandle(YPriSetMsg_t *msg)
{
    char *ptr = NULL;
    char data[128] = {0};
    SysLocation_t *location;
    YPSensorParam_t sersorParam;
//    YPReplyQn_t *qn;

    SysLog("msgID = %d", msg->mid);
    
    switch(msg->mid)
    {
        case YP_CMD_SET_TIME: //同步时间
            SysSetDataTime(dataTimeConvert(msg->value));
            YPriOptResultSend(msg->qn, OPT_RESULT_SUCCESS);
            break;
        case YP_CMD_GET_REPORT_INTERVAL: //读取上报间隔
            sprintf(data, "RtdInterval=%d", g_reportInterval);
            YPriPostData(msg->mid, msg->qn, data);
            YPriOptResultSend(msg->qn, OPT_RESULT_SUCCESS);
            break;        
        case YP_CMD_SET_REPORT_INTERVAL: //设置上报间隔
            ptr = strstr(msg->value, "RtdInterval=");
            if(ptr)
            {
                ptr = &ptr[strlen("RtdInterval=")];
                g_reportInterval = strtol(ptr, NULL, 10);
                SysSetReportInterval(g_reportInterval);
                YPriOptResultSend(msg->qn, OPT_RESULT_SUCCESS);
                g_eventHandle(YP_EVENT_SET_POST_INTERVAL, (void *)g_reportInterval, 0);
            }
            break;
        case YP_CMD_RESET: //复位
            YPriOptResultSend(msg->qn, OPT_RESULT_SUCCESS);
            g_eventHandle(YP_EVENT_REBOOT, NULL, 0);
            break;
        case YP_CMD_GET_ARGS: //读取参数
            if(parseSensorParam(msg->value, &sersorParam) < 0)
            {
                YPriOptResultSend(msg->qn, OPT_RESULT_FAILED);
            }
            else
            {
                g_eventHandle(YP_EVENT_READ_SENSOR_ARGS, &sersorParam, replyQnInsert(msg->qn));
            }

            if(sersorParam.value)
            {
                free(sersorParam.value);
                sersorParam.value = NULL;
            }
            break;
        case YP_CMD_SET_ARGS: //设置参数
            if(parseSensorParam(msg->value, &sersorParam) < 0)
            {
                YPriOptResultSend(msg->qn, OPT_RESULT_FAILED);
            }
            else
            {
                g_eventHandle(YP_EVENT_SET_SENSOR_ARGS, &sersorParam, replyQnInsert(msg->qn));
                //YPriOptResultSend(msg->qn, OPT_RESULT_SUCCESS);
            }
            
            if(sersorParam.value)
            {
                free(sersorParam.value);
                sersorParam.value = NULL;
            }
            break;
        case YP_CMD_OTA:
            g_otaReplyID = replyQnInsert(msg->qn);
            otaHandle(msg->value);
            //g_qnNum[0] = '\0';
            //strcpy(g_qnNum, msg->qn);
            break;
        case YP_CMD_GET_LOCATION: //定位
            location = SysGetLocation();
            if(location)
            {
                //LON-Rtd=116.59957;LAT-Rtd=35.44304
                sprintf(data, "LON-Rtd=%f;LAT-Rtd=%f", location->longitude, location->latitude);
                YPriPostData(msg->mid, msg->qn, data);
                YPriOptResultSend(msg->qn, OPT_RESULT_SUCCESS);
            }
            else
            {
                YPriOptResultSend(msg->qn, OPT_RESULT_NO_DATA);
            }
            break;       
        default:
            break;
    }
}

static void updateDeviceTime(void)
{
    static SysTime_t lastUpdateTime = 0;

    if(g_serverStatus == SERVER_LOGIN)
    {
        if(lastUpdateTime == 0 || SysTimeHasPast(lastUpdateTime, YP_TIME_ADJUST_TIME))//10min
        {
            YPriRequestTiming();
            lastUpdateTime = SysTime();
        }
    }
}

static int protocolEventHandle(YPriEvent_t event, void *args)
{
    int ret = 0;
    uint32_t value;
    
    g_serverOnlineTime = SysTime();
    SysLog("event = %d", event);
    switch(event)
    {
        case YPRI_EVENT_REQUEST:
            ret = deviceStatusQuery((uint16_t)(uint32_t)args);
            break;
        case YPRI_EVENT_REQUEST_VALUE:
            serverRequestHandle((YPriSetMsg_t *)args);
            break;
        case YPRI_EVENT_RECV_ACK:
            value = (uint32_t)args;
            if(value == YP_CMD_ONLINE)
            {
                g_serverStatus = SERVER_LOGIN;
            }
            break;
        case YPRI_EVENT_HEARTBEAT:
            SysPrintf("hearbeat \n");
            break;
        default:
            break;
    }
    return ret;
}

void YPStop(void)
{
    SysLog("");
    GPRSStop();
    g_start = false;
}

void YPStart(void)
{
    SysLog("");
    GPRSStart();
    g_start = true;
}

void YPPoll(void)
{
    if(g_start)
    {
        serverLinkHandle();
        replyQnTimeoutHandle();
        heartbeatSend();
        updateDeviceTime();
        YPriPoll();
    }
    GPRSPoll();
}

void YPInitialize(void)
{
    VTListInit(&g_replyQN);
    GPRSInitialize();
    GPRSEventHandleRegister(gprsEventHandle);
    YPriInitialize();
    YPriCallbackRegister(tcpDataSend, protocolEventHandle);
    g_reportInterval = SysGetReportInterval();
}

