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

    // Mounted (non-nest) view of the PFS being dumped, e.g. /mnt/sandbox/pfsmnt/CUSA00568-app0
    // Used to probe whether a file's data is actually installed (PlayGo language chunks
    // that are not installed make raw reads from pfs_image.dat block forever).
    std::string mounted_root;
    std::string dump_root;
    std::string skipped_log_path;
    uint64_t skipped_files = 0;
    uint64_t skipped_bytes = 0;

    static std::string derive_mounted_root(const std::string& pfs_path) {
        // ".../<TID>-app0-nest/pfs_image.dat" -> ".../<TID>-app0"
        std::string dir = std::filesystem::path(pfs_path).parent_path().string();
        const std::string suffix = "-nest";
        if (dir.size() > suffix.size() && dir.compare(dir.size() - suffix.size(), suffix.size(), suffix) == 0)
            return dir.substr(0, dir.size() - suffix.size());
        return "";
    }

    // Returns true if the file's first and last byte are readable through the mounted PFS.
    // If no mounted view exists we cannot probe and assume the file is available.
    static bool is_file_available(const std::string& fname, uint64_t size) {
        if (mounted_root.empty() || size == 0) return true;
        if (fname.compare(0, dump_root.size(), dump_root) != 0) return true;

        std::string rel = fname.substr(dump_root.size());
        std::string probe_path = mounted_root + (rel.empty() || rel[0] == '/' ? "" : "/") + rel;

        int fd = sceKernelOpen(probe_path.c_str(), O_RDONLY, 0);
        if (fd < 0) {
            // The mounted view exists (checked in extract()), but this file can't be opened:
            // that's exactly what happens for not-installed PlayGo chunks (e.g. 0x8002005f).
            log_warn("probe: cannot open %s (%x), treating as not installed", probe_path.c_str(), fd);
            return false;
        }
        char b;
        bool ok = sceKernelRead(fd, &b, 1) == 1;
        if (ok && size > 1) {
            ok = sceKernelLseek(fd, size - 1, SEEK_SET) >= 0 && sceKernelRead(fd, &b, 1) == 1;
        }
        sceKernelClose(fd);
        return ok;
    }

    static void record_skipped(const std::string& fname, uint64_t size) {
        skipped_files++;
        skipped_bytes += size;
        pfs_copied += size;
        if (pfs_copied > pfs_size) pfs_copied = pfs_size;
        pfs_progress = (uint64_t)(((float)pfs_copied / pfs_size) * 100.f);
        log_warn("Skipping not-installed file (PlayGo chunk missing): %s (%llu bytes)", fname.c_str(), size);
        if (!skipped_log_path.empty()) {
            std::ofstream out(skipped_log_path, std::ios::app);
            if (out) out << fname << "\t" << size << "\n";
        }
    }

    void memcpy_to_file(const std::string& fname, uint64_t ptr, uint64_t size, const std::string& title, const std::string& tid)
    {
        uint64_t bytes;
        uint64_t ix = 0;
        if (!is_file_available(fname, size)) {
            record_skipped(fname, size);
            return;
        }
        if(std::filesystem::exists(fname)){
		uint64_t existing_size = std::filesystem::file_size(fname);
    		if(existing_size == size) {
        		log_debug("Skipping already complete file: %s (%llu bytes)", fname.c_str(), size);
        		pfs_copied += size;
        		if (pfs_copied > pfs_size) pfs_copied = pfs_size;
        		pfs_progress = (uint64_t)(((float)pfs_copied / pfs_size) * 100.f);
        		return;
    		} else {
			log_debug("Partial file detected, redumping: %s (has %llu, need %llu)", 
                  		fname.c_str(), existing_size, size);
        		std::filesystem::remove(fname);
    		}
	}
        int fd = sceKernelOpen(fname.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK, 0777);
        log_info("---- name: %s, ptr %p, sz %zu, FD: %i", fname.c_str(), ptr, size, fd);
        if (fd > 0)
        {
            bool io_error = false;
            while (size > 0)
            {
                bytes = (size > PFS_DUMP_BUFFER) ? PFS_DUMP_BUFFER : size;
               // log_info("copying %llu bytes ix: %i size: %llu pfs prog: %llu/%llu", bytes, ix, size, pfs_copied, pfs_size);
                sceKernelLseek(pfs, ptr + ix * PFS_DUMP_BUFFER, SEEK_SET);
                int64_t rd = sceKernelRead(pfs, copy_buffer.get(), bytes);
                if (rd != (int64_t)bytes) {
                    log_error("Read error on %s at block %llu: got %lld of %llu bytes", fname.c_str(), ix, rd, bytes);
                    io_error = true;
                    break;
                }
                int64_t wr = sceKernelWrite(fd, copy_buffer.get(), bytes);
                if (wr != (int64_t)bytes) {
                    log_error("Write error on %s at block %llu: wrote %lld of %llu bytes", fname.c_str(), ix, wr, bytes);
                    io_error = true;
                    break;
                }
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
            if (io_error) {
                // Don't leave a truncated file behind; the resume logic would treat a
                // same-size file as complete, and a short one just gets redumped anyway.
                { std::error_code rm_ec; std::filesystem::remove(fname, rm_ec); }
                pfs_copied += size; // account for the remainder so progress keeps moving
                if (pfs_copied > pfs_size) pfs_copied = pfs_size;
                record_skipped(fname, 0);
            }
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
            skipped_files = 0;
            skipped_bytes = 0;
            mounted_root = derive_mounted_root(pfs_path);
            std::error_code mr_ec;
            if (!mounted_root.empty() && !std::filesystem::is_directory(mounted_root, mr_ec)) {
                log_warn("Mounted view %s not available, availability probe disabled", mounted_root.c_str());
                mounted_root.clear();
            }
            dump_root = output_path;
            while (dump_root.size() > 1 && dump_root.back() == '/') dump_root.pop_back();
            skipped_log_path = dump_root + "_skipped_files.txt";
            { std::error_code rm_ec; std::filesystem::remove(skipped_log_path, rm_ec); }
            log_info("Mounted root for availability probe: %s", mounted_root.empty() ? "(none)" : mounted_root.c_str());

            calculate_pfs((uint32_t)header->superroot_ino, (uint32_t)0, output_path);
            dump_pfs((uint32_t)header->superroot_ino, (uint32_t)0, output_path, tid, title);
            sceKernelClose(pfs);
            if (skipped_files > 0)
                log_warn("Dump finished with %llu skipped file(s) (%llu bytes) - see %s", skipped_files, skipped_bytes, skipped_log_path.c_str());

            return true;
        }
} // namespace pfs
