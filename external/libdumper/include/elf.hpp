// Copyright (c) 2021 Al Azif
// License: GPLv3

#ifndef ELF_HPP_
#define ELF_HPP_

#include "elf_64.hpp"
#include "elf_common.hpp"

#include "log.h"
#include <iostream>
#include <vector>

#define ELF_MAGIC 0x7F454C46
#define SELF_MAGIC 0x4F153D1D

#define MAP_SELF 0x80000
#define PT_NID 0x61000000


typedef struct _dirdesc {
	int	dd_fd;		/* file descriptor associated with directory */
	long	dd_loc;		/* offset in current buffer */
	long	dd_size;	/* amount of data returned by getdirentries */
	char* dd_buf;	/* data buffer */
	int	dd_len;		/* size of data buffer */
	long	dd_seek;	/* magic cookie returned by getdirentries */
	long	dd_rewind;	/* magic cookie for rewinding */
	int	dd_flags;	/* flags for readdir */
	struct pthread_mutex* dd_lock;	/* lock */
	struct _telldir* dd_td;
} DIR;

extern "C"
{
	extern struct dirent* readdir(DIR* dirp);
	extern int closedir(DIR* dirp);
	extern DIR* opendir(const char* filename);
}


namespace elf {
	typedef struct {
		uint32_t props;
		uint32_t reserved;
		uint64_t offset;
		uint64_t file_size;
		uint64_t memory_size;
	} SelfEntry;

	// SELF Header from: https://www.psdevwiki.com/ps4/SELF_File_Format#SELF_Header_Structure
	typedef struct {
		uint32_t magic; /* File magic. */

		// uint32_t unknown;        /* Always 00 01 01 12. */
		uint8_t version;
		uint8_t mode;
		uint8_t endian;
		uint8_t attr;

		unsigned char content_type; /* 1 on Self, 4 on PUP Entry. */
		unsigned char program_type; /* 0x0 PUP, 0x8 NPDRM Application, 0x9 PLUGIN, 0xC Kernel, 0xE Security Module, 0xF Secure Kernel */
		uint16_t padding;           /* Padding. */
		uint16_t header_size;       /* Header size. */
		uint16_t signature_size;    /* Metadata size */
		uint64_t self_size;         /* Size of SELF. */
		uint16_t num_of_segments;   /* Number of Segments, 1 Kernel, 2 SL and Modules, 4 Kernel ELFs, 6 .selfs, 2 .sdll, 6 .sprx, 6 ShellCore, 6 eboot.bin, 2 sexe. */
		uint16_t flags;             /* Always 0x22. */
		uint32_t reserved;          /* Reserved. */
	} SelfHeader;

	// SCE Header from: https://www.psdevwiki.com/ps4/SELF_File_Format#SCE_Special
	typedef struct {
		uint64_t program_authority_id;
		uint64_t program_type;
		uint64_t app_version;
		uint64_t fw_version;
		unsigned char digest[0x20];
	} SceHeader;

	// SCE Header from: https://www.psdevwiki.com/ps4/SELF_File_Format#SCE_Special
	typedef struct {
		uint64_t program_authority_id;
		uint64_t program_type;
		uint64_t app_version;
		uint64_t fw_version;
		unsigned char digest[0x20];
		unsigned char padding[0x10];    // TODO: There is something here besides just padding...?
		unsigned char content_id[0x14]; // TODO: Or is it 0x13 if you don't include the "-"
		unsigned char unknown[0xC];     // TODO: There appears to be more here...?
	} SceHeaderNpdrm;

	typedef struct {
		int index;
		uint64_t fileoff;
		size_t bufsz;
		size_t filesz;
		int enc;
	} SegmentBufInfo;


#define TRUE 1
#define FALSE 0

#define DECRYPT_SIZE 0x100000

	uint64_t get_sce_header_offset(const std::string& path);
	SceHeader get_sce_header(const std::string& path);
	SceHeaderNpdrm get_sce_header_npdrm(const std::string& path);
	bool Check_ELF_Magic(const std::string& path, uint32_t FILE_MAGIC = ELF_MAGIC);
	bool is_npdrm(const std::string& path);
	std::string get_ptype(const std::string& path);
	uint64_t get_paid(const std::string& path);
	uint64_t get_app_version(const std::string& path);
	uint64_t get_fw_version(const std::string& path);
	std::vector<unsigned char> get_digest(const std::string& path);
	std::vector<unsigned char> get_auth_info(const std::string& path);
	bool is_valid_decrypt(const std::string& original, const std::string& decrypted);
	void zero_section_header(const std::string& path);
	bool decrypt_dir(const std::string& inputPath, const std::string& outputPath, bool fself);
	bool decrypt_and_dump_self(const char* selfFile, const char* saveFile);
} // namespace elf

#endif // ELF_HPP_
