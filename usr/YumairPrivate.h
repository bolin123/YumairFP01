#ifndef YUMAIR_PRIVATE_H
#define YUMAIR_PRIVATE_H

#include "Sys.h"

//cmd id
//device send
#define YP_CMD_ONLINE   3021 //现场机上线通知
#define YP_CMD_ERROR    3022 //现场机上报异常
#define YP_CMD_TIMING   3020 //请求上位机对时
#define YP_CMD_DATA_REPORT  2011 //现场机上传数据
#define YP_CMD_DEV_HEATBEAT 3023

//server send
#define YP_CMD_HEATBEAT_ACK 3024
#define YP_CMD_SET_TIME 1012 //设置现场机时间
#define YP_CMD_RESET    3015 //重启现场机
#define YP_CMD_GET_ARGS 3016 //读取\上传现场机传感器参数
#define YP_CMD_SET_ARGS 3017 //设置现场机传感器参数
#define YP_CMD_OTA      3018 //升级现场机固件
#define YP_CMD_GET_LOCATION 3019 //读取\上传现场机GPS数据
#define YP_CMD_SET_REPORT_INTERVAL 1062 //设置实时数据上报间隔
#define YP_CMD_GET_REPORT_INTERVAL 1061 //读取\上传实时数据上报间隔

//应答和结果
#define YP_CMD_REQUEST_ACK    9011 //请求应答
#define YP_CMD_OPT_RESULT     9012 //操作执行结果
#define YP_CMD_NOTICE_ACK     9013 //通知应答

typedef enum
{
    OPT_RESULT_SUCCESS = 1,   //执行成功
    OPT_RESULT_FAILED = 2,    //执行失败，未知原因
    OPT_RESULT_NO_DATA = 100, //没有数据
}YPriOptResult_t;

typedef enum
{
    REQUEST_RETURN_READY = 1, //准备执行请求
    REQUEST_RETURN_REJECT,    //请求被拒绝
    REQUEST_RETURN_PWD_ERR,   //密码错误
}YPriRequestReturn_t;

typedef enum
{
    MSG_TYPE_NONE = 0,
    MSG_TYPE_REPORT, //上传命令,上报实时数据
    MSG_TYPE_NOTICE,     //通知命令
    MSG_TYPE_REQUEST,    //请求命令
}YPriMsgType_t;

typedef enum
{
    YPRI_EVENT_REQUEST = 0,
    YPRI_EVENT_REQUEST_VALUE,
    YPRI_EVENT_RECV_ACK,
    YPRI_EVENT_HEARTBEAT,
}YPriEvent_t;

typedef struct
{
    uint16_t mid;
    char *value;
    char *qn;
}YPriSetMsg_t;

typedef int (* YPriEventHandle_cb)(YPriEvent_t event, void *args);
typedef void (* YPriDataSend_cb)(const char *data, uint16_t len);

void YPriPostData(uint16_t msgID, const char *qn, const char *value);

void YPriLoginNotice(uint32_t interval, uint8_t err[], uint8_t errlen);
void YPriErrorReport(uint8_t errNum[], uint8_t len);
void YPriOptResultSend(const char *qn, YPriOptResult_t result);
void YPriHeatbeatSend(void);

void YPriRequestTiming(void);
void YPriMessageRecv(char *msg, uint16_t length);
void YPriPropertiesPost(void);
void YPriPropertySet(const char *name, const char *value, uint32_t flagValue);
void YPriPropertyRegister(const char *name, const char *flagName);

void YPriCallbackRegister(YPriDataSend_cb sendHandle, YPriEventHandle_cb eventHandle);
void YPriInitialize(void);
void YPriPoll(void);

#endif

