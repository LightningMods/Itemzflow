#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <unordered_map>
#include <fstream>
#include <locale>
#include <codecvt>
#include "game_saves.h"
#include "patcher.h"
#include "net.h"

unsigned char false_data[2] = { 0x30, 0xa }; // "0\n"
unsigned char true__data[2] = { 0x31, 0xa }; // "1\n"

// https://stackoverflow.com/a/30606613
std::vector<u8> HexToBytes(const std::string& hex) {
    std::vector<u8> bytes;
    for (u32 i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        u8 byte = (u8)strtol(byteString.c_str(), NULL, 16);
        bytes.push_back(byte);
    }
    return bytes;
}

u64 hash(const char *str) {
    u64 hash = 5381;
    u32 c;
    while ((c = *str++))
        hash = hash * 33 ^ c;
    return hash;
}

u64 patch_hash_calc(std::string title, std::string name, std::string app_ver, std::string title_id, std::string name_elf)
{
    std::string pre_hash = title + name + app_ver + title_id + name_elf;
    u64 hash_data = hash(pre_hash.c_str());
    log_debug("hash_input: %s", pre_hash.c_str());
    log_debug("hash_data: 0x%016lx", hash_data);
    return hash_data;
}

s32 Read_File(const char *File, char **Data, size_t *Size, int extra) {
    s32 res = 0;
    s32 fd = 0;

    log_debug("Reading File \"%s\"", File);

    fd = sceKernelOpen(File, 0, 0777);
    if (fd < 0) {
        log_debug("sceKernelOpen() 0x%08x", fd);
        res = fd;
        goto term;
    }

    *Size = sceKernelLseek(fd, 0, SEEK_END);
    if (*Size == 0) {
        log_debug("ERROR: File is empty %i", res);
        res = -1;
        goto term;
    }

    res = sceKernelLseek(fd, 0, SEEK_SET);
    if (res < 0) {
        log_debug("sceKernelLseek() 0x%08x", res);
        goto term;
    }

    *Data = (char *)malloc(*Size + extra);
    if (*Data == NULL) {
        log_debug("ERROR: malloc()\n");
        goto term;
    }

    res = sceKernelRead(fd, *Data, *Size);
    if (res < 0) {
        log_debug("sceKernelRead() 0x%08x", res);
        goto term;
    }

    res = sceKernelClose(fd);

    if (res < 0) {
        log_debug("ERROR: sceKernelClose() 0x%08x", res);
        goto term;
    }

    log_debug("File %s has been read - Res: %d - Size: %jd", File, res,
                 *Size);

    return res;

term:

    if (fd != 0) {
        sceKernelClose(fd);
    }

    return res;
}

int Write_File(const char *File, unsigned char *Data, size_t Size) {
    int32_t fd = sceKernelOpen(File, 0x200 | 0x002, 0777);
    if (fd < 0) {
        log_debug("Failed to make file \"%s\"", File);
        return 0;
    }
    log_debug("Writing File \"%s\" %li", File, Size);
    ssize_t written = sceKernelWrite(fd, Data, Size);
    log_debug("Written File \"%s\" %li", File, written);
    sceKernelClose(fd);
    return 1;
}

void write_enable(std::string hashid, std::string cfg_file, std::string patch_name) {
    char *buffer2;
    u64 size2;
    int res = Read_File(cfg_file.c_str(), &buffer2, &size2, 4);
    if (res == 0) {
        if (buffer2[0] == 0x30) {
            log_debug("setting %s true", cfg_file.c_str());
            Write_File(cfg_file.c_str(), true__data, 2);
            std::string patch_state = fmt::format("{} {}", getLangSTR(PATCH_SET1), patch_name);
            ani_notify(NOTIFI_GAME, patch_state, "");
        }
        if (buffer2[0] == 0x31) {
            log_debug("setting %s false", cfg_file.c_str());
            Write_File(cfg_file.c_str(), false_data, 2);
            std::string patch_state = fmt::format("{} {}", getLangSTR(PATCH_SET0), patch_name);
            ani_notify(NOTIFI_GAME, patch_state, "");
        }
    }
    return;
}

bool get_patch_path(std::string path){
    std::ifstream exists;
    bool ret = true;
    patch_file = fmt::format("{}/patches/json/{}.json", patch_path_base, path);
    log_info("got patch path: %s", patch_file.c_str());
    exists.open(patch_file);
    if (!exists) {
        log_info("file: %s does not exist", patch_file.c_str());
        ret = false;
    }
    log_info("ret: %i", ret);
    return ret;
}

std::string get_patch_data(std::string path){
    msgok(NORMAL, path);
    char *buffer;
    u64 size;
    std::string data;
    int ret = Read_File(path.c_str(), &buffer, &size, 32);
    if (ret == 0x80020002) {
        log_error("file not found, path: %s ret: 0x%08x", path.c_str(), ret);
        return data;
    }
    log_info("read_file %i", ret);
    data = buffer;
    return data;
}

bool get_metadata0(s32& pid, u64 ex_start) {
    return false;
}

bool get_metadata1(struct _trainer_struct *tmp,
                   const char *patch_app_ver1,
                   const char *patch_app_elf1,
                   const char *patch_title1,
                   const char *patch_ver1,
                   const char *patch_name1,
                   const char *patch_author1,
                   const char *patch_note1) {
    // convert to std::string hack
    std::string patch_app_ver = patch_app_ver1;
    std::string patch_app_elf = patch_app_elf1;
    std::string patch_title = patch_title1;
    std::string patch_ver = patch_ver1;
    std::string patch_name = patch_name1;
    std::string patch_author = patch_author1;
    std::string patch_note = patch_note1;
    // first time setup
    bool cfg_state = false;
    u64 hash_ret = patch_hash_calc(patch_title, patch_name, patch_app_ver, patch_file, patch_app_elf);
    std::string hash_id = fmt::format("{:#016x}", hash_ret);
    std::string hash_file = fmt::format("/data/GoldHEN/patches/settings/{0}.txt",  hash_id);
    char* buffer;
    u64 size;
    s32 ret = Read_File(hash_file.c_str(), &buffer, &size, 4);
    if (ret == 0x80020002){
        log_warn("file %s not found, initializing false. ret: 0x%08x\n", hash_file.c_str(), ret);
        Write_File(hash_file.c_str(), false_data, 2);
    }
    if(ret == 0){
        if (buffer[0] == 0x30){
            cfg_state = false;
        }
        else if (buffer[0] == 0x31){
            cfg_state = true;
        }
    }
    tmp->patcher_app_ver.push_back(patch_app_ver);
    tmp->patcher_app_elf.push_back(patch_app_elf);
    tmp->patcher_title.push_back(patch_title);
    tmp->patcher_patch_ver.push_back(patch_ver);
    tmp->patcher_name.push_back(patch_name);
    tmp->patcher_author.push_back(patch_author);
    tmp->patcher_note.push_back(patch_note);
    tmp->patcher_enablement.push_back(cfg_state);
    return true;
}

void trainer_launcher() {
    return;
}

void dl_patches(){
    std::string patch_zip = "patch1.zip";
    std::string patch_url = "https://github.com/GoldHEN/GoldHEN_Patch_Repository/raw/gh-pages/" + patch_zip;
    std::string patch_path = patch_path_base + "/" + patch_zip;
    std::string patch_build_path = patch_path_base + "/patches/json/build.txt";
    std::string patch_dl_msg;
    loadmsg(getLangSTR(PATCH_DL_QUESTION_YES));
    int ret = dl_from_url(patch_url.c_str(), patch_path.c_str());
    if (ret != 0) // 0 != failed
    {
        log_error("dl_from_url for %s failed with %i", patch_url.c_str(), ret);
        patch_dl_msg = fmt::format("Download failed with error code: {}", ret);
        goto error;
    }
    if(!extract_zip(patch_path.c_str(), patch_path_base.c_str())){
        patch_dl_msg = fmt::format("Failed to extract patch zip to {}", patch_path_base);
    } else {
        std::ifstream t(patch_build_path);
        std::stringstream buffer;
        buffer << t.rdbuf();
        patch_dl_msg = fmt::format("{}\n{}", getLangSTR(PATCH_DL_COMPLETE), buffer.str());
    }

error:
    sceMsgDialogTerminate();
    msgok(NORMAL, patch_dl_msg);
    return;
}
