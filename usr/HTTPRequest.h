#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include "Sys.h"
#include "VTList.h"

typedef enum
{
    HTTP_REQ_METHOD_POST,
    HTTP_REQ_METHOD_GET,
}HTTPRequestMethod_t;

typedef enum
{
    HTTP_REQ_ERROR_NONE = 0,
    HTTP_REQ_ERROR_FAIL,
    HTTP_REQ_ERROR_SUCCESS,
}HTTPRequestError_t;

typedef struct HTTPParam_st
{
    char *key;
    char *value;
    VTLIST_ENTRY(struct HTTPParam_st);
}HTTPParam_t;

typedef struct HTTPRequest_st HTTPRequest_t;

typedef void (*HTTPRequestDataRecvCallback_t)(HTTPRequest_t *request, const uint8_t *data, uint16_t len, HTTPRequestError_t error);


struct HTTPRequest_st
{
    char *url;
    char *host;
    uint16_t port;

    HTTPRequestMethod_t method;
    HTTPParam_t params;
    char *data;

    //
    HTTPRequestDataRecvCallback_t dataRecvCb;
    bool hostIsDomain : 1;

    void *userData;

    bool hasStart;
    SysTime_t validTime;

    //
    char *headerBuf;
    uint16_t headerBufCount;
    uint8_t rnCount;

    //
    uint16_t chunkLen;
    uint8_t chunkRNCount;


    uint32_t respContentDataCount;
    uint32_t respContentLength;

    VTLIST_ENTRY(struct HTTPRequest_st);
};



HTTPRequest_t *HTTPRequestCreate(const char *url, HTTPRequestMethod_t method);

void HTTPRequestDestroy(HTTPRequest_t *request);

void HTTPRequestSetData(HTTPRequest_t *request, const char *data);
void HTTPRequestAddParam(HTTPRequest_t *request, const char *key, const char *value);
void HTTPRequestStart(HTTPRequest_t *request);

#endif // HTTP_REQUEST_H



