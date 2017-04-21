#ifndef MD5_H
#define MD5_H

#include "Sys.h"

typedef struct
{
    unsigned int count[2];
    unsigned int state[4];
    unsigned char buffer[64];
}MD5_CTX;

void SysMD5Init(MD5_CTX *context);
void SysMD5Update(MD5_CTX *context,unsigned char *input,unsigned int inputlen);
void SysMD5Final(MD5_CTX *context,unsigned char digest[16]);

#endif // MD5_H
