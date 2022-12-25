// Copyright (c) 2021 Al Azif
// License: GPLv3

#include "elf.hpp"
#include "common.hpp"
#include "elf_64.hpp"
#include "elf_common.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "log.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>
#include <ps4sdk.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include "libshahash.h"
#include <user_mem.h>

#define KB(x)   ((size_t) (x) << 10)
#define MB(x)   ((size_t) (x) << 20)

namespace elf {
    uint64_t get_sce_header_offset(const std::string& path) {
        // Check for empty or pure whitespace path
        if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
            log_error("Empty path argument!");
        }

        // Check if file exists and is file
        if (!std::filesystem::is_regular_file(path.c_str())) {
            log_error("Input path does not exist or is not a file!");
        }

        // Open path
        std::ifstream self_input(path, std::ios::in | std::ios::binary);
        if (!self_input || !self_input.good()) {
            self_input.close();
            log_error("Cannot open file: ");
        }

        // Check to make sure file is a SELF
        if (!Check_ELF_Magic(path, SELF_MAGIC)) {
            self_input.close();
            log_error("Input path is not a SELF!");
        }

        // Read SELF header
        SelfHeader self_header;
        self_input.read((char*)&self_header, sizeof(self_header)); // Flawfinder: ignore
        if (!self_input.good()) {
            // Should never reach here... will affect coverage %
            self_input.close();
            log_error("Error reading SELF header!");
        }

        // Calculate ELF header offset from the number of SELF segments
        uint64_t elf_header_offset = sizeof(self_header) + self_header.num_of_segments * sizeof(SelfEntry);

        // Read ELF header
        Elf64_Ehdr elf_header;
        self_input.seekg(elf_header_offset, self_input.beg);
        self_input.read((char*)&elf_header, sizeof(elf_header)); // Flawfinder: ignore
        if (!self_input.good()) {
            self_input.close();
            log_error("Error reading ELF header!");
        }
        self_input.close();

        // Check ELF magic
        // Can this be less bad/robust?
        unsigned char elf_magic[4];
        elf_magic[0] = (ELF_MAGIC >> 24) & 0xFF;
        elf_magic[1] = (ELF_MAGIC >> 16) & 0xFF;
        elf_magic[2] = (ELF_MAGIC >> 8) & 0xFF;
        elf_magic[3] = (ELF_MAGIC >> 0) & 0xFF;

        if (std::memcmp(elf_header.e_ident, elf_magic, 4) != 0) {
            log_error("Error reading ELF magic!");
        }

        // Calculate SCE header offset from number of ELF entries
        uint64_t sce_header_offset = elf_header_offset + elf_header.e_ehsize + elf_header.e_phnum * elf_header.e_phentsize;

        // Align
        while (sce_header_offset % 0x10 != 0) {
            sce_header_offset++;
        }

        return sce_header_offset;
    }

    SceHeader get_sce_header(const std::string& path) {
        // Check for empty or pure whitespace path
        if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
            log_error("Empty path argument!");
        }

        // Check if file exists and is file
        if (!std::filesystem::is_regular_file(path)) {
            log_error("Input path does not exist or is not a file!");
        }

        // Open path
        std::ifstream self_input(path, std::ios::in | std::ios::binary);
        if (!self_input || !self_input.good()) {
            self_input.close();
            log_error("Cannot open file: %s", path.c_str());
        }

        // Check to make sure file is a SELF
        if (!Check_ELF_Magic(path, SELF_MAGIC)) {
            self_input.close();
            log_error("Input path is not a SELF!");
        }

        // Calculate SCE header offset from number of ELF entries
        uint64_t sce_header_offset = get_sce_header_offset(path);

        self_input.seekg(sce_header_offset, self_input.beg);
        SceHeader sce_header;
        self_input.read((char*)&sce_header, sizeof(sce_header)); // Flawfinder: ignore
        if (!self_input.good()) {
            self_input.close();
            log_error("Error reading SCE header!");
        }

        return sce_header;
    }

    SceHeaderNpdrm get_sce_header_npdrm(const std::string& path) {
        // Check for empty or pure whitespace path
        if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
            log_error("Empty path argument!");
        }

        // Check if file exists and is file
        if (!std::filesystem::is_regular_file(path.c_str())) {
            log_error("Input path does not exist or is not a file!");
        }

        // Open path
        std::ifstream self_input(path, std::ios::in | std::ios::binary);
        if (!self_input || !self_input.good()) {
            self_input.close();
            log_error("Cannot open file: %s", path.c_str());
        }

        // Check to make sure file is a SELF
        if (!Check_ELF_Magic(path, SELF_MAGIC)) {
            self_input.close();
            log_error("Input path is not a SELF!");
        }

        // Calculate SCE header offset from number of ELF entries
        uint64_t sce_header_offset = get_sce_header_offset(path);

        self_input.seekg(sce_header_offset, self_input.beg);
        SceHeaderNpdrm sce_header;
        self_input.read((char*)&sce_header, sizeof(sce_header)); // Flawfinder: ignore
        if (!self_input.good()) {
            self_input.close();
            log_error("Error reading SCE header!");
        }

        return sce_header;
    }


    bool Check_ELF_Magic(const std::string& path, uint32_t FILE_MAGIC) {
        // Check for empty or pure whitespace path
        if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
            log_error("Empty path argument!");
        }

        // Check if file exists and is file
        if (!std::filesystem::is_regular_file(path)) {
            log_error("Input path does not exist or is not a file!");
        }

        // Read SELF header
        uint32_t magic;

        // Open path
        int elf = sceKernelOpen(path.c_str(), O_RDONLY, 0);
        if (elf < 0) return false;

        sceKernelLseek(elf, 0, SEEK_SET);
        sceKernelRead(elf, &magic, sizeof(uint32_t));

        sceKernelClose(elf);

        return (__builtin_bswap32(magic) == FILE_MAGIC);

    }

    bool is_npdrm(const std::string& path) {
        // Check for empty or pure whitespace path
        if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
            log_error("Empty path argument!");
        }

        // Check if file exists and is file
        if (!std::filesystem::is_regular_file(path.c_str())) {
            log_error("Input path does not exist or is not a file!");
        }

        // Open path
        std::ifstream self_input(path, std::ios::in | std::ios::binary);
        if (!self_input || !self_input.good()) {
            self_input.close();
            log_error("Cannot open file: %s", path.c_str());
        }

        // Check if the file is a SELF
        if (!Check_ELF_Magic(path, SELF_MAGIC)) {
            self_input.close();
            log_error("Input path is not a SELF!");
        }

        // Read SELF header
        SelfHeader self_header;
        self_input.read((char*)&self_header, sizeof(self_header)); // Flawfinder: ignore
        if (!self_input.good()) {
            // Should never reach here... will affect coverage %
            self_input.close();
            log_error("Error reading SELF header!");
        }

        uint64_t program_type = self_header.program_type;
        while (program_type >= 0x10) {
            program_type -= 0x10;
        }

        // TODO: Is it just npdrm_exec that are considered NPDRM or is npdrm_dynlib as well? What about fake?
        if (program_type == 0x4) {
            return true;
        }

        return false;
    }

    std::string get_ptype(const std::string& path) {
        // Check for empty or pure whitespace path
        if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
            log_error("Empty path argument!");
        }

        // Check if file exists and is file
        if (!std::filesystem::is_regular_file(path.c_str())) {
            log_error("Input path does not exist or is not a file!");
        }

        // Open path (To check permissions)
        std::ifstream self_input(path, std::ios::in | std::ios::binary);
        if (!self_input || !self_input.good()) {
            self_input.close();
            log_error("Cannot open file: %s", path.c_str());
        }
        self_input.close();

        // Check if the file is a SELF. If it's not it *should* not have a SCE header
        if (!Check_ELF_Magic(path, SELF_MAGIC)) {
            log_error("Input path is not a SELF!");
        }

        uint64_t program_type;
        if (is_npdrm(path)) {
            program_type = get_sce_header_npdrm(path).program_type;
        }
        else {
            program_type = get_sce_header(path).program_type;
        }

        std::string output;
        switch (program_type) {
        case 0x1:
            output = "fake";
            break;
        case 0x4:
            output = "npdrm_exec";
            break;
        case 0x5:
            output = "npdrm_dynlib";
            break;
        case 0x8:
            output = "system_exec";
            break;
        case 0x9:
            output = "system_dynlib"; // Includes mono libraries
            break;
        case 0xC:
            output = "host_kernel";
            break;
        case 0xE:
            output = "secure_module";
            break;
        case 0xF:
            output = "secure_kernel";
            break;
        default:
            log_error("Unknown ptype!");
            break;
        }

        return output;
    }


    uint64_t get_paid(const std::string& path) {
        // Check for empty or pure whitespace path
        if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
            log_error("Empty path argument!");
        }

        // Check if file exists and is file
        if (!std::filesystem::is_regular_file(path.c_str())) {
            log_error("Input path does not exist or is not a file!");
        }

        // Open path (To check permissions)
        std::ifstream self_input(path, std::ios::in | std::ios::binary);
        if (!self_input || !self_input.good()) {
            self_input.close();
            log_error("Cannot open file: %s", path.c_str());
        }
        self_input.close();

        // Check if the file is a SELF. If it's not it *should* not have a SCE header
        if (!Check_ELF_Magic(path, SELF_MAGIC)) {
            log_error("Input path is not a SELF!");
        }

        if (is_npdrm(path)) {
            return get_sce_header_npdrm(path).program_authority_id;
        }

        return get_sce_header(path).program_authority_id;
    }

    uint64_t get_app_version(const std::string& path) {
        // Check for empty or pure whitespace path
        if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
            log_error("Empty path argument!");
        }

        // Check if file exists and is file
        if (!std::filesystem::is_regular_file(path.c_str())) {
            log_error("Input path does not exist or is not a file!");
        }

        // Open path (To check permissions)
        std::ifstream self_input(path, std::ios::in | std::ios::binary);
        if (!self_input || !self_input.good()) {
            self_input.close();
            log_error("Cannot open file: %s", path.c_str());
        }
        self_input.close();

        // Check if the file is a SELF. If it's not it *should* not have a SCE header
        if (!Check_ELF_Magic(path, SELF_MAGIC)) {
            log_error("Input path is not a SELF!");
        }

        if (is_npdrm(path)) {
            return get_sce_header_npdrm(path).app_version;
        }

        return get_sce_header(path).app_version;
    }

    uint64_t get_fw_version(const std::string& path) {
        // Check for empty or pure whitespace path
        if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
            log_error("Empty path argument!");
        }

        // Check if file exists and is file
        if (!std::filesystem::is_regular_file(path.c_str())) {
            log_error("Input path does not exist or is not a file!");
        }

        // Open path (To check permissions)
        std::ifstream self_input(path, std::ios::in | std::ios::binary);
        if (!self_input || !self_input.good()) {
            self_input.close();
            log_error("Cannot open file: %s", path.c_str());
        }
        self_input.close();

        // Check if the file is a SELF. If it's not it *should* not have a SCE header
        if (!Check_ELF_Magic(path, SELF_MAGIC)) {
            log_error("Input path is not a SELF!");
        }

        if (is_npdrm(path)) {
            return get_sce_header_npdrm(path).fw_version;
        }

        return get_sce_header(path).fw_version;
    }

    std::vector<unsigned char> get_digest(const std::string& path) {
        // Check for empty or pure whitespace path
        if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
            log_error("Empty path argument!");
        }

        // Check if file exists and is file
        if (!std::filesystem::is_regular_file(path.c_str())) {
            log_error("Input path does not exist or is not a file!");
        }

        // Open path (To check permissions)
        std::ifstream self_input(path, std::ios::in | std::ios::binary);
        if (!self_input || !self_input.good()) {
            self_input.close();
            log_error("Cannot open file: %s", path.c_str());
        }
        self_input.close();

        // Check if the file is a SELF. If it's not it *should* not have a SCE header
        if (!Check_ELF_Magic(path, SELF_MAGIC)) {
            log_error("Input path is not a SELF!");
        }

        std::vector<unsigned char> digest;
        if (is_npdrm(path)) {
            SceHeaderNpdrm sce_header = get_sce_header_npdrm(path);
            for (size_t i = 0; i < sizeof(sce_header.digest); i++) {
                digest.push_back(sce_header.digest[i]);
            }
        }
        else {
            SceHeader sce_header = get_sce_header(path);
            for (size_t i = 0; i < sizeof(sce_header.digest); i++) {
                digest.push_back(sce_header.digest[i]);
            }
        }

        return digest;
    }

    // https://www.psdevwiki.com/ps4/Auth_Info
    std::vector<unsigned char> get_auth_info(const std::string& path) {
        // Check for empty or pure whitespace path
        if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
            log_error("Empty path argument!");
        }

        // Check if file exists and is file
        if (!std::filesystem::is_regular_file(path.c_str())) {
            log_error("Input path does not exist or is not a file!");
        }

        // Open path (To check permissions)
        std::ifstream self_input(path, std::ios::in | std::ios::binary);
        if (!self_input || !self_input.good()) {
            self_input.close();
            log_error("Cannot open file: %s", path.c_str());
        }
        self_input.close();

        // Check if the file is a SELF
        if (!Check_ELF_Magic(path, SELF_MAGIC)) {
            log_error("Input path is not a SELF!");
        }

#if defined(__ORBIS__)
        // TODO:
        // This may be better as a Mira fuctions to expose the required kernel functions as getting the auth_info is done in kernel
        // int kern_get_self_auth_info(struct thread* td, const char* path, int pathseg, char* info);
        // int self_kget_auth_info(struct thread *td, struct path_kmethod_uap_t *uap) {
        //   char auth_info[0x88];
        //   memset(auth_info, 0, sizeof(auth_info));
        //
        //   const char *path = uap->path;
        //   int res = kern_get_self_auth_info(td, path, (int)UIO_SYSSPACE, auth_info);
        //   if (res == 0) {
        //     khexdump("AUTH_INFO", auth_info, 0x88);
        //     kdprintf("END_AUTH_INFO\n");
        //   } else {
        //     kdprintf("Failed to get AUTH_INFO\n");
        //   }
        // }
#elif defined(__TEST__)
        // TODO: Some sort of output to know the above code is functioning as much as can be expected
#else
        // TODO: Some sort of output to know the above code is functioning as much as can be expected
#endif

        std::vector<unsigned char> blah;

        return blah;
    }

    bool is_valid_decrypt(const std::string& original_path, const std::string& decrypted_path) {
        // Check for empty or pure whitespace path
        if (original_path.empty() || std::all_of(original_path.begin(), original_path.end(), [](char c) { return std::isspace(c); })) {
            log_error("Empty original path argument!");
        }

        // Check if file exists and is file
        if (!std::filesystem::is_regular_file(original_path.c_str())) {
            log_error("Input original path does not exist or is not a file!");
        }

        // Open path
        std::ifstream original_self(original_path, std::ios::in | std::ios::binary);
        if (!original_self || !original_self.good()) {
            original_self.close();
            log_error("Cannot open file: %s", original_path.c_str());
        }
        original_self.close();

        // Check if file is a SELF
        if (!Check_ELF_Magic(original_path, SELF_MAGIC)) {
            log_error("Input original path is not a SELF!");
        }

        // Check for empty or pure whitespace path
        if (decrypted_path.empty() || std::all_of(decrypted_path.begin(), decrypted_path.end(), [](char c) { return std::isspace(c); })) {
            log_error("Empty decrypted path argument!");
        }

        // Check if file exists and is file
        if (!std::filesystem::is_regular_file(decrypted_path.c_str())) {
            log_error("Input decrypted path does not exist or is not a file!");
        }

        // Open path
        std::ifstream decrypted_elf(decrypted_path, std::ios::in | std::ios::binary);
        if (!decrypted_elf || !decrypted_elf.good()) {
            decrypted_elf.close();
            log_error("Cannot open file: %s", decrypted_path.c_str());
        }

        // Check if file is an ELF
        if (!Check_ELF_Magic(decrypted_path)) {
            decrypted_elf.close();
            log_error("Input decrypted path is not an ELF!");
        }

        // Read digest
        unsigned char digest[32];
        std::vector<unsigned char> original_digest_from_self = get_digest(original_path);
        for (size_t i = 0; i < sizeof(digest); i++) {
            digest[i] = original_digest_from_self[i];
        }

        unsigned char calculated_digest[sizeof(digest)];

        SHA256_CTX context;
        SHA256_Init(&context);

        while (decrypted_elf.good()) {
            unsigned char buffer[PAGE_SIZE];
            decrypted_elf.read((char*)buffer, sizeof(buffer)); // Flawfinder: ignore

            SHA256_Update(&context, buffer, decrypted_elf.gcount());
        }
        decrypted_elf.close();

        SHA256_Final(&context, calculated_digest);
#if 1

        char sha_digest[33] = { 0 };
        char sha_calcdigest[33] = { 0 };

        for (int i = 0; i < 32; ++i) {
            snprintf(&sha_digest[i], 32, "%0x", (unsigned int)digest[i]);
        }


        for (int i = 0; i < 32; ++i) {
            snprintf(&sha_calcdigest[i], 32, "%0x", (unsigned int)calculated_digest[i]);
        }

        log_info("[libshahash][SHA256] ELF Digest Hash: %s Calc'd Digest Hash: %s", sha_digest, sha_calcdigest);
#endif

        if (std::memcmp(calculated_digest, digest, sizeof(digest)) != 0) {
            log_info("File %s has failed validation", decrypted_path.c_str());
            return false;
        }
        else
            log_info("File %s Successfully validated", decrypted_path.c_str());

        return true;
    }

    // This is done in other dumpers but is it necessary?
    void zero_section_header(const std::string& path) {
        // Check for empty or pure whitespace path
        if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
            log_error("Empty path argument!");
        }

        // Check if file exists and is file
        if (!std::filesystem::is_regular_file(path.c_str())) {
            log_error("Input path does not exist or is not a file!");
        }

        // Open path
        std::ifstream elf_path(path, std::ios::in | std::ios::binary);
        if (!elf_path || !elf_path.good()) {
            elf_path.close();
            log_error("Cannot open file: %s", path.c_str());
        }

        // Check if file is an ELF
        if (!Check_ELF_Magic(path, ELF_MAGIC)) {
            elf_path.close();
            log_error("Input path is not an ELF!");
        }

        elf_path.seekg(0, elf_path.beg);
        Elf64_Ehdr elf_header;
        elf_path.seekg(0, elf_path.beg);
        elf_path.read((char*)&elf_header, sizeof(elf_header)); // Flawfinder: ignore
        if (!elf_path.good()) {
            // Should never reach here... will affect coverage %
            elf_path.close();
            log_error("Error reading ELF header!");
        }
        elf_path.close();

        // Zero section headers as they are invalid
        elf_header.e_shoff = 0;
        elf_header.e_shnum = 0;
        elf_header.e_shentsize = 0;
        elf_header.e_shstrndx = 0;

        std::ofstream output_file(path, std::ios::in | std::ios::out | std::ios::binary);
        if (!output_file || !output_file.good()) {
            // Should never reach here... will affect coverage %
            output_file.close();
            log_error("Cannot open file: %s", path.c_str());
        }

        output_file.seekp(0, output_file.beg);
        output_file.write((char*)&elf_header, sizeof(elf_header));
        output_file.close();
    }


    bool write_decrypt_segment_to(int fdr, int fdw, uint64_t index, uint64_t offset, size_t size)
    {

        uint64_t outSize = size;
        uint64_t realOffset = (index << 32) | offset;
        while (outSize > 0)
        {
            size_t bytes = (outSize > DECRYPT_SIZE) ? DECRYPT_SIZE : outSize;
            uint8_t* addr = (uint8_t*)mmap(0, bytes, PROT_READ, MAP_PRIVATE | 0x80000, fdr, realOffset);
            if (addr != MAP_FAILED)
            {
                write(fdw, addr, bytes);
                munmap(addr, bytes);
            }
            else
            {
                log_error("mmap segment [%lu] err(%d) : %s", index, errno, strerror(errno));
                return false;
            }

            outSize -= bytes;
            realOffset += bytes;
        }
        return true;
    }

    int is_segment_in_other_segment(Elf64_Phdr* phdr, int index, Elf64_Phdr* phdrs, int num) {
        for (int i = 0; i < num; i += 1) {
            Elf64_Phdr* p = &phdrs[i];
            if (i != index) {
                if (p->p_filesz > 0) {
                    if ((phdr->p_offset >= p->p_offset) && ((phdr->p_offset + phdr->p_filesz) <= (p->p_offset + p->p_filesz))) {
                        return TRUE;
                    }
                }
            }
        }
        return FALSE;
    }


    SegmentBufInfo* parse_phdr(Elf64_Phdr* phdrs, int num, int* segBufNum) {
        SegmentBufInfo* infos = (SegmentBufInfo*)malloc(sizeof(SegmentBufInfo) * num);
        int segindex = 0;
        for (int i = 0; i < num; i += 1) {
            Elf64_Phdr* phdr = &phdrs[i];


            if (phdr->p_filesz > 0) {
                if ((!is_segment_in_other_segment(phdr, i, phdrs, num)) || (phdr->p_type == 0x6fffff01)) {
                    SegmentBufInfo* info = &infos[segindex];
                    segindex += 1;
                    info->index = i;
                    info->bufsz = (phdr->p_filesz + (phdr->p_align - 1)) & (~(phdr->p_align - 1));
                    info->filesz = phdr->p_filesz;
                    info->fileoff = phdr->p_offset;
                    info->enc = (phdr->p_type != 0x6fffff01) ? TRUE : FALSE;

                }
            }
        }
        *segBufNum = segindex;
        return infos;
    }

    bool do_dump(const char* saveFile, int fd, SegmentBufInfo* segBufs, int segBufNum, Elf64_Ehdr* ehdr) {

        unlink(saveFile);

        int sf = sceKernelOpen(saveFile, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (sf > 0) {
            size_t elfsz = 0x40 + ehdr->e_phnum * sizeof(Elf64_Phdr);
            write(sf, ehdr, elfsz);

            for (int i = 0; i < segBufNum; i += 1) {


                if (segBufs[i].enc)
                {
                    lseek(sf, segBufs[i].fileoff, SEEK_SET);
                    if (!write_decrypt_segment_to(fd, sf, segBufs[i].index, 0, segBufs[i].filesz))
                    {
                        log_error("write_decrypt_segment_to failed for %s", saveFile);
                        sceKernelClose(sf);
                        return true;
                    }
                }
                else
                {

                    if (segBufs[i].bufsz > MB(10))
                        log_error("segBufs[i].bufsz > MB(10)");

                    uint8_t* buf = (uint8_t*)malloc(segBufs[i].bufsz);
                    memset(buf, 0, segBufs[i].bufsz);

                    lseek(fd, -segBufs[i].filesz, SEEK_END);
                    read(fd, buf, segBufs[i].filesz);
                    lseek(sf, segBufs[i].fileoff, SEEK_SET);
                    write(sf, buf, segBufs[i].filesz);

                    free(buf);
                }
            }
            sceKernelClose(sf);
        }
        else
            log_error("open %s err : %x", saveFile, sf);



        return false;
    }

    int decrypt_and_dump_self(const char* selfFile, const char* saveFile) {

        int fd = sceKernelOpen(selfFile, O_RDONLY, 0);
        if (fd > 0) {
            void* addr = mmap(0, 0x4000, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
            if (addr != MAP_FAILED) {
                log_info("mmap %s : %p, fd: %i", selfFile, addr, fd);

                uint16_t snum = *(uint16_t*)((uint8_t*)addr + 0x18);
                Elf64_Ehdr* ehdr = (Elf64_Ehdr*)((uint8_t*)addr + 0x20 + snum * 0x20);

                // shdr fix
                ehdr->e_shoff = ehdr->e_shentsize = ehdr->e_shnum = ehdr->e_shstrndx = 0;

                Elf64_Phdr* phdrs = (Elf64_Phdr*)((uint8_t*)ehdr + 0x40);


                int segBufNum = 0;
                SegmentBufInfo* segBufs = parse_phdr(phdrs, ehdr->e_phnum, &segBufNum);
                bool flag = do_dump(saveFile, fd, segBufs, segBufNum, ehdr);
                log_info("flag: %i", flag);
                free(segBufs);
                munmap(addr, 0x4000);

                sceKernelClose(fd);
                return flag;
            }
            else {
                log_fatal("mmap file %s err : %s", selfFile, strerror(errno));
                sceKernelClose(fd);
                return true;
            }
            sceKernelClose(fd);
        }
        else {
            log_fatal("open %s err : %s", selfFile, strerror(errno));
            return true;
        }
    }


    // The following code inspired from:
    // - https://github.com/AlexAltea/orbital
    // - https://github.com/xvortex/ps4-dumper-vtx
    bool decrypt_dir(const std::string& input, const std::string& output) {

        DIR* dir;
        struct dirent* dp;
        struct stat info;
        char src_path[1024], dst_path[1024];

        dir = opendir(input.c_str());
        if (!dir)
            return true;

        while ((dp = readdir(dir)) != NULL)
        {
            if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
            {
                // do nothing (straight logic)
            }
            else
            {
                snprintf(src_path, 1023, "%s/%s", input.c_str(), dp->d_name);
                snprintf(dst_path, 1023, "%s/%s", output.c_str(), dp->d_name);
                if (!stat(src_path, &info))
                {
                    if (S_ISDIR(info.st_mode))
                    {
                        mkdir(src_path, 0777);
                        decrypt_dir(src_path, dst_path);
                    }
                    else
                        if (S_ISREG(info.st_mode))
                        {

                            if (Check_ELF_Magic(src_path, SELF_MAGIC))
                            {

                                if (decrypt_and_dump_self(src_path, dst_path))
                                {
                                    //if (!is_valid_decrypt(src_path, dst_path))
                                        log_error("failed to decrypt %s -> %s", src_path, dst_path);
                                }
                                
                            }
                            else
                               log_info("file: %s is NOT a self", src_path);
                            
                        }
                }
            }
        }
        closedir(dir);

        return true;


    }
} // namespace elf
