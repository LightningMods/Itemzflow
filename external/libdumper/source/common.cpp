#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "common.hpp"

void ProgUpdate(int prog, const char* fmt, ...)
{
    char buff[300];
    memset(&buff[0], 0, sizeof(buff));

    va_list args;
    va_start(args, fmt);
    vsnprintf(&buff[0], 299, fmt, args);
    va_end(args);

   if (sceMsgDialogUpdateStatus() == COMMON_DIALOG_STATUS_RUNNING)
   {
        sceMsgDialogProgressBarSetMsg(0, (const char*)buff);
        sceMsgDialogProgressBarSetValue(0, prog);
    }
}
