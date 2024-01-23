// Copyright (c) 2021 Al Azif
// License: GPLv3

#include "pkg.hpp"
#include "common.hpp"
#include "log.h"
#include "lang.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include "pfs.hpp"
#include <user_mem.h>
#include <regex>
namespace pfs {
    std::unique_ptr<pfs_header> header;
    uint64_t pfs_size = 0;
    uint64_t pfs_copied = 0;
    std::vector<di_d32> inodes;
    std::unique_ptr<char[]> copy_buffer;
    int pfs = -1;
    uint32_t pfs_progress = 10;
    Dump_Options dump_op;

    void memcpy_to_file(const std::string& fname, uint64_t ptr, uint64_t size, const std::string& title, const std::string& tid)
    {
        uint64_t bytes;
        uint64_t ix = 0;
        if(std::filesystem::exists(fname)){
            log_debug("File already exists: %s", fname.c_str());
            size -= bytes;
            ix++;
            pfs_copied += bytes;
            if (pfs_copied > pfs_size) pfs_copied = pfs_size;
            pfs_progress = (uint64_t)(((float)pfs_copied / pfs_size) * 100.f);
            return;
        }
        int fd = sceKernelOpen(fname.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK, 0777);
        log_info("---- name: %s, ptr %p, sz %zu, FD: %i", fname.c_str(), ptr, size, fd);
        if (fd > 0)
        {
            while (size > 0)
            {
                bytes = (size > PFS_DUMP_BUFFER) ? PFS_DUMP_BUFFER : size;
               // log_info("copying %llu bytes ix: %i size: %llu pfs prog: %llu/%llu", bytes, ix, size, pfs_copied, pfs_size);
                sceKernelLseek(pfs, ptr + ix * PFS_DUMP_BUFFER, SEEK_SET);
                sceKernelRead(pfs, copy_buffer.get(), bytes);
                sceKernelWrite(fd, copy_buffer.get(), bytes);
                size -= bytes;
                ix++;
                pfs_copied += bytes;
                if (pfs_copied > pfs_size) pfs_copied = pfs_size;
                pfs_progress = (uint64_t)(((float)pfs_copied / pfs_size) * 100.f);
                switch(pfs::dump_op){
                    case BASE_GAME:
                        ProgUpdate(pfs_progress,  getDumperLangSTR(DUMP_INFO) + "\n\n" + getDumperLangSTR(APP_NAME) + ": " + title + "\n" + getDumperLangSTR(TITLE_ID) + " " + tid + "\n\n" + getDumperLangSTR(EXT_GAME_FILES)  + "\n" + fname);
                    break;
                    case GAME_PATCH:
                        ProgUpdate(pfs_progress,  getDumperLangSTR(DUMP_INFO) + "\n\n" + getDumperLangSTR(APP_NAME) + ": " + title + "\n" + getDumperLangSTR(TITLE_ID) + " " + tid + "\n\n" + getDumperLangSTR(EXT_PATCH) + "\n" + fname); 
                    break;
                    case ADDITIONAL_CONTENT_DATA:
                        ProgUpdate(pfs_progress,  getDumperLangSTR(DUMP_INFO) + "\n\n" + getDumperLangSTR(DLC_NAME) + ": " + title + "\n" + getDumperLangSTR(TITLE_ID) + " " + tid + "\n\n" + getDumperLangSTR(EXT_ADDON) + "\n" + fname);
                    break;
                    default:
                    break;
                }
            }
            log_info("Close(fd): %x", sceKernelClose(fd));
        }
        else
        {
            log_error("Cannot copy file |%s|\n\nError: %x!", fname.c_str(), fd);
        }
    }

    void __parse_directory(uint32_t ino, uint32_t lev, const std::string& parent_name, bool dry_run, const std::string& title = "dry", const std::string& tid = "run") {

    for (uint32_t z = 0; z < inodes[ino].blocks; z++) {
        uint32_t db = inodes[ino].db[0] + z;
        uint64_t pos = static_cast<uint64_t>(header->blocksz) * db;
        uint64_t size = inodes[ino].size;
        uint64_t top = pos + size;

        while (pos < top) {
            std::unique_ptr<pfs_dirent_t> ent = std::make_unique<pfs_dirent_t>();
            sceKernelLseek(pfs, pos, SEEK_SET);
            sceKernelRead(pfs, ent.get(), sizeof(pfs_dirent_t));

            if (ent->type == 0) {
                break;
            }

            std::unique_ptr<char[]> name = std::make_unique<char[]>(ent->namelen + 1);
            memset(name.get(), 0, ent->namelen + 1);
            if (lev > 0) {
                sceKernelLseek(pfs, pos + sizeof(pfs_dirent_t), SEEK_SET);
                sceKernelRead(pfs, name.get(), ent->namelen);
            }
            std::string fname;
            if (parent_name.empty()) {
                fname = name.get();
            } else {
                fname = parent_name + "/" + name.get();
            }

            if ((ent->type == 2) && (lev > 0)) {
                if (!dry_run) {
                   pfs_progress = (uint64_t)(((float)pfs_copied / pfs_size) * 100.f);
                    switch(pfs::dump_op){
                        case BASE_GAME:
                        ProgUpdate(pfs_progress,  getDumperLangSTR(DUMP_INFO) + "\n\n" + getDumperLangSTR(APP_NAME) + ": " + title + "\n" + getDumperLangSTR(TITLE_ID) + " " + tid + "\n\n" + getDumperLangSTR(EXT_GAME_FILES)  + "\n" + fname);
                        break;
                        case GAME_PATCH:
                        ProgUpdate(pfs_progress,  getDumperLangSTR(DUMP_INFO) + "\n\n" + getDumperLangSTR(APP_NAME) + ": " + title + "\n" + getDumperLangSTR(TITLE_ID) + " " + tid + "\n\n" + getDumperLangSTR(EXT_PATCH) + "\n" + fname); 
                        break;
                        case ADDITIONAL_CONTENT_DATA:
                        ProgUpdate(pfs_progress,  getDumperLangSTR(DUMP_INFO) + "\n\n" + getDumperLangSTR(DLC_NAME) + ": " + title + "\n" + getDumperLangSTR(TITLE_ID) + " " + tid + "\n\n" + getDumperLangSTR(EXT_ADDON) + "\n" + fname);
                        break;
                        default:
                        break;
                    }
                }
                if (dry_run) {
                    pfs_size += inodes[ent->ino].size;
                } else {
                    memcpy_to_file(fname, static_cast<uint64_t>(header->blocksz) * inodes[ent->ino].db[0], inodes[ent->ino].size, title, tid);
                }

               // log_info("%s -- prog %i", fname.c_str(), pfs_progress);
            } else if (ent->type == 3) {
                log_info(">scan dir %s", name.get());
                mkdir(fname.c_str(), 0777);
                __parse_directory(ent->ino, lev + 1, fname, dry_run, title, tid);
            }

            pos += ent->entsize;
        }
    }
}
    void dump_pfs(uint32_t ino, uint32_t level, const std::string& output_path, const std::string& tid, const std::string& title) {
        __parse_directory(ino, level, output_path, false, title, tid);
    }

    void calculate_pfs(uint32_t ino, uint32_t level, const std::string& output_path) {
        __parse_directory(ino, level, output_path, true);
    }

    bool extract(const std::string& pfs_path, const std::string& output_path, const std::string& tid, const std::string& title)
    {
        pfs_progress = 11;
        // clean output path

            pfs = sceKernelOpen(pfs_path.c_str(), O_RDONLY, 0);
            if (pfs < 0) {
                log_fatal("could not open %s", pfs_path.c_str());
                return false;
            }

            copy_buffer = std::make_unique<char[]>(PFS_DUMP_BUFFER);

            header = std::make_unique<pfs_header>();
            sceKernelLseek(pfs, 0, SEEK_SET);
            sceKernelRead(pfs, header.get(), sizeof(pfs_header));

            if (__builtin_bswap64(header->magic) != PFS_MAGIC) {
                log_fatal("MAGIC != PFS_MAGIC");
                sceKernelClose(pfs);
                return false;
            }
            inodes.resize(header->ndinode);
            uint32_t ix = 0;

            ProgUpdate(20,  getDumperLangSTR(DUMP_INFO) + "\n\n" + title + "\n" + getDumperLangSTR(TITLE_ID) + " " + tid + "\n\n" + getDumperLangSTR(PROCESSING)); 
            for (uint32_t i = 0; i < header->ndinodeblock; i++) {
                for (uint32_t j = 0; (j < (header->blocksz / sizeof(di_d32))) && (ix < header->ndinode); j++) {
                    sceKernelLseek(pfs, (uint64_t)header->blocksz * (i + 1) + sizeof(di_d32) * j, SEEK_SET);
                    sceKernelRead(pfs, &inodes[ix], sizeof(di_d32));

                    ix++;
                }
            }

            pfs_size = 0;
            pfs_copied = 0;

            calculate_pfs((uint32_t)header->superroot_ino, (uint32_t)0, output_path);
            dump_pfs((uint32_t)header->superroot_ino, (uint32_t)0, output_path, tid, title);
            sceKernelClose(pfs);

            return true;
        }
} // namespace pfs