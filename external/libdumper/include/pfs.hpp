#ifndef PFS_HPP_
#define PFS_HPP_

#include <iostream>
#include <vector>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ps4sdk.h>

#define PFS_MAGIC 0x0B2A330100000000

#define PFS_DUMP_BUFFER 0x200000

namespace pfs {
	typedef struct {
		uint64_t version;
		uint64_t magic;
		uint32_t id[2];
		char fmode;
		char clean;
		char ronly;
		char rsv;
		uint16_t mode;
		uint16_t unk1;
		uint32_t blocksz;
		uint32_t nbackup;
		uint64_t nblock;
		uint64_t ndinode;
		uint64_t ndblock;
		uint64_t ndinodeblock;
		uint64_t superroot_ino;
	} pfs_header;

	typedef struct {
		uint16_t mode;
		uint16_t nlink;
		uint32_t flags;
		uint64_t size;
		uint64_t size_compressed;
		uint64_t unix_time[4];
		uint32_t time_nsec[4];
		uint32_t uid;
		uint32_t gid;
		uint64_t spare[2];
		uint32_t blocks;
		uint32_t db[12];
		uint32_t ib[5];
	} di_d32;

	typedef struct {
		uint32_t ino;
		uint32_t type;
		uint32_t namelen;
		uint32_t entsize;
	} pfs_dirent_t;

	extern pfs_header* header;
	extern size_t pfs_size;
	extern size_t pfs_copied;
	extern di_d32* inodes;

	void __parse_directory(uint32_t ino, uint32_t lev, const char* parent_name, bool dry_run, char* title = NULL, const char* tid = "CUSA0000");
	void calculate_pfs(uint32_t ino, uint32_t level, const std::string& output_path);
	void dump_pfs(uint32_t ino, uint32_t level, const std::string& output_path, const char* tid = "CUSA0000", char* title = NULL);
	bool extract(const std::string& pfs_path, const std::string& output_path, const char* tid = "CUSA0000", char* title = NULL);
} // namespace pfs

#endif // PFS_HPP_
