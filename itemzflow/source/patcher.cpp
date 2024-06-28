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
std::string patch_file;
extern u32 patch_count;

u64 djb2(const char *str) {
    u64 hash = 5381;
    u32 c;
    while ((c = *str++))
        hash = hash * 33 ^ c;
    return hash;
}


u64 patch_hash_calc(std::string title, std::string name, std::string app_ver, std::string title_id, std::string name_elf)
{
    std::string pre_hash = title + name + app_ver + title_id + name_elf;
    u64 hash_data = djb2(pre_hash.c_str());
    log_debug("hash_input: %s", pre_hash.c_str());
    log_debug("hash_data: 0x%016lx", hash_data);
    return hash_data;
}

void write_enable(std::string cfg_file, std::string patch_name) {
    std::ifstream exists;
    exists.open(cfg_file);
    if (exists)
    {
        std::stringstream buffer;
        buffer << exists.rdbuf();
        char data[2];
        buffer.get(data, 2);
        std::ofstream output_hash_file_stream;
        std::string state_data = "";
        if (data[0] == 0x30)
        {
            output_hash_file_stream.open(cfg_file);
            state_data = "1\n";
            output_hash_file_stream << state_data;
            output_hash_file_stream.close();
            // msgok(MSG_DIALOG::NORMAL, "TRUE");
            log_debug("setting %s true", cfg_file.c_str());
            std::string patch_state = fmt::format("{} {}", getLangSTR(LANG_STR::PATCH_SET1), patch_name);
            ani_notify(NOTIFI::GAME, patch_state, "");
        }
        if (data[0] == 0x31)
        {
            output_hash_file_stream.open(cfg_file);
            state_data = "0\n";
            output_hash_file_stream << state_data;
            output_hash_file_stream.close();
            // msgok(MSG_DIALOG::NORMAL, "FALSE");
            log_debug("setting %s false", cfg_file.c_str());
            std::string patch_state = fmt::format("{} {}", getLangSTR(LANG_STR::PATCH_SET0), patch_name);
            ani_notify(NOTIFI::GAME, patch_state, "");
        }
    }
    return;
}

bool get_patch_path(std::string path){
    std::ifstream exists;
    bool ret = true;
    patch_file = fmt::format("{}/patches/xml/{}.xml", patch_path_base, path);
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
    return nullptr;
}

bool get_metadata0(s32& pid, u64 ex_start) {
    return false;
}

void get_metadata1(struct _trainer_struct *tmp,
                   std::string patch_app_ver,
                   std::string patch_app_elf,
                   std::string patch_title,
                   std::string patch_ver,
                   std::string patch_name,
                   std::string patch_author,
                   std::string patch_note) 
{
    // first time setup
    bool cfg_state = false;
    u64 hash_ret = patch_hash_calc(patch_title, patch_name, patch_app_ver, patch_file, patch_app_elf);
    std::string hash_id = fmt::format("{:#016x}", hash_ret);
    std::string hash_file = fmt::format("/data/GoldHEN/patches/settings/{0}.txt", hash_id);
    std::ifstream hash_file_stream;
    hash_file_stream.open(hash_file);

    if (!hash_file_stream)
    {
        log_warn("file %s not found, initializing false\n", hash_file.c_str());
        std::ofstream output_hash_file_stream;
        output_hash_file_stream.open(hash_file);
        std::string off_data = "0\n";
        output_hash_file_stream << off_data;
        output_hash_file_stream.close();
        // reopen the file with new data
        hash_file_stream.close();
        hash_file_stream.open(hash_file);
    }

    std::stringstream buffer;
    buffer << hash_file_stream.rdbuf();

    if(hash_file_stream)
    {
        char data[2];
        buffer.get(data, 2);
        if (data[0] == 0x30){
            cfg_state = false;
        }
        else if (data[0] == 0x31){
            cfg_state = true;
        }
    }

    hash_file_stream.close();
    tmp->patcher_app_ver.push_back(patch_app_ver);
    tmp->patcher_app_elf.push_back(patch_app_elf);
    tmp->patcher_title.push_back(patch_title);
    tmp->patcher_patch_ver.push_back(patch_ver);
    tmp->patcher_name.push_back(patch_name);
    tmp->patcher_author.push_back(patch_author);
    tmp->patcher_note.push_back(patch_note);
    tmp->patcher_enablement.push_back(cfg_state);
    return;
}

void trainer_launcher() {
    return;
}

void dl_patches(){
    std::string patch_zip = "patch1.zip";
    std::string patch_url = "https://github.com/GoldHEN/GoldHEN_Patch_Repository/raw/gh-pages/" + patch_zip;
    std::string patch_path = patch_path_base + "/" + patch_zip;
    std::string patch_build_path = patch_path_base + "/patches/misc/patch_ver.txt";
    std::string patch_dl_msg;
    loadmsg(getLangSTR(LANG_STR::PATCH_DL_QUESTION_YES));
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
        patch_dl_msg = fmt::format("{}\n{}", getLangSTR(LANG_STR::PATCH_DL_COMPLETE), buffer.str());
    }

error:
    sceMsgDialogTerminate();
    msgok(MSG_DIALOG::NORMAL, patch_dl_msg);
    return;
}
