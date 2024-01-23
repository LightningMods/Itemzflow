#include "Header.h"
#include <stdarg.h>
#include <MsgDialog.h>
#include "lang.h"
#define ORBIS_TRUE 1

int Confirmation_Msg(const char* msg)
{

    sceMsgDialogTerminate();
    //ds

    sceMsgDialogInitialize();
    OrbisMsgDialogParam param;
    OrbisMsgDialogParamInitialize(&param);
    param.mode = ORBIS_MSG_DIALOG_MODE_USER_MSG;

    OrbisMsgDialogUserMessageParam userMsgParam;
    memset(&userMsgParam, 0, sizeof(userMsgParam));
    userMsgParam.msg = msg;
    userMsgParam.buttonType = 1;
    param.userMsgParam = &userMsgParam;
    // cv   
    if (0 < sceMsgDialogOpen(&param))
        return NO;

    OrbisCommonDialogStatus stat;
    //
    while (1) {
        stat = sceMsgDialogUpdateStatus();
        if (stat == ORBIS_COMMON_DIALOG_STATUS_FINISHED) {

            OrbisMsgDialogResult result;
            memset(&result, 0, sizeof(result));

            sceMsgDialogGetResult(&result);

            return result.buttonId;
        }
    }
    //c
    return NO;
}

int msgok(const char* format, ...)
{
    int ret = 0;
	sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG);

	sceMsgDialogTerminate();

	char buff[1024];
	char buffer[1000];
	memset(buff, 0, 1024);

	va_list args;
	va_start(args, format);
	vsprintf(&buff[0], format, args);
	va_end(args);

	strcpy(&buffer[0], &buff[0]);

	logshit(&buff[0]);

	sceMsgDialogInitialize();
	OrbisMsgDialogParam param;
	OrbisMsgDialogParamInitialize(&param);
	param.mode = ORBIS_MSG_DIALOG_MODE_USER_MSG;

	OrbisMsgDialogUserMessageParam userMsgParam;
	memset(&userMsgParam, 0, sizeof(userMsgParam));
	userMsgParam.msg = buffer;
	userMsgParam.buttonType = ORBIS_MSG_DIALOG_BUTTON_TYPE_OK;
	param.userMsgParam = &userMsgParam;

	if (sceMsgDialogOpen(&param) < 0)
           ret = -1;	
	

	OrbisCommonDialogStatus stat;

	while (1)
	{
		stat = sceMsgDialogUpdateStatus();
		if (stat == ORBIS_COMMON_DIALOG_STATUS_FINISHED)
		{
			OrbisMsgDialogResult result;
			memset(&result, 0, sizeof(result));

			if (0 > sceMsgDialogGetResult(&result))
                        ret = -2;

			sceMsgDialogTerminate();
			break;
		}
	}

	return ret;
}

int loadmsg(char* format, ...)
{
	sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG);

	sceMsgDialogInitialize();

	char buff[1024];
	memset(&buff[0], 0, 1024);

	va_list args;
	va_start(args, format);
	vsprintf(&buff[0], format, args);
	va_end(args);

	OrbisMsgDialogButtonsParam buttonsParam;
	OrbisMsgDialogUserMessageParam messageParam;
	OrbisMsgDialogParam dialogParam;

	OrbisMsgDialogParamInitialize(&dialogParam);

	memset(&buttonsParam, 0x00, sizeof(buttonsParam));
	memset(&messageParam, 0x00, sizeof(messageParam));

	dialogParam.userMsgParam = &messageParam;
	dialogParam.mode = ORBIS_MSG_DIALOG_MODE_USER_MSG;

	messageParam.buttonType = ORBIS_MSG_DIALOG_BUTTON_TYPE_WAIT;

	messageParam.msg = buff;

	sceMsgDialogOpen(&dialogParam);

	return 0;
}

