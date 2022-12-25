#ifndef _ORBIS_MSG_H_
#define _ORBIS_MSG_H_
#pragma once


#include <stdint.h>
#include <string.h>

#define ORBIS_COMMON_DIALOG_MAGIC_NUMBER 0xC0D1A109


typedef enum OrbisCommonDialogStatus {
	ORBIS_COMMON_DIALOG_STATUS_NONE				= 0,
	ORBIS_COMMON_DIALOG_STATUS_INITIALIZED		= 1,
	ORBIS_COMMON_DIALOG_STATUS_RUNNING			= 2,
	ORBIS_COMMON_DIALOG_STATUS_FINISHED			= 3
} OrbisCommonDialogStatus;

typedef enum OrbisCommonDialogResult {
	ORBIS_COMMON_DIALOG_RESULT_OK					= 0,
	ORBIS_COMMON_DIALOG_RESULT_USER_CANCELED		= 1,
} OrbisCommonDialogResult;

typedef struct OrbisCommonDialogBaseParam {
	size_t size;
	uint8_t reserved[36];
	uint32_t magic;
} OrbisCommonDialogBaseParam __attribute__ ((__aligned__(8)));


static inline void _sceCommonDialogSetMagicNumber( uint32_t* magic, const OrbisCommonDialogBaseParam* param )
{
	*magic = (uint32_t)( ORBIS_COMMON_DIALOG_MAGIC_NUMBER + (uint64_t)param );
}

static inline void _sceCommonDialogBaseParamInit(OrbisCommonDialogBaseParam *param)
{
	memset(param, 0x0, sizeof(OrbisCommonDialogBaseParam));
	param->size = (uint32_t)sizeof(OrbisCommonDialogBaseParam);
	_sceCommonDialogSetMagicNumber( &(param->magic), param );
}


// Empty Comment
void sceCommonDialogInitialize();
// Empty Comment
void sceCommonDialogIsUsed();


#include <stdint.h>


#ifdef __cplusplus 
extern "C" {
#endif


#define ORBIS_SYSMODULE_MESSAGE_DIALOG			0x00a4
#define ORBIS_MSG_DIALOG_MODE_USER_MSG				(1)
#define ORBIS_MSG_DIALOG_BUTTON_TYPE_WAIT				(5)
#define ORBIS_MSG_DIALOG_BUTTON_TYPE_OK				(0)

typedef struct OrbisMsgDialogButtonsParam {
	const char *msg1;					
	const char *msg2;					
	char reserved[32];					
} OrbisMsgDialogButtonsParam;

typedef struct OrbisMsgDialogUserMessageParam {
	int32_t buttonType;				
	int :32;										
	const char *msg;								
	OrbisMsgDialogButtonsParam *buttonsParam;		
	char reserved[24];								
													
} OrbisMsgDialogUserMessageParam;

typedef struct OrbisMsgDialogSystemMessageParam {
	int32_t sysMsgType;		
	char reserved[32];								
} OrbisMsgDialogSystemMessageParam;

typedef struct OrbisMsgDialogProgressBarParam {
	int32_t barType;			
	int :32;										
	const char *msg;								
	char reserved[64];								
} OrbisMsgDialogProgressBarParam;



typedef struct OrbisMsgDialogParam {
	OrbisCommonDialogBaseParam baseParam; 	
	size_t size;									
	int32_t mode;							
	int :32;	
	OrbisMsgDialogUserMessageParam *userMsgParam;	
	OrbisMsgDialogProgressBarParam *progBarParam;		
	OrbisMsgDialogSystemMessageParam *sysMsgParam;	
	int32_t userId;					
	char reserved[40];								
	int :32;										
} OrbisMsgDialogParam;



typedef struct OrbisMsgDialogResult {
	int32_t mode;							
													
	int32_t result;									
													
	int32_t buttonId;					
													
	char reserved[32];								
													
} OrbisMsgDialogResult;


static inline
void OrbisMsgDialogParamInitialize(OrbisMsgDialogParam *param)
{
	memset( param, 0x0, sizeof(OrbisMsgDialogParam) );

	_sceCommonDialogBaseParamInit( &param->baseParam );
	param->size = sizeof(OrbisMsgDialogParam);
}

// Initialize the message dialog. Should be called before trying to use the
// message dialog.
int32_t sceMsgDialogInitialize(void);

// Display the message dialog.
int32_t sceMsgDialogOpen(const OrbisMsgDialogParam *param);

// Get the result of the message dialog after the user closes the dialog.
// This can be used to detect which option was pressed (yes, no, cancel, etc).
int32_t sceMsgDialogGetResult(OrbisMsgDialogResult *result);

// Get the status of the message dialog. This can be used to check if a
// message dialog is initialized, is being displayed, or is finished.
OrbisCommonDialogStatus sceMsgDialogGetStatus();

// Update the current status of the message dialog.
OrbisCommonDialogStatus sceMsgDialogUpdateStatus(void);

// Close the message dialog.
int32_t sceMsgDialogClose(void);

// Terminate the message dialog. Should be called when all message dialog
// operations are finished.
int32_t sceMsgDialogTerminate(void);

#endif

#ifdef __cplusplus
}
#endif