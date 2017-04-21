#include "YumairPrivate.h"
#include "VTList.h"

#define YPR_RETRY_COUNT_MAX  3
#define YPR_RETRY_TIMEOUT  10000

//QN=20040516010101001;ST=22;CN=3022;PW=16944725;MN=YA00100000000043;
#define YPRI_PROTO_HEAD_NORMAL "QN=%s;ST=%d;CN=%d;PW=%s;MN=%s;Flag=1;"
#define YPRI_PROTO_HEAD_POST  "ST=%d;CN=%d;PW=%s;MN=%s;"

#define YPRI_ST_AIR_QUALITY  22 //空气质量
#define YPRI_ST_SYS_INTERACT 91 //系统交互

//key name
#define KEY_QN  "QN="
#define KEY_CN  "CN="
#define KEY_MN  "MN="
#define KEY_PWD "PW="
#define KEY_CP  "CP="
#define KEY_FLAG "Flag="

typedef struct YPriMsgRetry_st
{
    uint8_t retryCount;
    uint16_t length;
    char *contents;
    SysTime_t lastSendTime;
    VTLIST_ENTRY(struct YPriMsgRetry_st);
}YPriMsgRetry_t;

/*
typedef struct YPriMsg_st
{
    uint16_t id;
    YPriMsgType_t type : 16;
    VTLIST_ENTRY(struct YPriMsg_st);
}YPriMsg_t;
*/
typedef struct
{
    char *name;
    uint32_t value;
}YPriFlag_t;

typedef struct YPriProperty_st
{
    char *name;
    char *value;
    YPriFlag_t flag;
    VTLIST_ENTRY(struct YPriProperty_st);
}YPriProperty_t;

static YPriProperty_t g_property;
static YPriDataSend_cb g_sendFunc = NULL;
static YPriEventHandle_cb g_eventHandle = NULL;
//static YPriMsg_t g_msgInfo;
static YPriMsgRetry_t g_msgRetry;

static char *propertyToText(void)
{
    uint16_t len = 0;
    uint16_t nodelen = 0;
    char *buff = NULL;
    YPriProperty_t *node;
    
#if 1
    //DataTime=20170415114000; //24 datatime length
    buff = (char *)malloc(25);
    SysDateTime_t *date = SysGetDateTime();
    sprintf(buff, "DataTime=%04d%02d%02d%02d%02d%02d;", date->year, date->month, date->day, \
                                                       date->hour, date->minute, date->second);
    len += strlen(buff);
    nodelen = 25;
#endif
    VTListForeach(&g_property, node)
    {
        if(node->flag.name != NULL)
        {
            nodelen += (strlen(node->name) + strlen(node->value) + strlen(node->flag.name) + 6);
            buff = realloc(buff, nodelen);
            len += sprintf(buff + len, "%s=%s,%s=%d;", node->name, node->value, \
                                        node->flag.name, node->flag.value);
        }
        else
        {
            nodelen += (strlen(node->name) + strlen(node->value) + 3);
            buff = realloc(buff, nodelen);
            len += sprintf(buff + len, "%s=%s;", node->name, node->value);
        }
    }
    return buff;
}

static uint16_t crc16(uint8_t *data, uint16_t len)
{
    uint16_t i = 0;
    uint16_t crc = 0xffff;  

    while(len--)
    {
        for(crc ^= *(data++), i = 0; i < 8; i++)
        {
            crc = (crc & 0x0001) ? (crc >> 1) ^ 0xA001 : crc >> 1;
        }
    }
       
    return crc;
}


static YPriMsgType_t getMsgType(uint16_t msgID)
{
    //YPriMsgType_t type = MSG_TYPE_NONE;

    //switch(msgID)
    //{
    //case YP_CMD_SET_TIME: //设置现场机时间
    //    //MSG_TYPE_NOTICE
    //    break;
    //case YP_CMD_RESET: //重启现场机
    //    break;
    //case YP_CMD_GET_ARGS: //读取现场机传感器参数
    //    break;
    //case YP_CMD_SET_ARGS: //设置现场机传感器参数
    //    break;
    //case YP_CMD_OTA: //升级现场机固件
    //    break;
    //case YP_CMD_GET_LOCATION: //读取现场机GPS数据
    //    break;
    //case YP_CMD_SET_REPORT_INTERVAL: //设置实时数据上报间隔
    //    break;
    //case YP_CMD_GET_REPORT_INTERVAL: //读取实时数据上报间隔
    //    break;
    //default:
    //    break;
    //}
    return MSG_TYPE_REQUEST; //目前服务器下发的消息全部为请求应答
#if 0
    YPriMsg_t *msg;

    VTListForeach(&g_msgInfo, msg)
    {
        if(msg->id == msgID)
        {
            return msg->type;
        }
    }

    return MSG_TYPE_NONE;
#endif
}


static void frameSend(uint8_t st, uint16_t cn, const char *data, bool needAck)
{
    uint16_t crc;
    uint16_t length;
    char crcstr[7];
    char frame[800];
    char *head = NULL;
    char *contents = NULL;
    YPriMsgRetry_t *msgList;

    head = (char *)malloc(100);
    if(head == NULL)
    {
        SysLog("no mem!");
        return ;
    }
    if(needAck)
    {
        sprintf(head, YPRI_PROTO_HEAD_NORMAL, SysGetDataTimeString(), st, cn, \
                                    SysGetDevicePwd(), SysGetDeviceID());
    }
    else
    {
        sprintf(head, YPRI_PROTO_HEAD_POST, st, cn, SysGetDevicePwd(), SysGetDeviceID());
    }

    length = strlen(head) + strlen(data) + 7; // 7:"CP=&&&&"

    //frame = malloc(length + 16);
    //if(frame)
    //{
    sprintf(frame, "##%04d%sCP=&&%s&&", length, head, data);
    free(head);

    contents = &frame[6];
    crc = crc16((uint8_t *)contents, length);
    sprintf(crcstr, "%02x%02x\r\n", (uint8_t)(crc >> 8), (uint8_t)crc);
    strcat(frame, crcstr);

    //SysLog("%s", frame);
    if(needAck)
    {
        msgList = (YPriMsgRetry_t *)malloc(sizeof(YPriMsgRetry_t));
        if(msgList)
        {
            msgList->length = strlen(frame);
            msgList->retryCount = 0;
            msgList->lastSendTime = 0;
            msgList->contents = malloc(msgList->length);
            if(msgList->contents)
            {
                memcpy(msgList->contents, frame, msgList->length);
                VTListAdd(&g_msgRetry, msgList);
            }
            else
            {
                free(msgList);
            }
        }
    }
    else
    {
        g_sendFunc(frame, strlen(frame));
    }
        //free(frame);
    //}
}

static void msgRetryHandle(void)
{
    YPriMsgRetry_t *msg;

    msg = VTListFirst(&g_msgRetry);
    if(msg)
    {
        if(msg->lastSendTime == 0 || SysTimeHasPast(msg->lastSendTime, YPR_RETRY_TIMEOUT))
        {
            if(msg->retryCount < YPR_RETRY_COUNT_MAX)
            {

                g_sendFunc(msg->contents, msg->length);
                msg->lastSendTime = SysTime();
                msg->retryCount++;
            }
            else
            {
                VTListDel(msg);
                if(msg->contents)
                {
                    free(msg->contents);
                }
                free(msg);
            }
        }
    }
}

//操作结果
void YPriOptResultSend(const char *qn, YPriOptResult_t result)
{
    char buff[32] = {0};
    SysLog("");
    sprintf(buff, "QN=%s;ExeRtn=%d", qn, result);
    frameSend(YPRI_ST_SYS_INTERACT, YP_CMD_OPT_RESULT, buff, false);
}

//请求应答
static void requestAckSend(const char *qn, YPriRequestReturn_t result)
{
    char buff[32] = {0};
    SysLog("");
    sprintf(buff, "QN=%s;QnRtn=%d", qn, result);
    frameSend(YPRI_ST_SYS_INTERACT, YP_CMD_REQUEST_ACK, buff, false);
}

//通知应答
static void noticeAckSend(const char *qn)
{
    char buff[32] = {0};
    SysLog("");
    sprintf(buff, "QN=%s", qn);
    frameSend(YPRI_ST_SYS_INTERACT, YP_CMD_NOTICE_ACK, buff, false);
}

static char *getValueFromMsg(const char *name, const char *msg)
{
    char *pos, *end = NULL;
    char *value;

    pos = strstr(msg, name);
    if(pos)
    {
        pos = strchr(pos, '=');
        if(pos)
        {
            pos++;
            if(strchr(pos, ';'))
            {
                end = strchr(pos, ';');
            }
            else if(strchr(pos, '&'))
            {
                end = strchr(pos, '&');
            }
            else if(strchr(pos, ','))
            {
                end = strchr(pos, ',');
            }
            else
            {
                end = NULL;
            }
               
            
            if(end)
            {
                value = malloc(end - pos + 1);
                memcpy(value, pos, end - pos);
                value[end - pos] = '\0';
                return value;
            }
        }
    }
    return NULL;
}

static char *getCPTextFromMsg(const char *msg)
{
    char *pos, *end;
    char *text;

    pos = strstr(msg, KEY_CP);
    if(pos)
    {
        pos = strstr(pos, "&&");
        if(pos)
        {
            pos += 2;
            end = strstr(pos, "&&");
            if(end)
            {
                text = malloc(end - pos + 1);
                memcpy(text, pos, end - pos);
                text[end - pos] = '\0';
                return text;
            }
        }
    }
    return NULL;
}

static void findAndDelSendCache(const char *qn)
{
    YPriMsgRetry_t *msg;
    uint32_t msgID;

    VTListForeach(&g_msgRetry, msg)
    {
        if(strstr(msg->contents, qn) != NULL)
        {
            SysLog("del, qn=%s", qn);
            msgID = strtol(getValueFromMsg(KEY_CN, msg->contents), NULL, 10);
            g_eventHandle(YPRI_EVENT_RECV_ACK, (void *)msgID);
            VTListDel(msg);
            if(msg->contents)
            {
                free(msg->contents);
            }
            free(msg);
        }
    }
}

static void msgParse(const char *msg, uint16_t len)
{
    char *qnVal;
    char *cnVal;
    char *cpVal;
    uint16_t msgID;
    uint8_t requestResult;
    //YPriMsgType_t msgType;
    YPriSetMsg_t setMsg;
    
    //char *flag;
    
    qnVal = getValueFromMsg(KEY_QN, msg);
    cnVal = getValueFromMsg(KEY_CN, msg);
    //flag  = getValueFromMsg(KEY_FLAG, msg);
    cpVal = getCPTextFromMsg(msg);

    msgID = (uint16_t)(uint32_t)strtol(cnVal, NULL, 10);

    if(msgID == YP_CMD_NOTICE_ACK || msgID == YP_CMD_REQUEST_ACK)
    {
        SysLog("ack, qn=%s", qnVal);
        findAndDelSendCache(qnVal);
        goto DataFree;
    }
        
    if(msgID == YP_CMD_HEATBEAT_ACK)
    {
        g_eventHandle(YPRI_EVENT_HEARTBEAT, NULL);
        goto DataFree;
    }

    switch(getMsgType(msgID))
    {
        case MSG_TYPE_NOTICE:     //通知命令
            noticeAckSend(qnVal);
            break;
        case MSG_TYPE_REQUEST:    //请求命令
            requestResult = g_eventHandle(YPRI_EVENT_REQUEST, (void *)(uint32_t)msgID);
            requestAckSend(qnVal, (YPriRequestReturn_t)requestResult);
            break;
        default:
            break;
    }

    setMsg.mid = msgID;
    setMsg.value = cpVal;
    setMsg.qn = qnVal;
    g_eventHandle(YPRI_EVENT_REQUEST_VALUE, &setMsg);

DataFree:
    if(qnVal)
    {
        free(qnVal);
        qnVal = NULL;
    }
    if(cnVal)
    {
        free(cnVal);
        cnVal = NULL;
    }
    if(cpVal)
    {
        free(cpVal);
        cpVal = NULL;
    }
}

void YPriMessageRecv(char *msg, uint16_t length)
{
    uint16_t i;
    char *msgStart = NULL;
    char *contents = NULL;
    char *crc = NULL;
    uint16_t crcValue;
    uint16_t contentLen;

    SysPrintf("Msg recv: %s\n", msg);
    
    for(i = 0; i < length; i++)
    {
        if(msg[i] == '#' && msg[i + 1] == '#')
        {
            msgStart = &msg[i];
            break;
        }
    }

    if(msgStart)
    {
        contentLen = (uint16_t)(uint32_t)strtol(&msgStart[2], NULL, 10);
        contents = &msgStart[2 + 4];
        crc = &msgStart[2 + 4 + contentLen]; //包头2B + 数据段长度4B + 数 据 段
        crcValue = (uint16_t)(uint32_t)strtol(crc, NULL, 16);
        if(crcValue == crc16((uint8_t *)contents, contentLen))
        {
            msgParse(contents, contentLen);
        }
        else
        {
            SysLog("crc error!");
        }
    }
}

void YPriPostData(uint16_t msgID, const char *qn, const char *value)
{
    char *buff = NULL;

    SysLog("");
    if(qn != NULL)
    {
        buff = malloc(strlen(qn) + strlen(value) + 16);
        if(buff)
        {
            sprintf(buff, "QN=%s;%s", qn, value);
            frameSend(YPRI_ST_AIR_QUALITY, msgID, buff, false);
            free(buff);
        }
    }
    else
    {
        frameSend(YPRI_ST_AIR_QUALITY, msgID, value, false);
    }
}

void YPriHeatbeatSend(void)
{
    char *hb = "##0008CN=3023;2f51\r\n";
    SysLog("");
    g_sendFunc(hb, strlen(hb));
}

//请求授时
void YPriRequestTiming(void)
{
    SysLog("");
    frameSend(YPRI_ST_AIR_QUALITY, YP_CMD_TIMING, "", true);
}

//上报异常
void YPriErrorReport(uint8_t errNum[], uint8_t num)
{
    uint8_t i, len = 0;
    char buff[128] = "ErrorCode=[";
    char *err = &buff[strlen("ErrorCode=[")];

    for(i = 0; i < num - 1; i++)
    {
        len += sprintf(err + len, "%d,", errNum[i]);
    }
    sprintf(err + len, "%d];", errNum[num - 1]);
    
    frameSend(YPRI_ST_AIR_QUALITY, YP_CMD_ERROR, buff, true);
}

//上线通知
void YPriLoginNotice(uint32_t interval, uint8_t err[], uint8_t errlen)
{
    char buff[256];
    char *errcode;
    uint8_t i, len = 0;
    
    SysLog("");
    //ICCID=898600810906f8048812;RtdInterval=60;FMW-Type=yumair01;FMW-Version=1.0.0.1
    
    if(errlen)
    {
        sprintf(buff, "ICCID=%s;RtdInterval=%d;FMW-Type=%s;FMW-Version=%s;ErrorCode=[", SysGetUUID(), \
                                            interval, SysGetDevType(), SysGetVersion());
        errcode = &buff[strlen(buff)];
        for(i = 0; i < errlen - 1; i++)
        {
            len += sprintf(errcode + len, "%d,", err[i]);
        }
        sprintf(errcode + len, "%d];", err[errlen - 1]);
    }
    else
    {
        sprintf(buff, "ICCID=%s;RtdInterval=%d;FMW-Type=%s;FMW-Version=%s;ErrorCode=[]", SysGetUUID(), \
                                            interval, SysGetDevType(), SysGetVersion());
    }
    
    frameSend(YPRI_ST_AIR_QUALITY, YP_CMD_ONLINE, buff, true);
}

//上传数据
void YPriPropertiesPost(void)
{
    //char buff[300] = {0};
    char *buff = NULL;

    buff = propertyToText();
    frameSend(YPRI_ST_AIR_QUALITY, YP_CMD_DATA_REPORT, buff, false);
    free(buff);
    
}

//设置属性值
void YPriPropertySet(const char *name, const char *value, uint32_t flagValue)
{
    YPriProperty_t *node;
    
    VTListForeach(&g_property, node)
    {
        if(strcmp(node->name, name) == 0)
        {
            if(node->value)
            {
                free(node->value);
                node->value = NULL;
            }
            node->value = malloc(strlen(value) + 1);
            if(node->value)
            {
                strcpy(node->value, value);
            }
            if(node->flag.name)
            {
                node->flag.value = flagValue;
            }
            //SysPrintf("set %s : value:%s,%d\n", name, value, flagValue);
            break;
        }
    }
}

//注册属性
void YPriPropertyRegister(const char *name, const char *flagName)
{
    YPriProperty_t *property = (YPriProperty_t *)malloc(sizeof(YPriProperty_t));

    if(property)
    {
        property->name = malloc(strlen(name) + 1);
        if(property->name)
        {
            strcpy(property->name, name);
        }
        property->value = NULL;

        if(flagName)
        {
            property->flag.name = malloc(strlen(flagName) + 1);
            if(property->flag.name)
            {
                strcpy(property->flag.name, flagName);
            }
            property->flag.value = 0;
        }

        //SysPrintf("register:%s\n", name);
        VTListAdd(&g_property, property);
    }
}

#if 0
void YPriMessageTypeRegister(uint16_t msgID, YPriMsgType_t type)
{
    YPriMsg_t *msg = (YPriMsg_t *)malloc(sizeof(YPriMsg_t));

    if(msg)
    {
        msg->id = msgID;
        msg->type = type;
        VTListAdd(&g_msgInfo, msg);
    }
}
#endif

void YPriCallbackRegister(YPriDataSend_cb sendHandle, YPriEventHandle_cb eventHandle)
{
    g_sendFunc = sendHandle;
    g_eventHandle = eventHandle;
}

void YPriPoll(void)
{
    msgRetryHandle();
}

void YPriInitialize(void)
{
    VTListInit(&g_property);
    //VTListInit(&g_msgInfo);
    VTListInit(&g_msgRetry);
}

