#include <stdio.h>
#include <ps4sdk.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include "log.h"
#include <dirent.h>
#include <user_mem.h> 
#include <time.h>
#include <sqlite3.h>
#include "lang.h"
#include "utils.h"
#include <filesystem>
#include <fstream>

typedef uint32_t OrbisKernelModule;

#define SYS_dynlib_get_info         593
#define SYS_dynlib_get_info_ex      608

#define SYS_PROC_ALLOC              1
#define SYS_PROC_FREE               2
#define SYS_PROC_PROTECT            3
#define SYS_PROC_VM_MAP             4
#define SYS_PROC_INSTALL            5
#define SYS_PROC_CALL               6
#define SYS_PROC_ELF                7
#define SYS_PROC_INFO               8
#define SYS_PROC_THRINFO            9

#define SYS_CONSOLE_CMD_REBOOT      1
#define SYS_CONSOLE_CMD_PRINT       2
#define SYS_CONSOLE_CMD_JAILBREAK   3
#define SYS_CONSOLE_CMD_ISLOADED    6

#define ORBIS_SYSMODULE_INTERNAL_SAVE_DATA 0x8000000F // libSceSaveData

#define sys_proc_read_mem(p, a, d, l)       sys_proc_rw(p, a, d, l, 0)
#define sys_proc_write_mem(p, a, d, l)      sys_proc_rw(p, a, d, l, 1)

#define ORBIS_SAVE_DATA_MOUNT_MODE_RDONLY			1		//Read-only
#define ORBIS_SAVE_DATA_MOUNT_MODE_RDWR				2		//Read/write-enabled
#define ORBIS_SAVE_DATA_MOUNT_MODE_CREATE			4		//Create new (error if save data directory already exists)
#define ORBIS_SAVE_DATA_MOUNT_MODE_DESTRUCT_OFF		8		//Turn off corrupt flag (not recommended)
#define ORBIS_SAVE_DATA_MOUNT_MODE_COPY_ICON		16		//Copy save_data.png in package as icon when newly creating save data
#define ORBIS_SAVE_DATA_MOUNT_MODE_CREATE2			32		//Create new (mount save data directory if it already exists)

#define ORBIS_SAVE_DATA_PARAM_TYPE_ALL				0		//Update all parameters
#define ORBIS_SAVE_DATA_PARAM_TYPE_TITLE			1		//Update save data title only
#define ORBIS_SAVE_DATA_PARAM_TYPE_SUB_TITLE		2		//Update save data subtitle only
#define ORBIS_SAVE_DATA_PARAM_TYPE_DETAIL			3		//Update save data detailed information only
#define ORBIS_SAVE_DATA_PARAM_TYPE_USER_PARAM		4		//Update user parameter only

#define ORBIS_SAVE_DATA_MOUNT_MAX_COUNT				16			//Maximum number of instances of save data that can be mounted simultaneously
#define ORBIS_SAVE_DATA_DIRNAME_MAX_COUNT			1024		//Maximum number of save data directories
#define ORBIS_SAVE_DATA_ICON_WIDTH					228			//Icon width
#define ORBIS_SAVE_DATA_ICON_HEIGHT					128			//Icon height
#define ORBIS_SAVE_DATA_ICON_FILE_MAXSIZE			116736		//Maximum size for an icon file
#define ORBIS_SAVE_DATA_BLOCK_SIZE					32768		//Block size (bytes)
#define ORBIS_SAVE_DATA_BLOCKS_MIN2					96			//Minimum number of blocks
#define ORBIS_SAVE_DATA_BLOCKS_MAX 					32768		//Maximum number of blocks
#define ORBIS_SAVE_DATA_MEMORY_MAXSIZE3				33554432	//Maximum size for the data sections of the save data memory (maximum value for the total save data memory size of all users)
#define ORBIS_SAVE_DATA_MEMORY_SETUP_MAX_COUNT		4			//Maximum number of instances of save data memory that can be set up simultaneously
#define ORBIS_SAVE_DATA_MEMORY_DATANUM_MAX_COUNT	5			//Maximum number of data that can be specified at the same time to the data sections of the save data memory

#define ORBIS_SAVE_DATA_TITLE_ID_DATA_SIZE			10			//Save data title ID size
#define ORBIS_SAVE_DATA_DIRNAME_DATA_MAXSIZE		32			//Maximum size for a save data directory name
#define ORBIS_SAVE_DATA_MOUNT_POINT_DATA_MAXSIZE	16			//Maximum size for a mount point name
#define ORBIS_SAVE_DATA_FINGERPRINT_DATA_SIZE		65			//Fingerprint size
#define ORBIS_SAVE_DATA_TITLE_MAXSIZE				128			//Maximum size for a save data title name (NULL-terminated, UTF-8)
#define ORBIS_SAVE_DATA_SUBTITLE_MAXSIZE			128			//Maximum size for a save data subtitle name (NULL-terminated, UTF-8)
#define ORBIS_SAVE_DATA_DETAIL_MAXSIZE				1024		//Maximum size for save data detailed information (NULL-terminated, UTF-8)

int sys_proc_list(struct proc_list_entry* procs, uint64_t* num);
int sys_proc_rw(uint64_t pid, uint64_t address, void* data, uint64_t length, uint64_t write);
int sys_proc_cmd(uint64_t pid, uint64_t cmd, void* data);
int find_process_pid(const char* proc_name, int* pid);
int find_map_entry_start(int pid, const char* entry_name, uint64_t* start);
int sys_console_cmd(uint64_t cmd, void* data);
int check_syscalls();
sfo_context_t * sfo_alloc(void);
void sfo_free(sfo_context_t *context);
int sfo_read(sfo_context_t *context, const char *file_path);
int sfo_write(sfo_context_t *context, const char *file_path);

void sfo_grab(sfo_context_t *inout, sfo_context_t *tpl, int num_keys, const sfo_key_pair_t *keys);
void sfo_patch(sfo_context_t *inout, unsigned int flags);

std::string sfo_get_param_value_string(sfo_context_t *in, const char* param);
int sfo_get_param_value_int(sfo_context_t *in, const char* param);
