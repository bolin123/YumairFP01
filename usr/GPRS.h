#ifndef GPRS_H
#define GPRS_H

#include "Sys.h"

/*定位与时间信息*/
typedef struct
{
    SysDateTime_t time;
    struct
    {
        float latitude;
        float longitude;
    }location;
}GNSSLocation_t;

/*事件*/
typedef enum
{
    GEVENT_POWER_ON = 0,    //GPRS上电
    GEVENT_POWER_OFF,       //GPRS掉电
    GEVENT_GNSS_START,      //GPS开启
    GEVENT_GNSS_STOP,       //GPS关闭
    GEVENT_GNSS_FIXED,      //GPS锁定信息
    GEVENT_GPRS_STATUS_CHANGED, //GPRS状态改变
}GPRSEvent_t;

/*GPRS状态*/
typedef enum
{
    GPRS_STATUS_NONE = 0,    //默认状态，未建立通信
    GPRS_STATUS_INITIALIZED, //通信正常
    GPRS_STATUS_ACTIVE,      //激活全参数
    GPRS_STATUS_GSM_DONE,    //GSM初始化完成
    GPRS_STATUS_GPRS_DONE,   //GPRS初始化完成
}GPRSStatus_t;

typedef void (* GPRSTcpDisconnect_cb)(uint8_t id);
typedef void (* GPRSTcpConnect_cb)(uint8_t id, bool success);
typedef void (* GPRSTcpReceive_cb)(uint8_t id, uint8_t *data, uint16_t len);
typedef void (* GPRSEventHandle_cb)(GPRSEvent_t event, void *args);

typedef struct
{
    uint8_t socketId;
    bool connected;
    uint16_t dataLen;
    uint16_t recvCount;
    uint8_t *data;
    GPRSTcpConnect_cb connectCb;
    GPRSTcpDisconnect_cb disconnetCb;
    GPRSTcpReceive_cb recvCb;
}GPRSTcpSocket_t;


const char *GPRSGetICCID(void);

/*关闭GPS
* @return: 0=success, -1=fail
*/
int8_t GNSSStop(void);

/*开启GPS
* @return: 0=success, -1=fail
*/
int8_t GNSSStart(void);

/*GPS是否锁定*/
bool GNSSLocationFixed(void);

/*获取定位和时间信息*/
GNSSLocation_t *GNSSGetLocation(void);

/*GPRS事件处理注册
* @handle:事件处理函数
*/
void GPRSEventHandleRegister(GPRSEventHandle_cb handle);

/*设置接收模式(用于升级时连续接收大量数)
* @entry: 
*    true,接收模式(不再发送指令和数据)
*    false,退出接收模式
*/
void GPRSSetRecvMode(bool entry);

/*关闭TCP连接
* @id:连接时返回的id
*/
void GPRSTcpClose(GPRSTcpSocket_t *socket);

/*TCP发送函数
* @id:连接时返回的id, @data:数据，@len:数据长度
* @return:发送数据长度
*/
uint16_t GPRSTcpSend(GPRSTcpSocket_t *socket, uint8_t *data, uint16_t len);

/*建立TCP连接
* @url:服务器地址，@port:端口号，@callback:接收回调函数
* @return:id
*/
int8_t GPRSTcpConnect(GPRSTcpSocket_t *socket, const char *url, uint16_t port);

void GPRSTcpRelease(GPRSTcpSocket_t *socket);

GPRSTcpSocket_t *GPRSTcpCreate(void);


/*是否连接到网络(获取IP地址)
* 
*/
bool GPRSConnected(void);

/*获取当前GPRS状态
* @return:
*   GPRS_STATUS_NONE:无状态，未建立通信
*   GPRS_STATUS_INITIALIZED:通信正常
*   GPRS_STATUS_ACTIVE:激活全功能
*   GPRS_STATUS_GSM_DONE:GSM初始化完成
*   GPRS_STATUS_GPRS_DONE:GPRS初始化完成
*/
GPRSStatus_t GPRSGetStatus(void); 

/*获取CSQ值
* @return: csq value
*/
uint8_t GPRSGetSignalValue(void);

/*关闭GPRS功能
* @return:-1:already stop, -2:busy, 0:ok
*/
int8_t GPRSStop(void);

/*打开GPRS功能
* @return:-1:already start, -2:busy, 0:ok
*/
int8_t GPRSStart(void);

/*初始化函数*/
void GPRSInitialize(void);

/*循环执行函数*/
void GPRSPoll(void);

#endif

