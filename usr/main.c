#include "Sys.h"

int main(void)
{
    SysInitialize();

    while(1)
    {
        SysPoll();
    }
//    return 0;
}

