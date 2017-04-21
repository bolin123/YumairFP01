#include "HTTPRequest.h"
#include "GPRS.h"

#define CONTENT_LENGTH_FLAG "Content-Length: "

static HTTPRequest_t *g_httpRequest = NULL;
static GPRSTcpSocket_t *g_socket = NULL;

static void handleEnd(HTTPRequest_t *request, bool success)
{
    request->hasStart = false;
    GPRSTcpClose(g_socket);
    if(request->dataRecvCb)
    {
        request->dataRecvCb(request, NULL, 0, success ? HTTP_REQ_ERROR_SUCCESS : HTTP_REQ_ERROR_FAIL);
    }
    
}

static bool parseRequestURL(HTTPRequest_t *request)
{
    char *p;

    const char *hostStart = request->url;
    p = strstr(hostStart, "/");
    if(!p)
    {
        p = request->url + strlen(request->url);
    }

    char host[300] = {0};
    char port[10] = "80";
    memcpy(host, hostStart, (int)(p - hostStart));

    p = strchr(host, ':');
    if(p)
    {
        p[0] = 0;
        p++;
        strcpy(port, p);
    }

    request->host = malloc(strlen(host) + 1);
    strcpy(request->host, host);
    request->port = atoi(port);
    SysLog("host %s:%s", request->host, port);

    return true;
}

HTTPRequest_t *HTTPRequestCreate(const char *url, HTTPRequestMethod_t method)
{
    HTTPRequest_t *request = malloc(sizeof(HTTPRequest_t));
    SysLog("request:%p", request);

    request->method = method;
    request->url = malloc(strlen(url) + 1);
	request->headerBuf = malloc(512);

    if(strstr(url, "http://"))
    {
        strcpy(request->url, url + 7);
    }
    else
    {
        strcpy(request->url, url);
    }

    VTListInit(&request->params);
    request->host = NULL;
    request->dataRecvCb = NULL;
    request->data = NULL;

    if(parseRequestURL(request))
    {
        SysLog("");
        return request;
    }
    else
    {
        HTTPRequestDestroy(request);
        return NULL;
    }
}

static void destroyParam(HTTPParam_t *param)
{
    free(param->key);
    free(param->value);
    free(param);
}

void HTTPRequestDestroy(HTTPRequest_t *request)
{
    free(request->url);
    SysLog("free(request->url)");

    GPRSTcpRelease(g_socket);
    g_socket = NULL;

    HTTPParam_t *param;
    HTTPParam_t *lastParam = NULL;

	free(request->headerBuf);

    if(request->data)
    {
        free(request->data);
    }

    VTListForeach(&request->params, param)
    {
        if(lastParam)
        {
            destroyParam(lastParam);
        }
        lastParam = param;
    }
    if(lastParam)
    {
        destroyParam(lastParam);
    }

    SysLog("destroyParam(param)");


    if(request->host)
    {
        free(request->host);
        request->host = NULL;
    }

    SysLog("free(request->host)");


    free(request);
    g_httpRequest = NULL;
}

void HTTPRequestAddParam(HTTPRequest_t *request, const char *key, const char *value)
{
    HTTPParam_t *param = malloc(sizeof(HTTPParam_t));
    param->key = malloc(strlen(key) + 1);
    param->value = malloc(strlen(value) + 1);
    strcpy(param->key, key);
    strcpy(param->value, value);
    VTListAdd(&request->params, param);
}

static const char *methodToStr(HTTPRequestMethod_t method)
{
    switch(method)
    {
        case HTTP_REQ_METHOD_POST:
            return "POST";

        case HTTP_REQ_METHOD_GET:
            return "GET";
    }
		return "POST";
}

static void connectCb(uint8_t id, bool connectOk)
{
    HTTPRequest_t *request = g_httpRequest;

    SysLog("req:%s result:%d", request->url, connectOk);

    if(!connectOk)
    {
        handleEnd(request, false);
        return;
    }

    request->rnCount = 0;
    request->headerBufCount = 0;
    request->respContentLength = 0;

    char reqData[300] = {0};

    char *path = strstr(request->url, "/");
    if(path == NULL)
    {
        path = "/";
    }
    sprintf(reqData, "%s %s", methodToStr(request->method), strstr(request->url, "/"));
    strcat(reqData, " HTTP/1.1\r\n");
    strcat(reqData, "Host: ");
    strcat(reqData, request->host);

    if(request->port != 80)
    {
        char tmp[10];
        strcat(reqData, ":");
        sprintf(tmp, "%d", request->port);
        strcat(reqData, tmp);
    }
    strcat(reqData, "\r\n");
    strcat(reqData, "Connection: Keep-Alive\r\n");

    strcat(reqData, "Content-Type: application/x-www-form-urlencoded\r\n");
    uint16_t contentLen = 0;

    bool hasParamBefore = false;;
    HTTPParam_t *param;

    //计算 Content-Length
    //带数据
    if(request->data)
    {
        contentLen = strlen(request->data);
    }
    //带参数
    else
    {
        VTListForeach(&request->params, param)
        {
            if(hasParamBefore)
            {
                contentLen++;
            }

            contentLen = contentLen + strlen(param->key) + strlen(param->value) + 1;
            hasParamBefore = true;
        }
    }

    sprintf(reqData + strlen(reqData), "Content-Length: %d\r\n", contentLen);
    strcat(reqData, "\r\n");

    if(request->data)
    {
        strcat(reqData, request->data);
    }
    else
    {
        hasParamBefore = false;
        VTListForeach(&request->params, param)
        {
            if(hasParamBefore)
            {
                strcat(reqData, "&");
            }

            strcat(reqData, param->key);
            strcat(reqData, "=");
            strcat(reqData, param->value);
            hasParamBefore = true;
        }
    }

    SysLog("request\n%s", reqData);
    GPRSTcpSend(g_socket, (uint8_t *)reqData, strlen(reqData));
}

static void disconnectCb(uint8_t id)
{
    if(g_httpRequest)
    {
        SysLog("req:%s", g_httpRequest->url);
        handleEnd(g_httpRequest, false);
    }
}


#define CHAR_DIGIT(ch) ((ch) - '0' < 10 ? (ch) - '0' : (ch) - 'a' + 10)

static uint32_t parseHexNumStr(const char *str)
{
    uint8_t len = strlen(str);
    uint8_t i;
    uint32_t num = 0;
    char ch;
    for(i = 0; i < len; i++)
    {
        ch = str[i];
        num <<= 4;

        //转小写
        if(ch >= 'A' && ch <= 'Z')
        {
            ch += ('a' - 'A');
        }

        num |= CHAR_DIGIT(ch);
    }
    return num;
}

static void recvCb(uint8_t id, uint8_t *data, uint16_t len)
{
    HTTPRequest_t *request = g_httpRequest;

    uint32_t i;
//    SysLog("");

    request->validTime = SysTime();

    //\r\n\r\n表示HTTP头结束
    //接收header中
    if(request->rnCount < 4)
    {
        for(i = 0; i < len; i++)
        {
            request->headerBuf[request->headerBufCount++] = data[i];
            request->headerBuf[request->headerBufCount] = 0;

            if(data[i] == '\r' || data[i] == '\n')
            {
                request->rnCount++;
            }
            else
            {
                request->rnCount = 0;
            }

            if(request->rnCount == 4)
            {
//                SysLog("%s recv header:%s", request->url, request->headerBuf);

                char *p = strstr((char *)request->headerBuf, CONTENT_LENGTH_FLAG);
                if(p)
                {
                    p += strlen(CONTENT_LENGTH_FLAG);
                    char lenStr[10] = {0};
                    char *p2 = strchr(p, '\r');
                    memcpy(lenStr, p, p2 - p);
                    request->respContentLength = atoi(lenStr);
                    request->respContentDataCount = 0;
                    SysLog("respContentLength %d", request->respContentLength);
                }
                else
                {
                    request->chunkLen = 0;
                    request->chunkRNCount = 0;
                    request->headerBufCount = 0;
                }

                data = data + i + 1;
                len = len - i - 1;
                break;
            }
        }
    }

    if(request->rnCount < 4)
    {
        return;
    }

    //指定Content-Length
    if(request->respContentLength != 0)
    {
//        SysLog("len:%d count:%d contentLen:%d", len, request->respContentDataCount, request->respContentLength);
        request->respContentDataCount += len;

        if(request->dataRecvCb)
        {
            request->dataRecvCb(request, data, len, HTTP_REQ_ERROR_NONE);
        }

        if(request->respContentDataCount >= request->respContentLength)
        {
            SysLog("success.");
            handleEnd(request, true);
        }
    }
    //transfer-encoding:chunked，chunk格式:[hex]\r\n[Data]\r\n[...]0\r\n
    else
    {
        for(i = 0; i < len; i++)
        {
//            SysPrintf("%c", data[i]);

            //获取chunk长度
            if(request->chunkLen == 0)
            {
                if(data[i] == '\r' || data[i] == '\n')
                {
                    request->chunkRNCount++;
                }
                else
                {
                    request->chunkRNCount = 0;
                    request->headerBuf[request->headerBufCount++] = data[i];
                    request->headerBuf[request->headerBufCount] = '\0';
                }

                if(request->chunkRNCount == 2)
                {
                    request->chunkRNCount = 0;
                    request->headerBufCount = 0;
                    request->chunkLen = parseHexNumStr(request->headerBuf);
                    SysLog("chunk len:%d[%s]", request->chunkLen, request->headerBuf);
                    if(request->chunkLen == 0)
                    {
                        handleEnd(request, true);
                        return;
                    }
                    else
                    {
                        //chunk结尾的\r\n
                        request->chunkLen += 2;
                    }
                }
            }
            else
            {
                //屏蔽chunk结尾的\r\n
                if(request->chunkLen > 2)
                {
                    request->dataRecvCb(request, &data[i], 1, HTTP_REQ_ERROR_NONE);
                }
                request->chunkLen--;
            }
        }
    }
}


void HTTPRequestSetData(HTTPRequest_t *request, const char *data)
{
    if(request->data)
    {
        free(request->data);
    }
    request->data = malloc(strlen(data) + 1);
    strcpy(request->data, data);
}
/*
static void httpNetEventHandle(GPRSEvent_t event, void *args)
{    
    SysLog("");
    switch(event)
    {
        case GEVENT_TCP_CONNECT_OK:
            connectCb(true);
            break;
        case GEVENT_TCP_CONNECT_FAIL:
            connectCb(false);
            break;
        case GEVENT_TCP_CLOSED:
            disconnectCb();
            break;
        default:
            break;
    }
}
*/
void HTTPRequestStart(HTTPRequest_t *request)
{
    g_httpRequest = request;

    request->hasStart = true;
    request->validTime = SysTime();
    SysLog("start:%s", request->url);

    if(g_socket == NULL)
    {
        g_socket = GPRSTcpCreate();
    }
    
    if(g_socket)
    {
        g_socket->connectCb = connectCb;
        g_socket->recvCb = recvCb;
        g_socket->disconnetCb = disconnectCb;
        GPRSTcpConnect(g_socket, request->host, request->port);
    }
    
}

void HTTPRequestPoll(void)
{
    HTTPRequest_t *request;
    //超时

    if(g_httpRequest != NULL
        && g_httpRequest->hasStart 
        && SysTimeHasPast(g_httpRequest->validTime, 20000))
    {
        SysLog("http request timeout");
        handleEnd(request, false);
    }
    
}


