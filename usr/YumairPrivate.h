#ifndef YUMAIR_PRIVATE_H
#define YUMAIR_PRIVATE_H

#include "Sys.h"

//cmd id
//device send
#define YP_CMD_ONLINE   3021 //�ֳ�������֪ͨ
#define YP_CMD_ERROR    3022 //�ֳ����ϱ��쳣
#define YP_CMD_TIMING   3020 //������λ����ʱ
#define YP_CMD_DATA_REPORT  2011 //�ֳ����ϴ�����
#define YP_CMD_DEV_HEATBEAT 3023

//server send
#define YP_CMD_HEATBEAT_ACK 3024
#define YP_CMD_SET_TIME 1012 //�����ֳ���ʱ��
#define YP_CMD_RESET    3015 //�����ֳ���
#define YP_CMD_GET_ARGS 3016 //��ȡ\�ϴ��ֳ�������������
#define YP_CMD_SET_ARGS 3017 //�����ֳ�������������
#define YP_CMD_OTA      3018 //�����ֳ����̼�
#define YP_CMD_GET_LOCATION 3019 //��ȡ\�ϴ��ֳ���GPS����
#define YP_CMD_SET_REPORT_INTERVAL 1062 //����ʵʱ�����ϱ����
#define YP_CMD_GET_REPORT_INTERVAL 1061 //��ȡ\�ϴ�ʵʱ�����ϱ����

//Ӧ��ͽ��
#define YP_CMD_REQUEST_ACK    9011 //����Ӧ��
#define YP_CMD_OPT_RESULT     9012 //����ִ�н��
#define YP_CMD_NOTICE_ACK     9013 //֪ͨӦ��

typedef enum
{
    OPT_RESULT_SUCCESS = 1,   //ִ�гɹ�
    OPT_RESULT_FAILED = 2,    //ִ��ʧ�ܣ�δ֪ԭ��
    OPT_RESULT_NO_DATA = 100, //û������
}YPriOptResult_t;

typedef enum
{
    REQUEST_RETURN_READY = 1, //׼��ִ������
    REQUEST_RETURN_REJECT,    //���󱻾ܾ�
    REQUEST_RETURN_PWD_ERR,   //�������
}YPriRequestReturn_t;

typedef enum
{
    MSG_TYPE_NONE = 0,
    MSG_TYPE_REPORT, //�ϴ�����,�ϱ�ʵʱ����
    MSG_TYPE_NOTICE,     //֪ͨ����
    MSG_TYPE_REQUEST,    //��������
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

