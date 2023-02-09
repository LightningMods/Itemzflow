// Copyright (c) 2021 Al Azif
// License: GPLv3

#include "pkg.hpp"
#include "common.hpp"
#include "log.h"
extern "C" {
#include "lang.h"
}

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include "pfs.hpp"
#include <user_mem.h>

uint32_t pfs_progress = 10;

namespace pfs {
    pfs_header* header;
    uint64_t pfs_size;
    uint64_t pfs_copied;
    di_d32* inodes;

    char* copy_buffer = NULL;
    int pfs;

    void memcpy_to_file(const char* fname, uint64_t ptr, uint64_t size, char* title, const char* tid)
    {
        uint64_t bytes;
        uint64_t ix = 0;

        int fd = sceKernelOpen(fname, O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK, 0777);
        log_info("---- name: %s, ptr %p, sz %zu, FD: %i", fname, ptr, size, fd);
        if (fd > 0)
        {
            while (size > 0)
            {
                bytes = (size > PFS_DUMP_BUFFER) ? PFS_DUMP_BUFFER : size;
                log_info("copying %llu bytes ix: %i size: %llu pfs prog: %llu/%llu", bytes, ix, size, pfs_copied, pfs_size);
                sceKernelLseek(pfs, ptr + ix * PFS_DUMP_BUFFER, SEEK_SET);
                sceKernelRead(pfs, copy_buffer, bytes);
                sceKernelWrite(fd, copy_buffer, bytes);
                size -= bytes;
                ix++;
                pfs_copied += bytes;
                if (pfs_copied > pfs_size) pfs_copied = pfs_size;
                pfs_progress = (uint64_t)(((float)pfs_copied / pfs_size) * 100.f);
                if (strstr(fname, "patch"))
                    ProgUpdate(pfs_progress, "%s\n\n%s %s\n%s %s\n\n%s\n %s...\n", getDumperLangSTR(DUMP_INFO), getDumperLangSTR(APP_NAME), title, getDumperLangSTR(TITLE_ID), tid, getDumperLangSTR(EXT_PATCH), fname);
                else
                    ProgUpdate(pfs_progress, "%s\n\n%s %s\n%s %s\n\n%s\n %s...\n", getDumperLangSTR(DUMP_INFO), getDumperLangSTR(APP_NAME), title, getDumperLangSTR(TITLE_ID), tid, getDumperLangSTR(EXT_GAME_FILES), fname);

            }
            log_info("Close(fd): %x", sceKernelClose(fd));
        }
        else
        {
            log_error("Cannot copy file |%s|\n\nError: %x!", fname, fd);
        }
    }

    void __parse_directory(uint32_t ino, uint32_t lev, const char* parent_name, bool dry_run, char* title, const char* tid)
    {
        for (uint32_t z = 0; z < inodes[ino].blocks; z++)
        {
            uint32_t db = inodes[ino].db[0] + z;
            uint64_t pos = (uint64_t)header->blocksz * db;
            uint64_t size = inodes[ino].size;
            uint64_t top = pos + size;

            while (pos < top)
            {
                pfs_dirent_t* ent = (pfs_dirent_t*)malloc(sizeof(pfs_dirent_t));
                sceKernelLseek(pfs, pos, SEEK_SET);
                sceKernelRead(pfs, ent, sizeof(pfs_dirent_t));

                if (ent->type == 0)
                {
                    free(ent);
                    break;
                }

                char* name = (char*)malloc(ent->namelen + 1);
                memset(name, 0, ent->namelen + 1);
                if (lev > 0)
                {
                    sceKernelLseek(pfs, pos + sizeof(pfs_dirent_t), SEEK_SET);
                    sceKernelRead(pfs, name, ent->namelen);
                }


                char* fname = (char*)malloc(strlen(parent_name) + ent->namelen + 2);
                if (parent_name != NULL)
                    sprintf(fname, "%s/%s", parent_name, name);
                else
                    sprintf(fname, "%s", name);

                if ((ent->type == 2) && (lev > 0))
                {
                    if (!dry_run) {
                        if (strstr(fname, "patch"))
                            ProgUpdate(pfs_progress, "%s\n\n%s %s\n%s %s\n\n%s\n %s...\n", getDumperLangSTR(DUMP_INFO), getDumperLangSTR(APP_NAME), title, getDumperLangSTR(TITLE_ID), tid, getDumperLangSTR(EXT_PATCH), fname);
                        else
                            ProgUpdate(pfs_progress, "%s\n\n%s %s\n%s %s\n\n%s\n %s...\n", getDumperLangSTR(DUMP_INFO), getDumperLangSTR(APP_NAME), title, getDumperLangSTR(TITLE_ID), tid, getDumperLangSTR(EXT_GAME_FILES), fname);
                    }
                    if (dry_run)
                        pfs_size += inodes[ent->ino].size;
                    else
                        memcpy_to_file(fname, (uint64_t)header->blocksz * inodes[ent->ino].db[0], inodes[ent->ino].size, title, tid);

                    log_info("%s -- prog %i", fname, pfs_progress);
                }
                else
                    if (ent->type == 3)
                    {
                        log_info(">scan dir %s", name);
                        mkdir(fname, 0777);
                        __parse_directory(ent->ino, lev + 1, fname, dry_run, title, tid);
                    }

                pos += ent->entsize;

                free(ent);
                free(name);
                free(fname);
            }
        }
    }

    void dump_pfs(uint32_t ino, uint32_t level, const std::string& output_path, const char* tid, char* title) {
        __parse_directory(ino, level, output_path.c_str(), false, title, tid);
    }

    void calculate_pfs(uint32_t ino, uint32_t level, const std::string& output_path) {
        __parse_directory(ino, level, output_path.c_str(), true);
    }

    bool extract(const std::string& pfs_path, const std::string& output_path, const char* tid, char* title)
    {
        pfs_progress = 11;

        pfs = sceKernelOpen(pfs_path.c_str(), O_RDONLY, 0);
        if (pfs < 0) {
            log_fatal("could not open %s", pfs_path.c_str());
            return false;
        }


        if ((copy_buffer = (char*)malloc(PFS_DUMP_BUFFER)) != NULL)
        {

            if ((header = (pfs_header*)malloc(sizeof(pfs_header))) != NULL)
            {
                sceKernelLseek(pfs, 0, SEEK_SET);
                sceKernelRead(pfs, header, sizeof(pfs_header));

                if (__builtin_bswap64(header->magic) != PFS_MAGIC) {
                    log_fatal("MAGIC != PFS_MAGIC");
                    sceKernelClose(pfs);
                    free(copy_buffer);
                    free(header);
                    return false;
                }

                if ((inodes = (di_d32*)malloc(sizeof(di_d32) * header->ndinode)) != NULL)
                {

                    uint32_t ix = 0;

                    ProgUpdate(20, "%s\n\n%s %s\n%s %s\n\n %s ...", getDumperLangSTR(DUMP_INFO), getDumperLangSTR(APP_NAME), title, getDumperLangSTR(TITLE_ID), tid, getDumperLangSTR(PROCESSING));

                    for (uint32_t i = 0; i < header->ndinodeblock; i++)
                    {
                        for (uint32_t j = 0; (j < (header->blocksz / sizeof(di_d32))) && (ix < header->ndinode); j++)
                        {
                            sceKernelLseek(pfs, (uint64_t)header->blocksz * (i + 1) + sizeof(di_d32) * j, SEEK_SET);
                            sceKernelRead(pfs, &inodes[ix], sizeof(di_d32));

                            ix++;
                        }
                    }

                    pfs_size = 0;
                    pfs_copied = 0;

                    calculate_pfs((uint32_t)header->superroot_ino, (uint32_t)0, output_path);
                    dump_pfs((uint32_t)header->superroot_ino, (uint32_t)0, output_path, tid, title);

                    free(header);
                    free(inodes);
                    free(copy_buffer);
                    sceKernelClose(pfs);
                }
                else
                {
                    free(copy_buffer);
                    free(header);
                    sceKernelClose(pfs);
                    log_fatal("Buffer is NULL");
                    return false;
                }
            }
            else
            {
                free(copy_buffer);
                log_fatal("Buffer is NULL");
                return false;
            }
        }
        else
        {

            log_fatal("Buffer is NULL");
            return false;
        }

        return true;
    }

} // namespace pfs


