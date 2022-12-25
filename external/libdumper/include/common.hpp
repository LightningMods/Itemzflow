// Copyright (c) 2021 Al Azif
// License: GPLv3

#ifndef COMMON_HPP_
#define COMMON_HPP_

#include <filesystem>
#include <sstream>
#include <stdexcept>
#include "log.h"
#include <ps4sdk.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x4000
#endif

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

typedef enum orbisCommonDialogStatus {
	COMMON_DIALOG_STATUS_NONE = 0,
	COMMON_DIALOG_STATUS_INITIALIZED = 1,
	COMMON_DIALOG_STATUS_RUNNING = 2,
	COMMON_DIALOG_STATUS_FINISHED = 3
} orbisCommonDialogStatus;


extern "C" {
	extern orbisCommonDialogStatus sceMsgDialogUpdateStatus(void);
	extern int32_t sceMsgDialogProgressBarSetMsg(int target, const char* barMsg);
	extern int32_t sceMsgDialogProgressBarSetValue(int target, int rate);
}

void ProgUpdate(int prog, const char* fmt, ...);

#endif // COMMON_HPP_
