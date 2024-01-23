#include <stdio.h>
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
#include "sfo.hpp"
#include "utils.h"
#include "game_saves.h"

/*    SAVE DATA CODE LIFTED FROM HERE
* ******************************************
* **https://github.com/bucanero/apollo-ps4**
* ******************************************
* ******************************************/
 /* Made with info from https://www.psdevwiki.com/ps4/Param.sfo. */
 /* Get updates and Windows binaries at https://github.com/hippie68/sfo. */

#if __has_include("<byteswap.h>")
#include <byteswap.h>
#else
// Replacement function for byteswap.h's bswap_32
uint32_t bswap_32(uint32_t val) {
  val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0x00FF00FF );
  return (val << 16) | (val >> 16);
}
#endif

// Complete param.sfo file structure, 4 parts:
// 1. header
// 2. all entries
// 3. key_table.content (with trailing 4-byte alignment)
// 4. data_table.content

using namespace multi_select_options;
struct header {
    uint32_t magic;
    uint32_t version;
    uint32_t key_table_offset;
    uint32_t data_table_offset;
    uint32_t entries_count;
};

header header;

struct index_table_entry {
    uint16_t key_offset;
    uint16_t param_fmt;
    uint32_t param_len;
    uint32_t param_max_len;
    uint32_t data_offset;
} *entries;

struct table {
    unsigned int size;
    char* content;
} key_table, data_table;

// Finds the param.sfo's offset inside a PS4 PKG file
long int get_ps4_pkg_offset(FILE* file) {
  uint32_t pkg_table_offset;
  uint32_t pkg_file_count;
  fseek(file, 0x00C, SEEK_SET);
  fread(&pkg_file_count, 4, 1, file);
  fseek(file, 0x018, SEEK_SET);
  fread(&pkg_table_offset, 4, 1, file);
  pkg_file_count = bswap_32(pkg_file_count);
  pkg_table_offset = bswap_32(pkg_table_offset);
  struct pkg_table_entry {
    uint32_t id;
    uint32_t filename_offset;
    uint32_t flags1;
    uint32_t flags2;
    uint32_t offset;
    uint32_t size;
    uint64_t padding;
  } pkg_table_entry[pkg_file_count];
  fseek(file, pkg_table_offset, SEEK_SET);
  fread(pkg_table_entry, sizeof (struct pkg_table_entry), pkg_file_count, file);
  for (int i = 0; i < pkg_file_count; i++) {
    if (pkg_table_entry[i].id == 1048576) { // param.sfo ID
      return bswap_32(pkg_table_entry[i].offset);
    }
  }
  log_error("[SFO] Could not find param.sfo in PKG file.");
  return -1;
}

bool load_header(FILE* file) {
    if (fread(&header, sizeof(struct header), 1, file) != 1) {
		log_error("[SFO] Could not read header.");
       return false;
    }
    return true;
}

bool load_entries(FILE* file) {
    unsigned int size = sizeof(struct index_table_entry) * header.entries_count;
    entries = (struct index_table_entry *)malloc(size);
    if (entries == NULL) {
	   log_error("[SFO] Could not allocate %u bytes of memory for index table.");
       return false;
    }
    if (size && fread(entries, size, 1, file) != 1) {
	   log_error("[SFO] Could not read index table entries.");
       return false;
    }
    return true;
}

bool load_key_table(FILE* file) {
    key_table.size = header.data_table_offset - header.key_table_offset;
    key_table.content = (char *)malloc(key_table.size);
    if (key_table.content == NULL) {
       log_error("[SFO] Could not allocate %u bytes of memory for key table.",
            key_table.size);
       return false;
    }
    if (key_table.size && fread(key_table.content, key_table.size, 1, file) != 1) {
		log_error("[SFO] Could not read key table.");
       return false;
    }
    return true;
}

bool load_data_table(FILE* file) {
    if (header.entries_count) {
        data_table.size =
            (entries[header.entries_count - 1].data_offset +
                entries[header.entries_count - 1].param_max_len);
    }
    else {
        data_table.size = 0; // For newly created, empty param.sfo files
    }
    data_table.content = (char*)malloc(data_table.size);
    if (data_table.content == NULL) {
       log_error("[SFO] Could not allocate %u bytes of memory for data table.",
            data_table.size);
       return false;
    }
    if (data_table.size && fread(data_table.content, data_table.size, 1, file) != 1) {
		log_error("[SFO] Could not read data table.");
       return false;
    }
    return true;
}

// Returns a parameter's index table position
int get_index(char* key) {
    for (int i = 0; i < header.entries_count; i++) {
        if (strcmp(key, &key_table.content[entries[i].key_offset]) == 0) {
            return i;
        }
    }
    return -1;
}

void sfo_mem_to_file(char *file_name) {
  FILE *file = fopen(file_name, "wb");
  if (file == NULL) {
    log_error("[SFO] Could not open file \"%s\" for writing.", file_name);
    return;
  }

  // Adjust header's table offsets before saving
  header.key_table_offset = sizeof(struct header) +
    sizeof(struct index_table_entry) * header.entries_count;
  header.data_table_offset = header.key_table_offset + key_table.size;

  if (fwrite(&header, sizeof(struct header), 1, file) != 1) {
    log_error("[SFO] Could not write header to file \"%s\".", file_name);
  }
  if (header.entries_count && fwrite(entries,
    sizeof(struct index_table_entry) * header.entries_count, 1, file) != 1) {
    log_error("[SFO] Could not write index table to file \"%s\".", file_name);
  }
  if (key_table.size && fwrite(key_table.content, key_table.size, 1, file) != 1) {
    log_error("[SFO] Could not write key table to file \"%s\".", file_name);
  }
  if (data_table.size && fwrite(data_table.content, data_table.size, 1, file) != 1) {
    log_error("[SFO] Could not write data table to file \"%s\".", file_name);
  }

  fclose(file);
}

bool extract_sfo_from_pkg(const char* pkg, const char* outdir){
    // CAUTION! This function assumes you checked the pkg magic already
    unlink(outdir); 
    FILE* file = fopen(pkg, "rb"); // TODO: use Open for both next
    if (file == NULL) {
        log_error("Could not open file %s", pkg);
        return false;
    }

     // Get SFO header offset
    fseek(file, get_ps4_pkg_offset(file), SEEK_SET);


    // Load file contents
    if(!load_header(file)){
        log_error("Could not load header");
        fclose(file);
        return false;
    }
    if(!load_entries(file)){
        log_error("Could not load entries table");
        fclose(file);
        return false;
    }
     if(!load_key_table(file)){
        log_error("Could not load data table");
        fclose(file);
        return false;
    }
    if(!load_data_table(file)){
        log_error("Could not load data table");
        fclose(file);
        return false;
    }

    fclose(file);
    
    sfo_mem_to_file((char*)outdir);

    if (entries) free(entries);
    if (key_table.content) free(key_table.content);
    if (data_table.content) free(data_table.content);

    return true;
}

typedef struct
{
    char path[128];
    char* title;
    uint8_t icon_id;
} save_list_t;


typedef void module_patch_cb_t(void* arg, uint8_t* base, uint64_t size);

// custom syscall 107
struct proc_list_entry {
    char p_comm[32];
    int pid;
}  __attribute__((packed));

struct proc_vm_map_entry {
    char name[32];
    uint64_t start;
    uint64_t end;
    uint64_t offset;
    uint16_t prot;
} __attribute__((packed));

typedef struct OrbisKernelModuleSegmentInfo
{
    void* address;
    uint32_t size;
    int32_t prot;
} OrbisKernelModuleSegmentInfo;

typedef struct OrbisKernelModuleInfo
{
    size_t size;
    char name[256];
    OrbisKernelModuleSegmentInfo segmentInfo[4];
    uint32_t segmentCount;
    uint8_t fingerprint[20];
} OrbisKernelModuleInfo;

// custom syscall 109
// SYS_PROC_VM_MAP
struct sys_proc_vm_map_args {
    struct proc_vm_map_entry* maps;
    uint64_t num;
} __attribute__((packed));

// SYS_PROC_ALLOC
struct sys_proc_alloc_args {
    uint64_t address;
    uint64_t length;
} __attribute__((packed));

// SYS_PROC_FREE
struct sys_proc_free_args {
    uint64_t address;
    uint64_t length;
} __attribute__((packed));

// SYS_PROC_PROTECT
struct sys_proc_protect_args {
    uint64_t address;
    uint64_t length;
    uint64_t prot;
} __attribute__((packed));

// SYS_PROC_INSTALL
struct sys_proc_install_args {
    uint64_t stubentryaddr;
} __attribute__((packed));

// SYS_PROC_CALL
struct sys_proc_call_args {
    uint32_t pid;
    uint64_t rpcstub;
    uint64_t rax;
    uint64_t rip;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t r8;
    uint64_t r9;
} __attribute__((packed));

// SYS_PROC_ELF
struct sys_proc_elf_args {
    void* elf;
} __attribute__((packed));

// SYS_PROC_INFO
struct sys_proc_info_args {
    int pid;
    char name[40];
    char path[64];
    char titleid[16];
    char contentid[64];
} __attribute__((packed));

// SYS_PROC_THRINFO
struct sys_proc_thrinfo_args {
    uint32_t lwpid;
    uint32_t priority;
    char name[32];
} __attribute__((packed));

typedef struct orbis_patch {
    uint32_t offset;
    char* data;
    uint32_t size;
} orbis_patch_t;

// SAVEDATA LIBRARY PATCHES (libSceSaveData)
const orbis_patch_t scesavedata_patches_505[] = {
    {0x32998, (char*)"\x00", 1},        // 'sce_' patch
    {0x31699, (char*)"\x00", 1},        // 'sce_sdmemory' patch
    {0x01119, (char*)"\x30", 1},        // '_' patch
    {0, NULL, 0},
};

const orbis_patch_t scesavedata_patches_672[] = {
    {0x00038AE8, (char*)"\x00", 1},        // 'sce_' patch
    {0x000377D9, (char*)"\x00", 1},        // 'sce_sdmemory' patch
    {0x00000ED9, (char*)"\x30", 1},        // '_' patch
    {0, NULL, 0},
};

const orbis_patch_t scesavedata_patches_702[] = {
    {0x00036798, (char*)"\x00", 1},        // 'sce_' patch
    {0x00035479, (char*)"\x00", 1},        // 'sce_sdmemory' patch
    {0x00000E88, (char*)"\x30", 1},        // '_' patch
    {0, NULL, 0},
};

const orbis_patch_t scesavedata_patches_75x[] = {
    {0x000360E8, (char*)"\x00", 1},        // 'sce_' patch
    {0x00034989, (char*)"\x00", 1},        // 'sce_sdmemory' patch
    {0x00000E81, (char*)"\x30", 1},        // '_' patch
    {0, NULL, 0},
};

const orbis_patch_t scesavedata_patches_900[] = {
    {0x00035DA8, (char*)"\x00", 1},        // 'sce_' patch
    {0x00034679, (char*)"\x00", 1},        // 'sce_sdmemory' patch
    {0x00034609, (char*)"\x00", 1},        // by @Ctn
    {0x00036256, (char*)"\x00", 1},        // by @Ctn
    {0x00000FA1, (char*)"\x1F", 1},        // by GRModSave_Username
    {0, NULL, 0},
};

// SHELLCORE PATCHES (SceShellCore)

/* 5.05 patches below are taken from ChendoChap Save-Mounter */
const orbis_patch_t shellcore_patches_505[] = {
    {0xD42843, (char*)"\x00", 1},                        // 'sce_sdmemory' patch
    {0x7E4DC0, (char*)"\x48\x31\xC0\xC3", 4},            //verify keystone patch
    {0x068BA0, (char*)"\x31\xC0\xC3", 3},                //transfer mount permission patch eg mount foreign saves with write permission
    {0x0C54F0, (char*)"\x31\xC0\xC3", 3},                //patch psn check to load saves saves foreign to current account
    {0x06A349, (char*)"\x90\x90", 2},                    // ^
    {0x0686AE, (char*)"\x90\x90\x90\x90\x90\x90", 6},    // something something patches...
    {0x067FCA, (char*)"\x90\x90\x90\x90\x90\x90", 6},    // don't even remember doing this
    {0x067798, (char*)"\x90\x90", 2},                    //nevah jump                    
    {0x0679D5, (char*)"\x90\xE9", 2},                    //always jump
    {0, NULL, 0}
};

/* 6.72 patches below are taken from GiantPluto Save-Mounter */
const orbis_patch_t shellcore_patches_672[] = {
    {0x01600060, (char*)"\x00", 1},                        // 'sce_sdmemory' patch
    {0x0087F840, (char*)"\x48\x31\xC0\xC3", 4},            //verify keystone patch
    {0x00071130, (char*)"\x31\xC0\xC3", 3},                //transfer mount permission patch eg mount foreign saves with write permission
    {0x000D6830, (char*)"\x31\xC0\xC3", 3},                //patch psn check to load saves saves foreign to current account
    {0x0007379E, (char*)"\x90\x90", 2},                    // ^
    {0x00070C38, (char*)"\x90\x90\x90\x90\x90\x90", 6},    // something something patches...
    {0x00070855, (char*)"\x90\x90\x90\x90\x90\x90", 6},    // don't even remember doing this
    {0x00070054, (char*)"\x90\x90", 2},                    //nevah jump                    
    {0x00070260, (char*)"\x90\xE9", 2},                    //always jump
    {0, NULL, 0}
};

/* 7.02 patches below are taken from Joonie Save-Mounter */
const orbis_patch_t shellcore_patches_702[] = {
    {0x0130C060, (char*)"\x00", 1},                        // 'sce_sdmemory' patch
    {0x0083F4E0, (char*)"\x48\x31\xC0\xC3", 4},            //verify keystone patch
    {0x0006D580, (char*)"\x31\xC0\xC3", 3},                //transfer mount permission patch eg mount foreign saves with write permission
    {0x000CFAA0, (char*)"\x31\xC0\xC3", 3},                //patch psn check to load saves saves foreign to current account
    {0x0006FF5F, (char*)"\x90\x90", 2},                    // ^
    {0x0006D058, (char*)"\x90\x90\x90\x90\x90\x90", 6},    // something something patches...
    {0x0006C971, (char*)"\x90\x90\x90\x90\x90\x90", 6},    // don't even remember doing this
    {0x0006C1A4, (char*)"\x90\x90", 2},                    //nevah jump                    
    {0x0006C40C, (char*)"\x90\xE9", 2},                    //always jump
    {0, NULL, 0}
};

/* 7.5x patches below are taken from Joonie Save-Mounter */
const orbis_patch_t shellcore_patches_75x[] = {
    {0x01334060, (char*)"\x00", 1},                        // 'sce_sdmemory' patch
    {0x0084A300, (char*)"\x48\x31\xC0\xC3", 4},            //verify keystone patch
    {0x0006B860, (char*)"\x31\xC0\xC3", 3},                //transfer mount permission patch eg mount foreign saves with write permission
    {0x000C7280, (char*)"\x31\xC0\xC3", 3},                //patch psn check to load saves saves foreign to current account
    {0x0006D26D, (char*)"\x90\x90", 2},                    // ^
    {0x0006B338, (char*)"\x90\x90\x90\x90\x90\x90", 6},    // something something patches...
    {0x0006AC2D, (char*)"\x90\x90\x90\x90\x90\x90", 6},    // don't even remember doing this
    {0x0006A494, (char*)"\x90\x90", 2},                    //nevah jump                    
    {0x0006A6F0, (char*)"\x90\xE9", 2},                    //always jump
    {0, NULL, 0}
};

/* 9.00 patches below are taken from Graber Save-Mounter */
const orbis_patch_t shellcore_patches_900[] = {
    {0x00E351D9, (char*)"\x00", 1},                        // 'sce_sdmemory' patch
    {0x00E35218, (char*)"\x00", 1},                        // 'sce_sdmemory1' patch
    {0x00E35226, (char*)"\x00", 1},                        // 'sce_sdmemory2' patch
    {0x00E35234, (char*)"\x00", 1},                        // 'sce_sdmemory3' patch
    {0x008AEAE0, (char*)"\x48\x31\xC0\xC3", 4},            //verify keystone patch
    {0x0006C560, (char*)"\x31\xC0\xC3", 3},                //transfer mount permission patch eg mount foreign saves with write permission
    {0x000C9000, (char*)"\x31\xC0\xC3", 3},                //patch psn check to load saves saves foreign to current account
    {0x0006defe, (char*)"\x90\x90", 2},                    // ^ (thanks to GRModSave_Username)
    {0x0006C0A8, (char*)"\x90\x90\x90\x90\x90\x90", 6},    // something something patches...
    {0x0006BA62, (char*)"\x90\x90\x90\x90\x90\x90", 6},    // don't even remember doing this
    {0x0006B2C4, (char*)"\x90\x90", 2},                    //nevah jump                    
    {0x0006B51E, (char*)"\x90\xE9", 2},                    //always jump
    {0, NULL, 0}
};


const orbis_patch_t shellcore_lnc_debug_505[] = {
    {0x011F90BF, (char*)"\x01", 1},
    {0, NULL, 0}
};

/*
const orbis_patch_t shellcore_trophy_505[] = {
    {0x03ECEBF, (char*)"\x90\x90\x90\x90\x90\x90", 6},
    {0, NULL, 0}
};
*/
typedef struct OrbisSaveDataFingerprint {
    char data[ORBIS_SAVE_DATA_FINGERPRINT_DATA_SIZE];
    char padding[15];
} OrbisSaveDataFingerprint;

typedef struct OrbisSaveDataDirName {
    char data[ORBIS_SAVE_DATA_DIRNAME_DATA_MAXSIZE];
} OrbisSaveDataDirName;

typedef struct OrbisSaveDataTitleId {
    char data[ORBIS_SAVE_DATA_TITLE_ID_DATA_SIZE];
    char padding[6];
} OrbisSaveDataTitleId;

typedef struct OrbisSaveDataMountPoint {
    char data[ORBIS_SAVE_DATA_MOUNT_POINT_DATA_MAXSIZE];
} OrbisSaveDataMountPoint;

typedef struct OrbisSaveDataMountInfo {
    uint64_t blocks;
    uint64_t freeBlocks;
    uint8_t reserved[32];
} OrbisSaveDataMountInfo;

typedef struct __attribute__((packed)) OrbisSaveDataMount
{
    int32_t userId;
    int32_t unknown1;
    const char* titleId;
    const char* dirName;
    const char* fingerprint;
    uint64_t blocks;
    uint32_t mountMode;
    uint8_t reserved[36];
} OrbisSaveDataMount;

typedef struct __attribute__((packed)) OrbisSaveDataMountResult
{
    char mountPathName[ORBIS_SAVE_DATA_MOUNT_POINT_DATA_MAXSIZE];
    uint64_t requiredBlocks;
    uint32_t progress;
    uint8_t reserved[32];
} OrbisSaveDataMountResult;

typedef struct OrbisSaveDataDelete
{
    int32_t userId;
    int32_t unknown1;
    const OrbisSaveDataTitleId* titleId;
    const OrbisSaveDataDirName* dirName;
    uint32_t unused;
    uint8_t reserved[32];
    int32_t unknown2;
} OrbisSaveDataDelete;

typedef struct __attribute__((packed)) OrbisSaveDataParam
{
    char title[ORBIS_SAVE_DATA_TITLE_MAXSIZE];
    char subtitle[ORBIS_SAVE_DATA_SUBTITLE_MAXSIZE];
    char details[ORBIS_SAVE_DATA_DETAIL_MAXSIZE];
    uint32_t userParam;
    uint32_t unknown1;
    time_t mtime;
    char unknown2[0x20];
} OrbisSaveDataParam;

int read_buffer(const char *file_path, u8 **buf, size_t *size) {
	FILE *fp;
	u8 *file_buf;
	size_t file_size;

	if ((fp = fopen(file_path, "rb")) == NULL)
		return -1;
	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	file_buf = (u8 *)malloc(file_size);
	fread(file_buf, 1, file_size, fp);
	fclose(fp);

	if (buf)
		*buf = file_buf;
	else
		free(file_buf);
	if (size)
		*size = file_size;

	return 0;
}

list_t * list_alloc(void) {
	list_t *list;

	list = (list_t *)malloc(sizeof(list_t));
	if (!list)
		return NULL;
	memset(list, 0, sizeof(list_t));
	list->head = NULL;
	list->count = 0;

	return list;
}

void list_free(list_t *list) {
	list_node_t *node;
	list_node_t *tmp;

	if (!list)
		return;

	node = list->head;
	while (node) {
		tmp = node;
		node = node->next;
		free(tmp);
	}

	free(list);
}

list_node_t * list_append(list_t *list, void *value) {
	list_node_t *new_node;
	list_node_t *node;

	if (!list)
		return NULL;

	new_node = (list_node_t *)malloc(sizeof(list_node_t));
	if (!new_node)
		return NULL;
	new_node->value = value;
	new_node->next = NULL;

	node = list->head;
	if (!node)
		list->head = new_node;
	else {
		while (node) {
			if (!node->next) {
				node->next = new_node;
				break;
			}
			node = node->next;
		}
	}
	list->count++;

	return node;
}

list_node_t * list_head(list_t *list) {
	if (!list)
		return NULL;

	return list->head;
}

size_t list_count(list_t *list) {
	if (!list)
		return 0;

	return list->count;
}

list_node_t * list_next(list_node_t *node) {
	if (!node)
		return NULL;

	return node->next;
}

void * list_get(list_node_t *node) {
	if (!node)
		return NULL;

	return node->value;
}

sfo_context_t * sfo_alloc(void) {
	sfo_context_t *context;
	context = (sfo_context_t *)malloc(sizeof(sfo_context_t));
	if (context) {
		memset(context, 0, sizeof(sfo_context_t));
		context->params = list_alloc();
	}
	return context;
}

void sfo_free(sfo_context_t *context) {
	if (!context)
		return;

	if (context->params) {
		list_node_t *node;
		sfo_context_param_t *param;

		node = list_head(context->params);
		while (node) {
			param = (sfo_context_param_t *)node->value;
			if (param) {
				if (param->key)
					free(param->key);
				if (param->value)
					free(param->value);
				free(param);
			}
			node = node->next;
		}

		list_free(context->params);
	}

	free(context);
}

int sfo_read(sfo_context_t *context, const char *file_path) {
	int ret;
	u8 *sfo;
	size_t sfo_size;
	sfo_header_t *header;
	sfo_index_table_t *index_table;
	sfo_context_param_t *param;
	size_t i;

	ret = 0;

	if ((ret = read_buffer(file_path, &sfo, &sfo_size)) < 0)
		goto error;

	if (sfo_size < sizeof(sfo_header_t)) {
		ret = -1;
		goto error;
	}

	header = (sfo_header_t *)sfo;

	if (header->magic != 0x46535000u) {
		ret = -1;
		goto error;
	}

	for (i = 0; i < header->num_entries; ++i) {
		index_table = (sfo_index_table_t *)(sfo + sizeof(sfo_header_t) + i * sizeof(sfo_index_table_t));

		param = (sfo_context_param_t *)malloc(sizeof(sfo_context_param_t));
		if	(param) {
			memset(param, 0, sizeof(sfo_context_param_t));
			param->key = strdup((char *)(sfo + header->key_table_offset + index_table->key_offset));
			param->format = index_table->param_format;
			param->length = index_table->param_length;
			param->max_length = index_table->param_max_length;
			param->value = (u8 *)malloc(index_table->param_max_length);
			param->actual_length = index_table->param_max_length;
			if (param->value) {
				memcpy(param->value, (u8 *)(sfo + header->data_table_offset + index_table->data_offset), param->actual_length);
			} else {
				return -1;
			}
		} else {
			return -1;
		}

		list_append(context->params, param);
	}

error:
	return ret;
}

static sfo_context_param_t * sfo_context_get_param(sfo_context_t *context, const char *key) {
	list_node_t *node;
	sfo_context_param_t *param;

	if (!context || !key)
		return NULL;

	node = list_head(context->params);
	while (node) {
		param = (sfo_context_param_t *)node->value;
		if (param && strcmp(param->key, key) == 0)
			return param;
		node = node->next;
	}

	return NULL;
}

std::string sfo_get_param_value_string(sfo_context_t *in, const char* param) {
	sfo_context_param_t *p;

	p = sfo_context_get_param(in, param);
	if (p != NULL) {
          return std::string((const char*)(p->value));
	}

	return std::string();
}

int sfo_get_param_value_int(sfo_context_t *in, const char* param) {
    sfo_context_param_t *p;

    p = sfo_context_get_param(in, param);
    if (p != NULL) {
        return *(int*)(p->value);
    }

    return 0;
}

typedef struct OrbisSaveDataMount2 {
    int32_t userId;
    uint32_t unk1;
    const OrbisSaveDataDirName* dirName;
    uint64_t blocks;
    uint32_t mountMode;
    uint8_t reserved[32];
    uint32_t unk2;
} OrbisSaveDataMount2;

typedef struct OrbisSaveDataIcon {
    void* buf;
    size_t bufSize;
    size_t dataSize;
    uint8_t reserved[32];
} OrbisSaveDataIcon;


bool is_already_mounted = false;
char mount[32];
save_entry_t save; //Have UI Funcs grab this

extern "C"{
int32_t sceSaveDataMount(OrbisSaveDataMount*, OrbisSaveDataMountResult*);
int32_t sceSaveDataUmount(OrbisSaveDataMountPoint*);
int32_t sceSaveDataDelete(OrbisSaveDataDelete *del);
int32_t sceSaveDataSetParam(const OrbisSaveDataMountPoint * mountPoint, uint32_t type, void * buffer, size_t bufferSize);
int32_t sceSaveDataSaveIcon(OrbisSaveDataMountPoint *mnt, OrbisSaveDataIcon *icon); 
int32_t sceSaveDataInitialize3(int idc);
int32_t sceSaveDataTerminate();
}
static orbis_patch_t* shellcore_backup = NULL;
static int goldhen2 = 0;

int _sceKernelGetModuleInfo(OrbisKernelModule handle, OrbisKernelModuleInfo* info)
{
    if (!info)
        return 0x8002000E;

    memset(info, 0, sizeof(*info));
    info->size = sizeof(*info);

    return syscall(SYS_dynlib_get_info, handle, info);
}

static void sdmemory_patches_cb(void* patches, uint8_t* base, uint64_t size)
{
    for (const orbis_patch_t* patch = (const orbis_patch_t *)patches; patch->size > 0; patch++)
    {
        memcpy(base + patch->offset, patch->data, patch->size);

        log_info("(W) ptr=%p : %08X (%d bytes)", base, patch->offset, patch->size);
    }

}

static bool patch_SceSaveData(const orbis_patch_t* savedata_patch)
{
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SAVE_DATA);

    if (!patch_module("libSceSaveData.sprx", &sdmemory_patches_cb, (void*)savedata_patch, 0))
        return false;

    log_info("[+] Module patched!");
    return true;
}

static int get_firmware_version()
{
    int fw = ps4_fw_version();
    log_info("[i] PS4 Firmware %X", fw >> 16);
    return (fw >> 16);
}

// custom syscall 112
int sys_console_cmd(uint64_t cmd, void* data)
{
    return syscall(goldhen2 ? 200 : 112, cmd, data);
}

// GoldHEN 2+ only
// custom syscall 200
int check_syscalls()
{
    uint64_t tmp;

    if (syscall(200, SYS_CONSOLE_CMD_ISLOADED, NULL) == 1)
    {
        log_info("GoldHEN 2.x is loaded!");
        goldhen2 = 90;
        return 1;
    }
    else if (syscall(200, SYS_CONSOLE_CMD_PRINT, "apollo") == 0)
    {
        log_info("GoldHEN 1.x is loaded!");
        return 1;
    }
    else if (syscall(107, NULL, &tmp) == 0)
    {
        log_info("ps4debug is loaded!");
        return 1;
    }

    return 0;
}


// GoldHEN 2+ custom syscall 197
// (same as ps4debug syscall 107)
int sys_proc_list(struct proc_list_entry* procs, uint64_t* num)
{
    return syscall(107 + goldhen2, procs, num);
}

// GoldHEN 2+ custom syscall 198
// (same as ps4debug syscall 108)
int sys_proc_rw(uint64_t pid, uint64_t address, void* data, uint64_t length, uint64_t write)
{
    return syscall(108 + goldhen2, pid, address, data, length, write);
}

// GoldHEN 2+ custom syscall 199
// (same as ps4debug syscall 109)
int sys_proc_cmd(uint64_t pid, uint64_t cmd, void* data)
{
    return syscall(109 + goldhen2, pid, cmd, data);
}

/*
int goldhen_jailbreak()
{
    return sys_console_cmd(SYS_CONSOLE_CMD_JAILBREAK, NULL);
}
*/

int find_process_pid(const char* proc_name, int* pid)
{
    struct proc_list_entry* proc_list;
    uint64_t pnum;

    if (sys_proc_list(NULL, &pnum))
        return 0;

    proc_list = (struct proc_list_entry*)malloc(pnum * sizeof(struct proc_list_entry));

    if (!proc_list)
        return 0;

    if (sys_proc_list(proc_list, &pnum))
    {
        free(proc_list);
        return 0;
    }

    for (size_t i = 0; i < pnum; i++)
        if (strncmp(proc_list[i].p_comm, proc_name, 32) == 0)
        {
            log_info("[i] Found '%s' PID %d", proc_name, proc_list[i].pid);
            *pid = proc_list[i].pid;
            free(proc_list);
            return 1;
        }

    log_info("[!] '%s' Not Found", proc_name);
    free(proc_list);
    return 0;
}

int find_map_entry_start(int pid, const char* entry_name, uint64_t* start)
{
    struct sys_proc_vm_map_args proc_vm_map_args = {
        .maps = NULL,
        .num = 0
    };

    if (sys_proc_cmd(pid, SYS_PROC_VM_MAP, &proc_vm_map_args))
        return 0;

    proc_vm_map_args.maps = (struct proc_vm_map_entry*)malloc(proc_vm_map_args.num * sizeof(struct proc_vm_map_entry));

    if (!proc_vm_map_args.maps)
        return 0;

    if (sys_proc_cmd(pid, SYS_PROC_VM_MAP, &proc_vm_map_args))
    {
        free(proc_vm_map_args.maps);
        return 0;
    }

    for (size_t i = 0; i < proc_vm_map_args.num; i++)
        if (strncmp(proc_vm_map_args.maps[i].name, entry_name, 32) == 0)
        {
            log_info("Found '%s' Start addr %lX", entry_name, proc_vm_map_args.maps[i].start);
            *start = proc_vm_map_args.maps[i].start;
            free(proc_vm_map_args.maps);
            return 1;
        }

    log_info("[!] '%s' Not Found", entry_name);
    free(proc_vm_map_args.maps);
    return 0;
}

static void write_SceShellCore(int pid, uint64_t start, const orbis_patch_t* patches)
{
    //SHELLCORE PATCHES
    for (const orbis_patch_t* patch = patches; patch->size > 0; patch++)
    {
        sys_proc_write_mem(pid, start + patch->offset, patch->data, patch->size);

        log_info("(W) %lX : %08X (size %d)", start, patch->offset, patch->size);
        //dump_data((uint8_t*)patch->data, patch->size);
    }
}

// hack to disable unpatching on exit in case unmount failed (to avoid KP)
void disable_unpatch()
{
    shellcore_backup = NULL;
}

bool unpatch_SceShellCore()
{
    int pid;
    uint64_t ex_start;

    if (!shellcore_backup || !find_process_pid("SceShellCore", &pid) || !find_map_entry_start(pid, "executable", &ex_start))
    {
        log_info("Error: Unable to unpatch SceShellCore");
        return false;
    }

    write_SceShellCore(pid, ex_start, shellcore_backup);
    free(shellcore_backup), shellcore_backup = NULL;
    return true;
}



static bool patch_SceShellCore(const orbis_patch_t* shellcore_patch)
{
    int pid;
    uint64_t ex_start;

    log_info("Patching Shellcore....");
    if (!find_process_pid("SceShellCore", &pid) || !find_map_entry_start(pid, "executable", &ex_start))
    {
        log_info("Error: Unable to patch SceShellCore");
        return false;
    }

    if (!shellcore_backup)
    {
        size_t i = 0;

        while (shellcore_patch[i++].size) {}
        shellcore_backup = (orbis_patch_t *)calloc(1, i * sizeof(orbis_patch_t));

        for (i = 0; shellcore_patch[i].size > 0; i++)
        {
            shellcore_backup[i].offset = shellcore_patch[i].offset;
            shellcore_backup[i].size = shellcore_patch[i].size;
            shellcore_backup[i].data = (char*)malloc(shellcore_patch[i].size);
            sys_proc_read_mem(pid, ex_start + shellcore_backup[i].offset, shellcore_backup[i].data, shellcore_backup[i].size);

            log_info("(R) %lX : %08X (%d bytes)", ex_start, shellcore_backup[i].offset, shellcore_backup[i].size);
            //dump_data((uint8_t*)shellcore_backup[i].data, shellcore_backup[i].size);
        }

        write_SceShellCore(pid, ex_start, shellcore_patch);
    }

    return true;
}

bool patch_lnc_debug()
{
    if (!check_syscalls())
    {
        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::NO_GOLDHEN), "");
        return false;
    }

    if (!patch_SceShellCore(shellcore_lnc_debug_505))
    {
        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::FAILED_SAVE_PATCHES), "");
        return false;
    }

    return true;
}

bool is_patched()
{
    return shellcore_backup != NULL;
}

bool patch_save_libraries()
{
    if(is_patched()){
        return true;
    }
    const orbis_patch_t* shellcore_patch = NULL;
    const orbis_patch_t* savedata_patch = NULL;
    int version = get_firmware_version();

    switch (version)
    {
    case -1:
        return false;

    case 0x505:
    case 0x507:
        savedata_patch = scesavedata_patches_505;
        shellcore_patch = shellcore_patches_505;
        break;

    case 0x672:
        savedata_patch = scesavedata_patches_672;
        shellcore_patch = shellcore_patches_672;
        break;

    case 0x702:
        savedata_patch = scesavedata_patches_702;
        shellcore_patch = shellcore_patches_702;
        break;

    case 0x750:
    case 0x751:
    case 0x755:
        savedata_patch = scesavedata_patches_75x;
        shellcore_patch = shellcore_patches_75x;
        break;

    case 0x900:
        savedata_patch = scesavedata_patches_900;
        shellcore_patch = shellcore_patches_900;
        break;

    default:
        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::UNSUPPORTED_FW), fmt::format("{0:}: {1:#x}", getLangSTR(LANG_STR::UNSUPPORTED_FW), version >> 8));
        return false;
    }

    if (!check_syscalls())
    {
        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::NO_GOLDHEN), "");
        return false;
    }

    if (!patch_SceShellCore(shellcore_patch) || !patch_SceSaveData(savedata_patch))
    {
        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::FAILED_SAVE_PATCHES), "");
        return false;
    }

    log_info("PS4 %X.%02X Save patches applied", version >> 8, version & 0xFF);

    return true;
}

/*  GAME SAVE SHIT   */

bool orbis_SaveDelete(const save_entry_t* save)
{
    int ret;
    OrbisSaveDataDelete del;
    OrbisSaveDataDirName dir;
    OrbisSaveDataTitleId title;

    memset(&del, 0, sizeof(OrbisSaveDataDelete));
    memset(&dir, 0, sizeof(OrbisSaveDataDirName));
    memset(&title, 0, sizeof(OrbisSaveDataTitleId));
    strlcpy(dir.data, save->dir_name.c_str(), sizeof(dir.data));
    strlcpy(title.data, save->title_id.c_str(), sizeof(title.data));

    del.userId = save->userid;
    del.dirName = (const OrbisSaveDataDirName *)save->dir_name.c_str();
    del.titleId = (const OrbisSaveDataTitleId *)save->title_id.c_str();

    if ((ret = sceSaveDataDelete(&del)) < 0) {
       log_info("[DELETE] DELETE_ERROR: %x", ret);
       return false;
    }

    return true;
}

bool orbis_SaveUmount(const char* mountPath)
{
    OrbisSaveDataMountPoint umount;

    memset(&umount, 0, sizeof(OrbisSaveDataMountPoint));
    strncpy(umount.data, mountPath, sizeof(umount.data));

    int32_t umountErrorCode = sceSaveDataUmount(&umount);

    if (umountErrorCode < 0)
    {
        log_info("UMOUNT_ERROR (%X)", umountErrorCode);
        disable_unpatch();
    }

    return (umountErrorCode == ITEMZCORE_SUCCESS);
}

bool orbis_SaveMount(const save_entry_t* save, uint32_t mount_mode, char* mount_path)
{
    OrbisSaveDataDirName dirName;
    OrbisSaveDataTitleId titleId;
    memset(&dirName, 0, sizeof(OrbisSaveDataDirName));
    memset(&titleId, 0, sizeof(OrbisSaveDataTitleId));
    strlcpy(dirName.data, save->dir_name.c_str(), sizeof(dirName.data));
    strlcpy(titleId.data, save->title_id.c_str(), sizeof(titleId.data));

    OrbisSaveDataMount mount;
    memset(&mount, 0, sizeof(OrbisSaveDataMount));

    OrbisSaveDataFingerprint fingerprint;
    memset(&fingerprint, 0, sizeof(OrbisSaveDataFingerprint));
    strlcpy(fingerprint.data, "0000000000000000000000000000000000000000000000000000000000000000", sizeof(fingerprint.data));

    fmt::print("[MOUNT] Mounting {}", save->dir_name);
    fmt::print("SaveMount ({0:}, {1:}, {2:}, {3:#x}, {4:#x})", save->title_id, save->dir_name, mount_path, save->userid, mount_mode);

    mount.userId = save->userid;
    mount.dirName = dirName.data;
    mount.fingerprint = fingerprint.data;
    mount.titleId = titleId.data;
    mount.blocks = save->blocks;
    mount.mountMode = mount_mode | ORBIS_SAVE_DATA_MOUNT_MODE_DESTRUCT_OFF;

    OrbisSaveDataMountResult mountResult;
    memset(&mountResult, 0, sizeof(OrbisSaveDataMountResult));

    int32_t mountErrorCode = sceSaveDataMount(&mount, &mountResult);
    if (mountErrorCode < 0)
    {
       log_error("ERROR (0x%X): can't mount '%s/%s'", mountErrorCode, save->title_id.c_str(), save->dir_name.c_str());
       return false;
    }

    log_info("'%s/%s' mountPath (%s) ERROR: 0x%X", save->title_id.c_str(), save->dir_name.c_str(), mountResult.mountPathName, mountErrorCode);
    strncpy(mount_path, mountResult.mountPathName, ORBIS_SAVE_DATA_MOUNT_POINT_DATA_MAXSIZE);

    return true;
}

int orbis_UpdateSaveParams(std::string mountPath, std::string  title, std::string  subtitle, std::string  details, uint32_t userParam)
{
	OrbisSaveDataParam saveParams;
	OrbisSaveDataMountPoint mount;

    if(subtitle.empty())
       subtitle = "NA";
    if(details.empty())
       details = "NA";
    if(title.empty())
       title = "NA";

	memset(&saveParams, 0, sizeof(OrbisSaveDataParam));
	memset(&mount, 0, sizeof(OrbisSaveDataMountPoint));

	strncpy(mount.data, mountPath.c_str(), sizeof(mount.data));
	strncpy(saveParams.title, title.c_str(), ORBIS_SAVE_DATA_TITLE_MAXSIZE);
	strncpy(saveParams.subtitle, subtitle.c_str(), ORBIS_SAVE_DATA_SUBTITLE_MAXSIZE);
	strncpy(saveParams.details, details.c_str(), ORBIS_SAVE_DATA_DETAIL_MAXSIZE);
    saveParams.userParam = userParam;
	saveParams.mtime = time(NULL);

    log_info("UpdateSaveParams: (%s, %s, %s, %s, %x)", mount.data, saveParams.title, saveParams.subtitle, saveParams.details, userParam);

	int32_t setParamResult = sceSaveDataSetParam(&mount, ORBIS_SAVE_DATA_PARAM_TYPE_ALL, &saveParams, sizeof(OrbisSaveDataParam));
	if (setParamResult < 0) {
		log_error("sceSaveDataSetParam error (%X)", setParamResult);
		return false;
	}

	return true;
}

static bool _update_save_details(std::string mount, std::string usb_path, save_entry_t *save)
{
	std::string tmp;

    tmp = fmt::format("{}/sce_sys/param.sfo", usb_path);
    
    log_info("Update Save Details :: Reading %s...", tmp.c_str());

	// READ DATA FROM PARAM.SFO
	if (is_sfo(tmp.c_str()))
	{   
		if(!orbis_UpdateSaveParams(mount, save->main_title, save->sub_title, save->detail, save->userParam))
            return false;
	}  
    else{
        log_error("Update Save Details :: %s is not a valid SFO file", tmp.c_str());
        return false;
    }

    tmp = fmt::format("{}/sce_sys/icon0.png", usb_path);
	// READ ICON0.PNG, AND UPDATE THE ICON
	unsigned char* icon_png = NULL;
	if ((icon_png = orbisFileGetFileContent(tmp.c_str())) != NULL)
	{
		OrbisSaveDataMountPoint mp;
		OrbisSaveDataIcon icon;

		strlcpy(mp.data, mount.c_str(), sizeof(mp.data));
		memset(&icon, 0x00, sizeof(icon));
		icon.buf = icon_png;
		icon.bufSize = _orbisFile_lastopenFile_size;
		icon.dataSize = _orbisFile_lastopenFile_size;  // Icon data size

		if (sceSaveDataSaveIcon(&mp, &icon) < 0) {
			// Error handling
			log_info("ERROR sceSaveDataSaveIcon");
		}
		free(icon_png);
	}
    else
       log_error("Update Save Details :: %s no PNG file", tmp.c_str());
	

	return true;
}

void rm_nonprinting (std::string& str)
{
    str.erase (std::remove_if (str.begin(), str.end(),
                                [](unsigned char c){
                                    return !std::isprint(c);
                                }),
                                str.end());
}

bool get_save_details(std::vector<uint8_t> &sfo_data, save_entry_t &save)
{
    if(sfo_data.empty())
        return false;

    SfoReader sfo(sfo_data);
    save.userParam = 0;

    save.main_title = sfo.GetValueFor<std::string>("MAINTITLE");
    save.detail = sfo.GetValueFor<std::string>("DETAIL");
    save.dir_name = sfo.GetValueFor<std::string>("SAVEDATA_DIRECTORY");
    save.sub_title = sfo.GetValueFor<std::string>("SUBTITLE");
    save.title_id = sfo.GetValueFor<std::string>("TITLE_ID");
    rm_nonprinting(save.main_title);
    rm_nonprinting(save.detail);
    rm_nonprinting(save.sub_title);

    //save.user_id = sfo.GetValueFor<int>("ACCOUNT_ID");SAVEDATA_LIST_PARAM
    save.userParam = sfo.GetValueFor<int>("SAVEDATA_LIST_PARAM");
    save.blocks = sfo.GetValueFor<int>("SAVEDATA_BLOCKS");

    return true;
}

//make a c++ function that checks if a folderhas more than 1 save folder in it by checking if the folder has a param.sfo file
bool is_save_folder(std::string path)
{
    std::string tmp = fmt::format("{}/sce_sys/param.sfo", path);
    if (is_sfo(tmp.c_str()))
        return true;
    else
        return false;
}
// recursively check if a folder has more than 1 save folder in it
// in c++
bool is_nested_saves_recursive(std::string path, std::vector<std::string> &save_folders)
{
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (path.c_str())) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            if (ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
                std::string tmp = fmt::format("{}/{}", path, ent->d_name);
                if (is_save_folder(tmp))
                    save_folders.push_back(tmp);
            }
        }
        closedir (dir);
    } else {
        log_error("is_save_folder_recursive: could not open directory %s", path.c_str());
        return false;
    }
    if (save_folders.size() > 1)
        return true;
    else
        return false;
}
void SaveData_Operations(SAVE_Multi_Sel sv, std::string tid, std::string path)
{

    int ret;
    u32 userId;
    int save_flag = ORBIS_SAVE_DATA_MOUNT_MODE_RDONLY;
    std::string dest;
    std::string tmp2;
    std::string usb_path = fmt::format("/mnt/usb{0:d}", usbpath());
    
    int saves = (sv == RESTORE_GAME_SAVE) ? 1 : gm_save.numb_of_saves;

    loadmsg(getLangSTR(LANG_STR::SAVE_OP_ON_GOING));
    //Check to see if its a devkit or testkit filesystem
    //only Backing up saves works with devkit modules
    if (sv != BACKUP_GAME_SAVE && if_exists("/system/sys/set_upper.self")) {
        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::UNSUPPORTED_FS), getLangSTR(LANG_STR::RETAILO));
        sceMsgDialogTerminate();
        return;
    }
    if (sv < RESTORE_GAME_SAVE) {
        if (usbpath() == -1) {
            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::SAVE_OPS_FAILED), getLangSTR(LANG_STR::NO_USB));
            sceMsgDialogTerminate();
            return;
        }
    }


    if (!patch_save_libraries()){ 
        sceMsgDialogTerminate();
        return;
    }

    if ((ret = sceUserServiceGetForegroundUser(&userId)) == ITEMZCORE_SUCCESS) {

        if(is_already_mounted){
            orbis_SaveUmount(mount);
            is_already_mounted = false;
        }
        
        if (saves <= 0 && sv != RESTORE_GAME_SAVE) {
                ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::NO_SAVES), getLangSTR(LANG_STR::CANT_FIND_SAVE));
                goto unpatch;
        }
        if (sceSaveDataInitialize3(0) != ITEMZCORE_SUCCESS){
                ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::SAVE_OPS_FAILED), getLangSTR(LANG_STR::FAILED_INI_SD));
                goto unpatch;
        }
        for (int j = 0; j < saves; j++) {
            // loop thru all saves for title-id
             save.userid = userId;
             save.title_id = tid;
             std::string tmp = fmt::format("/system_data/savedata/{0:x}/db/user/savedata.db", userId);

            //do we even need this for the other operations?
            // replaced with ret to sav the value of j
            if(sv != RESTORE_GAME_SAVE){
               log_info("Save Data Operations :: %d %d %d %s", sv, saves, (sv == DELETE_GAME_SAVE) ? 0 : j, save.dir_name.c_str());
               if (!GameSave_Info(&save, (sv == DELETE_GAME_SAVE) ? 0 : j)) {
                   ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::NO_SAVES), getLangSTR(LANG_STR::CANT_FIND_SAVE));
                   goto unpatch;
                }
            }
            else if(sv == RESTORE_GAME_SAVE){
                dest = fmt::format("{}/sce_sys/param.sfo", path);
                if (!if_exists(dest.c_str()) || !is_sfo(dest.c_str())) {
                   ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::NO_SAVES), getLangSTR(LANG_STR::CANT_FIND_SAVE));
                   goto unpatch;
                } 

                std::vector<uint8_t> sfo_data = readFile(dest);
                save.blocks = -1;
                //ini sets blocks
                if(sfo_data.empty() || !get_save_details(sfo_data, save)){
                   ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::NO_SAVES), getLangSTR(LANG_STR::CANT_FIND_SAVE));
                   goto unpatch;
                } 

                fmt::print("[SAVE] {0:} | Blocks: {1:x}", save.dir_name, save.blocks);

                save_flag =  ORBIS_SAVE_DATA_MOUNT_MODE_RDWR | ORBIS_SAVE_DATA_MOUNT_MODE_CREATE2 | ORBIS_SAVE_DATA_MOUNT_MODE_COPY_ICON;
                // see if the dir_name (save) is already exists
                if(If_Save_Exists(tmp.c_str(), &save)){
                   if(Confirmation_Msg(getLangSTR(LANG_STR::OVERWRITE_SAVE)) ==  YES){
                      orbis_SaveDelete(&save);
                   }
                   else{
                      log_debug("Save overwrite cancelled");
                      ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::OP_CANNED), getLangSTR(LANG_STR::OP_CANNED_1));
                      goto unpatch;
                   }
                }
            }

            if (sv != DELETE_GAME_SAVE) {
                if (!orbis_SaveMount(&save, save_flag, mount)) {
                    ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::SAVE_OPS_FAILED), getLangSTR(LANG_STR::FAILED_MOUNT_SD));
                    log_error("Error! Failed to mount save data!");
                    goto unpatch;
                }
                else
                    is_already_mounted = true;
            }

            
            fmt::print("dir_name: {0:} | title_id: {0:}", save.dir_name, save.title_id);
            log_info("userid: 0xx%x", save.userid);

            save.path = fmt::format("/mnt/sandbox/ITEM00001_000{0:}/", mount); 

            switch (sv)
            {
            case BACKUP_GAME_SAVE: {

                dest = fmt::format("{0:}/itemzflow/saves/{1:x}/{2:}/{3:d}_{4:}/sce_sys/", usb_path, userId, tid, j+1, save.dir_name);
                std::filesystem::create_directories(std::filesystem::path(dest));
                dest = fmt::format("{0:}/itemzflow/saves/{1:x}/{2:}/{3:d}_{4:}/", usb_path, userId, tid, j+1, save.dir_name);

                fmt::print("Copying save data to {0:}", dest);

                if (!copy_dir(save.path, dest)){
                     ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::SAVE_OPS_FAILED), getLangSTR(LANG_STR::SD_TRANSFER_FAIL));
                     break;
                }

                ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::SD_SAVED), fmt::format("{0:d}/{1:d}", j+1, saves));

                break;
            }
            case DELETE_GAME_SAVE: {

                if(orbis_SaveDelete(&save)){
                    ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::SD_DELETED), fmt::format("{0:} ({1:d}/{2:d})", save.title_id, j+1, saves));
                    //1 less save for the UI
                    gm_save.is_loaded = false;
                    gm_save.numb_of_saves--;
                }
                else
                    ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::SAVE_OPS_FAILED), getLangSTR(LANG_STR::CANT_DELETE_SD));

                break; 
            }
            case RESTORE_GAME_SAVE: {

                tmp2 = fmt::format("/mnt/sandbox/ITEM00001_000{0:}/", mount);
                fmt::print("Copying save data to {0:} {1:}", tmp2, dest);

	            if(!copy_dir(path, tmp2)) goto restore_error;

	            if(!_update_save_details(mount, path, &save)) goto restore_error;
                
                if((gm_save.is_loaded = GameSave_Info(&gm_save, gm_save.numb_of_saves))){
                   dest = fmt::format("{0:}: {1:}", getLangSTR(LANG_STR::SAVE_IMPORT_2), gm_save.main_title);
                   ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::SAVE_IMPORTED), dest);
                   break;
                }

restore_error:
                ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::RESTORE_FAILED), getLangSTR(LANG_STR::SAVE_OPS_FAILED));
                break;
            }
            default:
                break;
            }

            if (sv != DELETE_GAME_SAVE) {
                log_info("%s %s", save.path.c_str(), mount);
                orbis_SaveUmount(mount);
                is_already_mounted = false;
            }
        }
        // end loop
    }
    else {
        log_info("%x", ret);
        dest = fmt::format("{0:}: {1:#x}", getLangSTR(LANG_STR::NO_USERID), ret);
        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::SAVE_OPS_FAILED), dest);
    }

unpatch:
   log_info("unpatching ...");
  // if (unpatch_SceShellCore())
      //  log_info("PS4 Save patches removed from memory");

    sceSaveDataTerminate();
    sceMsgDialogTerminate();
}