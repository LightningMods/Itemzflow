/*
    listing local folder using getdents()
*/

#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h> // malloc, qsort, free
#include <unistd.h> // close
#include <ctype.h>
#include "defines.h"
#include "sfo.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <feature_classes.hpp>

/*
  Glibc does not provide a wrapper for
  getdents, so we call it by syscall()
*/
#include <sys/syscall.h> // SYS_getdents
#include <sys/param.h>   // MIN
#include <errno.h>
#ifndef __ORBIS__
#include <sys/vfs.h>
#else
#include "installpkg.h"
#endif

#include <time.h>

#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include "utils.h"
#include <sstream>

int HDD_count = -1;
const char *SPECIAL_XMB_ICON_TID[] = {APP_HOME_DATA_TID, APP_HOME_HOST_TID, DEBUG_SETTINGS_TID};

bool if_exists(const char *path)
{
    /*
    int dfd = open(path, O_RDONLY, 0); // try to open dir
    if (dfd < 0) {
      // log_info("path %s, errno %s", path, strerror(errno));
        return false;
    }
    else
      close(dfd);
    */
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

bool filter_entry_on_IDs(const char *entry)
{
    if (strlen(entry) != 9)
        return false;

    unsigned int nid;
    char id[5],
        tmp[32];

    int o = sscanf(entry, "%c%c%c%c%5u",
                   &id[0], &id[1], &id[2], &id[3], &nid);
    id[4] = '\0';
    if (o == 5)
    {
        int res = strlen(id); // ABCD...
        if (res != 4 || nid > 99999)
            goto fail; // 01234

        snprintf(&tmp[0], 31, "%s%.5u", id, nid);

        if (strlen(tmp) != 9)
            goto fail;
        // passed
        //      log_info("%s - %s looks valid", entry, tmp);

        return true;
    }
fail:
    return false;
}

std::string app_name_callback = "NA";

static int app_info(void *, const char *s, const char *n,
                    const char *value, int)
{

    if (strcmp("APP_NAME", s) == 0 && strcmp("CUSTOM_NAME", n) == 0)
    {
        app_name_callback = value;
    }

    return 0;
}
bool getEntries(std::string path, std::vector<std::string> &cvEntries, FS_FILTER filter, bool for_fs_browser = false)
{
    struct dirent *pDirent;
    struct stat st;
    DIR *pDir = NULL;

    if (path.empty())
    {
        fmt::print("path is empty, {}", path);
        return false;
    }

    pDir = opendir(path.c_str());
    if (pDir == NULL)
    {
        fmt::print("opendir({}) failed {}", path, strerror(errno));
        return false;
    }

    while ((pDirent = readdir(pDir)) != NULL)
    {
        
        std::string upper_case_ext;
        std::string filename = pDirent->d_name;
        std::string full_path = path + "/" + filename;
		
        if (filename == "." || filename == ".." )
            continue;

        int err = stat(full_path.c_str(), &st);
		if (err == -1) {
			log_error("%s stat returned %s", full_path.c_str(), strerror(errno));
            continue;
        }

        //log_info("d_name %s pDirent->d_type %i", pDirent->d_name, pDirent->d_type);
        // Dir is title_id, it WILL go through 3 filters
        // so lets assume they are vaild until the filters say otherwise
        if (S_ISDIR(st.st_mode) && filter != FS_MP3_ONLY) // MP3S ONLY NO DIR NAMES
            cvEntries.push_back(filename);
        else if (filter != FS_FOLDER) // if folders only do ntohing
        {
            std::string ext;
            switch (filter)
            {
            case FS_MP3_ONLY:
            case FS_MP3:
                ext = ".mp3";
                break;
            case FS_FONT:
                ext = ".ttf";
                break;
            case FS_PNG:
                ext = ".png";
                break;
            case FS_PKG:
                ext = ".pkg";
                break;
            case FS_ZIP:
                ext = ".zip";
                break;
            case FS_NONE:
                cvEntries.push_back(filename);
            default:
                ext = ".lm_is_da_best"; // an ext no one will ever have
                break;
            }
            if (filename.rfind(ext) != std::string::npos)
                cvEntries.push_back(filename);
            else
            { // try with all caps
                // Transform the string to uppercase using the lambda function 
                upper_case_ext = ext;
                std::transform(upper_case_ext.begin(), upper_case_ext.end(), upper_case_ext.begin(), ::toupper);
                if(filename.rfind(upper_case_ext) != std::string::npos){
                        cvEntries.push_back(filename);
                }
            }
        }
    }
    if (closedir(pDir) != 0)
    {
        log_info("closedir failed %s", strerror(errno));
        return false;
    }

    if(for_fs_browser){
        // if we are in the fs browser we want to sort the entries
        std::sort(cvEntries.begin(), cvEntries.end(), [](std::string a, std::string b)
        { 
            std::transform(a.begin(), a.end(), a.begin(), ::tolower);
            std::transform(b.begin(), b.end(), b.begin(), ::tolower);

            return a.compare(b) < 0;
        });
    }


    return true;
}



#define MEM_DEBUG 0
// function for virtual itemzflow apps
bool is_if_vapp(std::string tid){
    
    if(tid == "ITEM00001"){
       log_info("ItemzCore selected????");
       return true;
    }

    return false;
}

bool is_vapp(std::string tid)
{

    // Only fake *kits and real *kits have this file, so we need to check
    // if they have APP_HOME etc...
    if (if_exists("/system/sys/set_upper.self"))
    {
        for (int i = 0; i < sizeof SPECIAL_XMB_ICON_TID / sizeof SPECIAL_XMB_ICON_TID[0]; i++)
        {
            if (tid == SPECIAL_XMB_ICON_TID[i])
                return true;
        }
    }
    else
    {
        if(!get->setting_strings[FUSE_PC_NFS_IP].empty()){
           if(tid == APP_HOME_HOST_TID)
             return true;
        }
    }
    

    return false;
}


void init_xmb_special_icons(const char *tid, std::vector<std::string>& cvEntries)
{
   if(is_vapp(tid)){
     cvEntries.push_back(tid);
     // update HDD count
     HDD_count = cvEntries.size();
     log_debug("[XMB OPS] Added special VAPP tid %s", tid);
   }
}
std::vector<uint8_t> readFile(std::string filename)
{
    // open the file:
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        log_info("Failed to open %s", filename.c_str());
        return std::vector<uint8_t>();
    }

    // Stop eating new lines in binary mode!!!
    file.unsetf(std::ios::skipws);

    // get its size:
    std::streampos fileSize;

    file.seekg(0, std::ios::end);
    fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // reserve capacity
    std::vector<uint8_t> vec;
    vec.reserve(fileSize);

    // read the data:
    vec.insert(vec.begin(),
               std::istream_iterator<uint8_t>(file),
               std::istream_iterator<uint8_t>());

    return vec;
}

bool AppExists(std::string tid, std::string &pkg_path, item_t& out_vec)
{
#if defined(__ORBIS__)
    pkg_path = fmt::format("/user/app/{}/app.pkg", tid);
    if (!if_exists(pkg_path.c_str()))
    {
        pkg_path = fmt::format("/mnt/ext0/user/app/{}/app.pkg", tid);
        if (!if_exists(pkg_path.c_str())) {
            pkg_path = fmt::format("/mnt/ext1/user/app/{}/app.pkg", tid);
        }
        else {
            out_vec.flags.is_ext_hdd = true;
        }
    }
#else
    pkg_path = fmt::format("{}/{}/app.pkg", APP_PATH("apps"), tid);
#endif

    return true;
}
// the Settings
extern ItemzSettings set,
    *get;

// each token_data is filled by dynalloc'd mem by strdup!
void index_items_from_dir(ThreadSafeVector<item_t> &out_vec, std::string dirpath, std::string dirpath2, Sort_Category category)
{
    int ext_count = -1;
    std::string pkg_path;
    std::string tmp, no_icon_path;

#if defined(__ORBIS__)
    if (if_exists("/user/appmeta/ITEM00001/icon0.png"))
        no_icon_path = "/user/appmeta/ITEM00001/icon0.png";
    else
        no_icon_path = "/user/appmeta/external/ITEM00001/icon0.png";
#else
    no_icon_path = asset_path("icon0.png");
#endif

    log_info("Starting index_items_from_dir ...");

    Favorites favorites;
    if(favorites.loadFromFile(APP_PATH("favorites.dat")))
       log_info("Favorites have beend loaded!");
    else
       log_warn("Failed to load favorites.dat does it exist??");

    std::vector<std::string> cvEntries;
    if (!getEntries(dirpath, cvEntries, FS_FOLDER))
        log_info("failed to get dir");
    else
        log_debug("[Dir 1] Found %i Apps in: %s", (HDD_count = cvEntries.size()), dirpath.c_str());
        
    for (int i = 0; i < sizeof SPECIAL_XMB_ICON_TID / sizeof SPECIAL_XMB_ICON_TID[0]; i++)
         init_xmb_special_icons(SPECIAL_XMB_ICON_TID[i], cvEntries);

    if (if_exists(dirpath2.c_str()))
    {
        if (!getEntries(dirpath2, cvEntries, FS_FOLDER))
            log_warn("Unable to locate Apps on %s", dirpath2.c_str());
        else
        {
            ext_count = (cvEntries.size() - HDD_count);
            log_debug("[Dir 2] Found %i Apps in: %s", ext_count, dirpath2.c_str());
        }
    }


    //remove dups
    // Sort the vector to group duplicates together
    std::sort(cvEntries.begin(), cvEntries.end());

    // Erase duplicates
    cvEntries.erase(std::unique(cvEntries.begin(), cvEntries.end()), cvEntries.end());

    uint32_t entCt = cvEntries.size();
    log_debug("Have %d entries:", entCt);
#if 0
    for (int ix = 0; ix < entCt; ix++) {
        std::string fname = cvEntries[ix];
        log_debug("ent[%06d] size: %d\t\"%s\"", ix, fname.length(), fname.c_str());
    }
#endif
    // add one for reserve
    entCt++;

    item_t if_app;
    if_app.info.picpath = no_icon_path;
    if_app.info.name = "ItemzCore_ls_dir";
    if_app.info.id = "ITEM99999";
    if_app.count.HDD_count = HDD_count - 1;
    out_vec.push_back(if_app);

    for (int not_used = 1; not_used < entCt; not_used++)
    {
        item_t item;
        // we use just those token_data
        std::string fname = cvEntries[not_used - 1];
        if (!is_vapp(fname.c_str()))
        {
            if (!filter_entry_on_IDs(fname.c_str()))
            {
                log_debug("[F1] Filtered out %s", fname.c_str());
                continue;
            }

#if 0 // doesnt work for recoverying so for now we ignore
      //  skip first reserved: take care
             bool does_app_exist = false;
            //extra filter: see if sonys pkg api can vaildate it
            if (app_inst_util_is_exists(fname.c_str(), &does_app_exist)) {
                if (!does_app_exist) {
                    log_debug("[F2] Filtered out %s, the App doesnt exist", fname.c_str());
                    continue;
                }
            }
#else
            if (!AppExists(fname, pkg_path, item)){
                log_debug("[F2] Filtered out %s, the App doesnt exist", fname.c_str());
                continue;
            }
#endif
        }

        //(i >= HDD_count - 1) ? (item.flags.is_ext_hdd = true) : (item.flags.is_ext_hdd = false);

        if (category == FILTER_HOMEBREW)
        {
            if (fname.find("CUSA") != std::string::npos || fname.find("PPSA") != std::string::npos)
                continue;
        }
        else if ( category == FILTER_RETAIL_GAMES)
        {
            if (fname.find("CUSA") == std::string::npos && fname.find("PPSA") == std::string::npos)
                continue;
        }

        // dynalloc for user tokens
        item.count.token_c = TOTAL_NUM_OF_TOKENS;
        item.info.name= item.info.id = fname;
        item.extra_data.extra_sfo_data.clear();
        item.flags.is_ph = false;
#if defined(__ORBIS__)

        tmp = fmt::format("/user/appmeta/{}/icon0.png", fname);
        if (!if_exists(tmp.c_str()))
        {
            tmp = fmt::format("/user/appmeta/external/{}/icon0.png", fname);
            if (!if_exists(tmp.c_str()))
                tmp = "/user/appmeta/ITEM00001/icon0.png";
        }

#else // on pc
        tmp = fmt::format("{}/{}/icon0.png", std::string(APP_PATH("apps")), fname);
#endif

        item.info.picpath = tmp;
        // don't try lo load
        item.icon.texture = GL_NULL; // XXX load_png_asset_into_texture(tmp);
        item.info.package = pkg_path;

#if defined(__ORBIS__)
        item.flags.is_fpkg = is_fpkg(item.info.package);
        item.flags.app_vis = AppDBVisiable(item.info.id, VIS_READ, 0);

        tmp = fmt::format("/system_data/priv/appmeta/{}/param.sfo", fname);
        if (!if_exists(tmp.c_str())) {
            log_info("%s: sfo %s does not exist", fname.c_str(), tmp.c_str());
            tmp = fmt::format("/system_data/priv/appmeta/external/{}/param.sfo", fname);
        }

#else // on pc
        tmp = fmt::format("{}/{}/param.sfo", APP_PATH("apps"), fname);

#endif
        item.info.sfopath = tmp;

        if ((category == FILTER_FPKG && !item.flags.is_fpkg) || (category == FILTER_RETAIL_GAMES && item.flags.is_fpkg))
            continue;

        item.flags.is_favorite = favorites.isFavorite(item);
        if(category == FILTER_FAVORITES && !item.flags.is_favorite)
            continue;

        // read sfo
        if (is_vapp(fname.c_str()))
        {
            if(fname == APP_HOME_DATA_TID)
            {
                item.info.picpath = "/data/APP_HOME.png";
                item.info.name = "APP_HOME(Data)";
                item.info.id = APP_HOME_DATA_TID;
            }
            else if(fname == DEBUG_SETTINGS_TID)
            {
                item.info.picpath = "/data/debug.png";
                item.info.name = "PS4 Debug Settings";
                item.info.id = DEBUG_SETTINGS_TID;
            }
            else if(fname == APP_HOME_HOST_TID)
            {
                item.info.picpath = asset_path("app_home_host.png");
                item.info.name = fmt::format("{} (/HOSTAPP)", getLangSTR(LANG_STR::REMOTE_APP_TITLE));
                item.info.id = APP_HOME_HOST_TID;
            }

            item.info.version = "0.00";
            item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("CATEGORY", "gde"));
            item.flags.is_vapp = true;

            log_info("[XMB OP] SFO Name: %s", item.info.name.c_str());
            log_info("[XMB OP] SFO ID: %s", item.info.id.c_str());
            log_info("[XMB OP] SFO VERSION: %s", item.info.version.c_str());
        }
        else
        {

            std::vector<uint8_t> sfo_data = readFile(item.info.sfopath);
            if (sfo_data.empty())
                continue;

            SfoReader sfo(sfo_data);

            std::string path = fmt::format("/user/app/ITEM00001/custom_app_names/{}.ini", item.info.id);

            if (if_exists(path.c_str()) && ini_parse(path.c_str(), &app_info, NULL) >= 0)
            {
                log_info("[XMB OP] Custom Name Found");
                item.info.name= app_name_callback;
                log_info("[XMB OP] Name Set to: %s", item.info.name.c_str());
            }
            else
            {
                item.info.name= sfo.GetValueFor<std::string>("TITLE");
                if (get->lang != 1)
                {
                    item.info.name= sfo.GetValueFor<std::string>(fmt::format("TITLE_{0:}{1:d}", (get->lang < 10) ? "0" : "", get->lang));
                    if (item.info.name.empty())
                        item.info.name= sfo.GetValueFor<std::string>("TITLE");
                }
            }

            //    log_info("[XMB OP] %s is a favorite", item.info.id.c_str());
            item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("CONTENT_ID", sfo.GetValueFor<std::string>("CONTENT_ID")));
            item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("VERSION", sfo.GetValueFor<std::string>("VERSION")));
            item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("ATTRIBUTE_INTERNAL", std::to_string(sfo.GetValueFor<int>("ATTRIBUTE_INTERNAL"))));
            for (int z = 1; z <= 7; z++){ // SERVICE_ID_ADDCONT_ADD_1
                std::string s = sfo.GetValueFor<std::string>(fmt::format("SERVICE_ID_ADDCONT_ADD_{0:d}", z)); 
                if (!s.empty()){
                    item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>(fmt::format("SERVICE_ID_ADDCONT_APPINFO_ADD_{0:d}", z), s));
                    std::string sub = s.substr(s.find("-")+1); sub.pop_back(), sub.pop_back(), sub.pop_back();
                    item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>(fmt::format("SERVICE_ID_ADDCONT_ADD_{0:d}", z), sub));
                }
            }
            for (int z = 1; z <= 4; z++) // USER_DEFINED_PARAM_
                item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>(fmt::format("USER_DEFINED_PARAM_{0:d}", z), std::to_string(sfo.GetValueFor<int>(fmt::format("USER_DEFINED_PARAM_{0:d}", z)))));

            for (int z = 0; z <= 30; z++) { // TITLE_
                std::string key = fmt::format("TITLE_{0:}{1:d}", (z < 10) ? "0" : "", z);
                item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>(key, sfo.GetValueFor<std::string>(key)));
            }
           // log_info("tid: %s, cat: %s", fname.c_str(), sfo.GetValueFor<std::string>("CATEGORY").c_str());
            // if its a patch just set it to GD
            if (sfo.GetValueFor<std::string>("CATEGORY") == "gp")
               item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("CATEGORY", "gd"));
            else
                item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("CATEGORY", sfo.GetValueFor<std::string>("CATEGORY")));

            if (item.extra_data.extra_sfo_data["CATEGORY"] == "gd")
                item.flags.is_dumpable = true;

            item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("APP_TYPE", std::to_string(sfo.GetValueFor<int>("APP_TYPE"))));
            item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("APP_VER", sfo.GetValueFor<std::string>("APP_VER")));
            item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("SYSTEM_VER",  std::to_string(sfo.GetValueFor<int>("SYSTEM_VER"))));
            item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("PARENTAL_LEVEL", std::to_string(sfo.GetValueFor<int>("PARENTAL_LEVEL"))));
            item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("DOWNLOAD_DATA_SIZE", std::to_string(sfo.GetValueFor<int>("DOWNLOAD_DATA_SIZE"))));
            item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("ATTRIBUTE", std::to_string(sfo.GetValueFor<int>("ATTRIBUTE"))));
            item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("ATTRIBUTE2", std::to_string(sfo.GetValueFor<int>("ATTRIBUTE2"))));
            item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("REMOTE_PLAY_KEY_ASSIGN", std::to_string(sfo.GetValueFor<int>("REMOTE_PLAY_KEY_ASSIGN"))));
            item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("PT_PARAM", std::to_string(sfo.GetValueFor<int>("PT_PARAM"))));
            item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("INSTALL_DIR_SAVEDATA", sfo.GetValueFor<std::string>("INSTALL_DIR_SAVEDATA")));
            item.extra_data.extra_sfo_data.insert(std::pair<std::string, std::string>("IRO_TAG", std::to_string(sfo.GetValueFor<int>("IRO_TAG"))));
           
            item.info.version = item.extra_data.extra_sfo_data["APP_VER"];

///IRO_TAG

            //log_info("PARENTAL_LEVEL: %i", sfo.GetValueFor<int>("PARENTAL_LEVEL")); 
        }
        if( (category == FILTER_RETAIL_GAMES || category == FILTER_GAMES) &&  !item.flags.is_dumpable){
            log_debug("[F3] Filtered out %s, not a dumpable game, LIKELY MEDIA APP", fname.c_str());
            continue;
        }
        //log_info("added %s, cat: %s", fname.c_str(), item.extra_data.extra_sfo_data["CATEGORY"].c_str());
        out_vec.push_back(item);
    }

    // Skip one cover to only show it
    // place holder games
    int out_size = out_vec.size() - 1;
    if (out_size < 2 || out_size < MIN_NUMBER_OF_IF_APPS)
    {
        for (int j = out_size; j < MIN_NUMBER_OF_IF_APPS; j++)
        {
            item_t pc_app;
            log_info("[XMB OP] Adding Placeholder Game %d", j);
            pc_app.info.name= getLangSTR(LANG_STR::PLACEHOLDER);
            pc_app.info.id = "ITEM0000" + std::to_string(j);
            pc_app.info.picpath = no_icon_path;
            pc_app.info.version = "9.99";
            pc_app.extra_data.extra_sfo_data.clear();
            pc_app.flags.is_ph = true;
            out_vec.push_back(pc_app);
        }
    }

    out_vec[0].count.token_c = out_size;
    log_info("valid count: %d", out_size);
    log_info("token_c: %i : %i", out_vec[0].count.token_c, out_size);
#if 1
    int HDD_Count = 0;
    for (auto item : out_vec) {
       if(!item.flags.is_ext_hdd && !item.flags.is_vapp)
          HDD_Count++;
    }
#endif
    out_vec[0].count.HDD_count = HDD_Count;
    cvEntries.clear();


    if (out_vec.size() - 1 != out_vec[0].count.token_c &&
        out_vec.size() - 1 != out_vec[0].count.token_c + 1)
    {
        log_error("[LS_ERR] out_vec.size()|%i| != out_vec[0].count.token_c|%i| ", out_vec.size(), out_vec[0].count.token_c);
        // ani_notify(NOTIFI::WARNING, "Should NOT see this", fmt::format( "|{0:d}| != |{1:d}| ", out_vec.size(), out_vec[0].count.token_c));
    }
}


// from https://elixir.bootlin.com/busybox/0.39/source/df.c
#include <stdio.h>
#include <sys/mount.h>

int df(std::string mountPoint, std::string &out)
{
    struct statfs s;
    long blocks_used = 0;
    long blocks_percent_used = 0;
    // struct fstab* fstabItem;

    if (statfs(mountPoint.c_str(), &s) != 0)
    {
        log_error("df cannot open %s", mountPoint.c_str());
        return 0;
    }

    if (s.f_blocks > 0)
    {
        blocks_used = s.f_blocks - s.f_bfree;
        blocks_percent_used = (long)(blocks_used * 100.0 / (blocks_used + s.f_bavail) + 0.5);
#if 0
        log_info("%-20s %9ld %9ld %9ld %3ld%% %s",
            out,
            (long)(s.f_blocks * (s.f_bsize / 1024.0)),
            (long)((s.f_blocks - s.f_bfree) * (s.f_bsize / 1024.0)),
            (long)(s.f_bavail * (s.f_bsize / 1024.0)),
            blocks_percent_used, mountPoint);
#endif
    }
    double gb_free = ((long)(s.f_bavail * (s.f_bsize / 1024.0)) / 1024);
    gb_free /= 1024.;

    double disk_space = (long)(s.f_blocks * (s.f_bsize / 1024.0) / 1024);
    disk_space /= 1024.;

    out = fmt::format("{0:}, {1:d}%, {2:.1f}GBs / {3:.1f}GBs", mountPoint, blocks_percent_used, disk_space - gb_free, disk_space);
    return blocks_percent_used;
}

int check_free_space(const char *mountPoint)
{
    struct statfs s;

    if (statfs(mountPoint, &s) != 0)
    {
        log_error("error %s", mountPoint);
        return 0;
    }
    double gb_free = ((long)(s.f_bavail * (s.f_bsize / 1024.0)) / 1024);
    gb_free /= 1024.;
    log_info("%s has %.1f GB free", mountPoint, gb_free);

    return (int)floor(gb_free);
}
#include <time.h> // ctime

void get_stat_from_file(std::string &out_str, std::string &filepath)
{
    struct stat fileinfo;
    //log_info("get_stat_from_file: %s %s", filepath.c_str(), out_str.c_str());

    if (stat(filepath.c_str(), &fileinfo) != -1)
    {
        // https://man7.org/linux/man-pages/man2/stat.2.html#EXAMPLES
        switch (fileinfo.st_mode & S_IFMT)
        {
        case S_IFBLK:
            out_str = "Block device";
            break;
        case S_IFCHR:
            out_str = "Character device";
            break;
        case S_IFDIR:
            out_str = "Directory";
            out_str += ", ";
            out_str += ctime(&fileinfo.st_mtime);
            out_str.pop_back();
            break;
        case S_IFIFO:
            out_str = "FIFO/Pipe";
            break;
        case S_IFLNK:
            out_str = "Symlink";
            break;
        case S_IFREG:
            out_str = "Size: ";
            fileinfo.st_size > 0 ?  out_str += calculateSize(fileinfo.st_size) : out_str += "0 KBs";
            out_str += ", ";
            out_str += ctime(&fileinfo.st_mtime);
            out_str.pop_back();
            break;
        case S_IFSOCK:
            out_str = "Size: ";
            fileinfo.st_size > 0 ?  out_str += calculateSize(fileinfo.st_size) : out_str += "0 KBs";
            out_str = "Socket";
            break;
        default:
            out_str = "Unknown, Mode ";
            out_str += std::to_string(fileinfo.st_mode);
            break;
        }
    }
    else // can't be reached by logic, but in case...
    {
        out_str = "Error stat";
        out_str += "(" + std::string(strerror(errno)) + ")";
    }
}

int check_stat(const char *filepath)
{
    struct stat fileinfo;
    if (!stat(filepath, &fileinfo))
    {
        switch (fileinfo.st_mode & S_IFMT)
        {
        case S_IFDIR:
            return S_IFDIR;
        case S_IFREG:
            return S_IFREG;
        }
    }
    return 0;
}

int get_folder_size(const char *file_path, u64 *size) {
	struct stat stat_buf;

	if (!file_path || !size)
		return -1;

	if (stat(file_path, &stat_buf) < 0)
		return -1;

	*size = stat_buf.st_size;

	return 0;
}
