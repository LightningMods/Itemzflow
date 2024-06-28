#include <iostream>
#include <string>
#include <cstring>
#include <map>
#include <unordered_map>
#include <fstream>
#include <locale>
#include <codecvt>
#include "game_saves.h"
#include "sfo.hpp"

enum kpatch_type {
    byte,
    bytes16,
    bytes32,
    bytes64,
    bytes,
    float32,
    float64,
    utf8,
    utf16,
};

struct _trainer_struct
{
    // stores 1 string, if you have more than 1
    // use a vector of strings and use .size() to get the number of strings
    std::vector<std::string> patcher_title,
                             patcher_app_ver,
                             patcher_app_elf,
                             patcher_patch_ver,
                             patcher_name,
                             patcher_author,
                             patcher_note;
    std::vector<bool>        patcher_enablement;
    std::vector<std::string> controls_text;
};


struct _update_struct {
  // stores 1 string, if you have more than 1
  // use a vector of strings and use .size() to get the number of strings
  std::string update_title, update_tid;
  std::vector <int> required_fw;
  std::vector<std::string> update_version, update_size, update_json;
  std::vector<std::string> update_text;
};
static std::map<std::string, u8> patch_type = {
    {"byte",    kpatch_type::byte},
    {"bytes16", kpatch_type::bytes16},
    {"bytes32", kpatch_type::bytes32},
    {"bytes64", kpatch_type::bytes64},
    {"bytes",   kpatch_type::bytes},
    {"float32", kpatch_type::float32},
    {"float64", kpatch_type::float64},
    {"utf8",    kpatch_type::utf8},
    {"utf16",   kpatch_type::utf16},
};

// https://gist.github.com/gchudnov/c1ba72d45e394180e22f
extern std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
extern u32 num_title_id;
extern std::string key_title;
extern std::string key_app_ver;
extern std::string key_patch_ver;
extern std::string key_name;
extern std::string key_author;
extern std::string key_note;
extern std::string key_app_titleid;
extern std::string key_app_elf;
// initializers
extern u8 arr8[1];
extern u8 arr16[2];
extern u8 arr32[4];
extern u8 arr64[8];
extern u8 max_node;
// - [ 0     1      2    ]
// - [ type, addr, value ]
// patch data
extern std::string blank;
extern std::string type;
extern u64 addr;
extern std::ofstream cfg_data;
extern std::string patch_applied;
extern std::string patch_path_base;
extern std::string patch_settings;
extern bool is_first_eboot;
extern bool is_non_eboot_entry;
extern bool is_trainer_launch;
extern bool no_launch;
extern s32 itemflow_pid;
extern char patcher_notify_icon[64];
extern struct _trainer_struct trs;
extern u32 patch_current_index;
extern u32 patch_count;

extern bool is_using_plugin;

u64 patch_hash_calc(std::string title, std::string name, std::string app_ver = "00.00", std::string title_id = "CUSAXXXXX", std::string name_elf = "");
void write_enable(std::string cfg_file, std::string patch_name);
bool get_patch_path(std::string path);
std::string get_patch_data(std::string path);
bool get_metadata0(s32& pid, u64 ex_start);
void get_metadata1(struct _trainer_struct *tmp,
                   std::string patch_app_ver,
                   std::string patch_app_elf,
                   std::string patch_title,
                   std::string patch_ver,
                   std::string patch_name,
                   std::string patch_author,
                   std::string patch_note);
void patch_data(s32& pid, u32& items);
void trainer_launcher();
void dl_patches();

extern int update_current_index;
extern struct _update_struct update_info;
bool Fetch_Update_Details(const std::string &title_id, const std::string &title, _update_struct &updateStruct, bool &is_connection_issue);
