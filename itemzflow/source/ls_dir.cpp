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
const char *SPECIAL_XMB_ICON_FOLDERS[] = {"/user/app/" APP_HOME_DATA_TID, "/user/app/" DEBUG_SETTINGS_TID};
const char *SPECIAL_XMB_ICON_TID[] = {APP_HOME_DATA_TID, DEBUG_SETTINGS_TID};

extern "C" bool if_exists(const char *path)
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

bool getEntries(std::string path, std::vector<std::string> &cvEntries, FS_FILTER filter)
{
    struct dirent *pDirent;
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

        if (strcmp(pDirent->d_name, ".") == 0 || strcmp(pDirent->d_name, "..") == 0)
            continue;

        // Dir is title_id, it WILL go through 3 filters
        // so lets assume they are vaild until the filters say otherwise
        if (pDirent->d_type == DT_DIR && filter != FS_MP3_ONLY) // MP3S ONLY NO DIR NAMES
            cvEntries.push_back(pDirent->d_name);
        else if (filter != FS_FOLDER) // if folders only do ntohing
        {
            const char *ext = NULL;

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
                cvEntries.push_back(pDirent->d_name);
            default:
                ext = ".lm_is_da_best"; // an ext no one will ever have
                break;
            }
            if (strstr(pDirent->d_name, ext))
                cvEntries.push_back(pDirent->d_name);
            else
            { // try with all caps
                char upper_case_ext[strlen(ext) + 1];
                for (int i = 0; i < strlen(ext); i++)
                    upper_case_ext[i] = toupper(ext[i]);

                if (strstr(pDirent->d_name, &upper_case_ext[0]))
                    cvEntries.push_back(pDirent->d_name);
            }
        }
    }
    if (closedir(pDir) != 0)
    {
        log_info("closedir failed %s", strerror(errno));
        return false;
    }
    return true;
}

void init_xmb_special_icons(const char *XMB_ICON_PATH)
{
    if (!if_exists(XMB_ICON_PATH))
    {
        log_info("[XMB OP] Creating %s for XMB", XMB_ICON_PATH);
        mkdir(XMB_ICON_PATH, 0777);
    }
    else
        log_info("[XMB OP] %s already exists", XMB_ICON_PATH);
}

#define MEM_DEBUG 0

bool is_XMB_spec_icon(const char *tid)
{

    // Only fake *kits and real *kits have this file, so we need to check
    // if they have APP_HOME etc...
    // if (strstr(DEBUG_SETTINGS_TID, tid) != NULL) return true;
    if (if_exists("/system/sys/set_upper.self"))
    {
        for (int i = 0; i < sizeof SPECIAL_XMB_ICON_TID / sizeof SPECIAL_XMB_ICON_TID[0]; i++)
        {
            if (strstr(SPECIAL_XMB_ICON_TID[i], tid) != NULL)
                return true;
        }
    }

    return false;
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

bool AppExists(std::string tid)
{
    std::string path = fmt::format("/user/app/{}/app.pkg", tid);
    if (!if_exists(path.c_str()))
    {
        path = fmt::format("/mnt/ext0/user/app/{}/app.pkg", tid);
        if (!if_exists(path.c_str()))
            return false;
    }
    return true;
}
// the Settings
extern ItemzSettings set,
    *get;

// each token_data is filled by dynalloc'd mem by strdup!
void index_items_from_dir(std::vector<item_t> &out_vec, std::string dirpath, std::string dirpath2, Sort_Category category)
{
    int ext_count = -1;
    bool is_spec_icon = false;
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

    for (int i = 0; i < sizeof SPECIAL_XMB_ICON_FOLDERS / sizeof SPECIAL_XMB_ICON_FOLDERS[0]; i++)
        init_xmb_special_icons(SPECIAL_XMB_ICON_FOLDERS[i]);

    std::vector<std::string> cvEntries;

    if (!getEntries(dirpath, cvEntries, FS_FOLDER))
        log_info("failed to get dir");
    else
        log_debug("[Dir 1] Found %i Apps in: %s", (HDD_count = cvEntries.size()), dirpath.c_str());

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
    out_vec.resize(entCt);

    int i = 1; // vaild app count
    for (int not_used = 1; not_used < entCt; not_used++)
    {
        // we use just those token_data
        std::string fname = cvEntries[not_used - 1];
        if (!is_XMB_spec_icon(fname.c_str()))
        {
            if (!filter_entry_on_IDs(fname.c_str()))
            {
                log_debug("[F1] Filtered out %s", fname.c_str());
                continue;
            }
#if defined(__ORBIS__)
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
            if (AppExists(fname))
                log_debug("[F2] App exists %s", fname.c_str());
            else
            {
                log_debug("[F2] Filtered out %s, the App doesnt exist", fname.c_str());
                continue;
            }

#endif
#endif
        }
        else
            is_spec_icon = true;

        if (i >= HDD_count - 1)
            out_vec[i].is_ext_hdd = true;
        else
            out_vec[i].is_ext_hdd = false;

        if (category == FILTER_HOMEBREW)
        {
            if (fname.find("CUSA") != std::string::npos)
                continue;
        }
        else if (category == FILTER_GAMES)
        {
            if (fname.find("CUSA") == std::string::npos)
                continue;
        }

        // dynalloc for user tokens
        out_vec[i].token_c = TOTAL_NUM_OF_TOKENS;
        out_vec[i].name = out_vec[i].id = fname;
        out_vec[i].extra_sfo_data.clear();
        out_vec[i].is_ph = false;
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

        out_vec[i].picpath = tmp;
        // don't try lo load
        out_vec[i].texture = GL_NULL; // XXX load_png_asset_into_texture(tmp);
#if defined(__ORBIS__)
        tmp = fmt::format("/user/app/{}/app.pkg", fname);
        log_info("[XMB OP] TItLE ID: %s", fname.c_str());
        if (!if_exists(tmp.c_str()))
            tmp = fmt::format("/mnt/ext0/user/app/{}/app.pkg", fname);
#else // on pc
        tmp = fmt::format("{}/{}/app.pkg", std::string(APP_PATH("apps")), fname);
#endif
        out_vec[i].package = tmp;

#if defined(__ORBIS__)
        out_vec[i].is_fpkg = is_fpkg(out_vec[i].package);
        out_vec[i].app_vis = AppDBVisiable(out_vec[i].id, VIS_READ, 0);

        tmp = fmt::format("/system_data/priv/appmeta/{}/param.sfo", fname);
        if (!if_exists(tmp.c_str()))
            tmp = fmt::format("/system_data/priv/appmeta/external/{}/param.sfo", fname);

#else // on pc
        tmp = fmt::format("{}/{}/param.sfo", APP_PATH("apps"), fname);

#endif
        out_vec[i].sfopath = tmp;

        if ((category == FILTER_FPKG && !out_vec[i].is_fpkg) || (category == FILTER_RETAIL_GAMES && out_vec[i].is_fpkg))
            continue;

        // read sfo
        if (is_spec_icon)
        {
            if (strstr(fname.c_str(), APP_HOME_DATA_TID) != NULL)
            {
                out_vec[i].picpath = "/data/APP_HOME.png";
                out_vec[i].name = "APP_HOME(Data)";
                out_vec[i].id = APP_HOME_DATA_TID;
            }
            if (strstr(fname.c_str(), DEBUG_SETTINGS_TID) != NULL)
            {
                out_vec[i].picpath = "/data/debug.png";
                out_vec[i].name = "PS4 Debug Settings";
                out_vec[i].id = DEBUG_SETTINGS_TID;
            }

            out_vec[i].version = "0.00";
            out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("CATEGORY", "gde"));

            log_info("[XMB OP] SFO Name: %s", out_vec[i].name.c_str());
            log_info("[XMB OP] SFO ID: %s", out_vec[i].id.c_str());
            log_info("[XMB OP] SFO VERSION: %s", out_vec[i].version.c_str());
        }
        else
        {
            std::vector<uint8_t> sfo_data = readFile(out_vec[i].sfopath);
            if (sfo_data.empty())
                continue;

            SfoReader sfo(sfo_data);

            std::string path = fmt::format("/user/app/ITEM00001/custom_app_names/{}.ini", out_vec[i].id);

            if (if_exists(path.c_str()) && ini_parse(path.c_str(), &app_info, NULL) >= 0)
            {
                log_info("[XMB OP] Custom Name Found");
                out_vec[i].name = app_name_callback;
                log_info("[XMB OP] Name Set to: %s", out_vec[i].name.c_str());
            }
            else
            {
                out_vec[i].name = sfo.GetValueFor<std::string>("TITLE");
                if (get->lang != 1)
                {
                    out_vec[i].name = sfo.GetValueFor<std::string>(fmt::format("TITLE_{0:}{1:d}", (get->lang < 10) ? "0" : "", get->lang));
                    if (out_vec[i].name.empty())
                        out_vec[i].name = sfo.GetValueFor<std::string>("TITLE");
                }
            }

            out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("CONTENT_ID", sfo.GetValueFor<std::string>("CONTENT_ID")));
            out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("VERSION", sfo.GetValueFor<std::string>("VERSION")));
            out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("ATTRIBUTE_INTERNAL", std::to_string(sfo.GetValueFor<int>("ATTRIBUTE_INTERNAL"))));
            for (int z = 1; z <= 7; z++){ // SERVICE_ID_ADDCONT_ADD_1
                std::string s = sfo.GetValueFor<std::string>(fmt::format("SERVICE_ID_ADDCONT_ADD_{0:d}", z)); 
                if (!s.empty()){
                    out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>(fmt::format("SERVICE_ID_ADDCONT_APPINFO_ADD_{0:d}", z), s));
                    std::string sub = s.substr(s.find("-")+1); sub.pop_back(), sub.pop_back(), sub.pop_back();
                    out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>(fmt::format("SERVICE_ID_ADDCONT_ADD_{0:d}", z), sub));
                }
            }
            for (int z = 1; z <= 4; z++) // USER_DEFINED_PARAM_
                out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>(fmt::format("USER_DEFINED_PARAM_{0:d}", z), std::to_string(sfo.GetValueFor<int>(fmt::format("USER_DEFINED_PARAM_{0:d}", z)))));

            for (int z = 0; z <= 30; z++) { // TITLE_
                std::string key = fmt::format("TITLE_{0:}{1:d}", (z < 10) ? "0" : "", z);
                out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>(key, sfo.GetValueFor<std::string>(key)));
            }
            // if its a patch just set it to GD
            if (sfo.GetValueFor<std::string>("CATEGORY") == "gp")
               out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("CATEGORY", "gd"));
            else
                out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("CATEGORY", sfo.GetValueFor<std::string>("CATEGORY")));

            if (out_vec[i].extra_sfo_data["CATEGORY"] == "gd")
                out_vec[i].is_dumpable = true;

            out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("APP_TYPE", std::to_string(sfo.GetValueFor<int>("APP_TYPE"))));
            out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("APP_VER", sfo.GetValueFor<std::string>("APP_VER")));
            out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("CATEGORY", "gd"));
            out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("SYSTEM_VER",  std::to_string(sfo.GetValueFor<int>("SYSTEM_VER"))));
            out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("PARENTAL_LEVEL", std::to_string(sfo.GetValueFor<int>("PARENTAL_LEVEL"))));
            out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("DOWNLOAD_DATA_SIZE", std::to_string(sfo.GetValueFor<int>("DOWNLOAD_DATA_SIZE"))));
            out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("ATTRIBUTE", std::to_string(sfo.GetValueFor<int>("ATTRIBUTE"))));
            out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("ATTRIBUTE2", std::to_string(sfo.GetValueFor<int>("ATTRIBUTE2"))));
            out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("REMOTE_PLAY_KEY_ASSIGN", std::to_string(sfo.GetValueFor<int>("REMOTE_PLAY_KEY_ASSIGN"))));
            out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("PT_PARAM", std::to_string(sfo.GetValueFor<int>("PT_PARAM"))));
            out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("INSTALL_DIR_SAVEDATA", sfo.GetValueFor<std::string>("INSTALL_DIR_SAVEDATA")));
            out_vec[i].extra_sfo_data.insert(std::pair<std::string, std::string>("IRO_TAG", std::to_string(sfo.GetValueFor<int>("IRO_TAG"))));
           
            out_vec[i].version = out_vec[i].extra_sfo_data["APP_VER"];
///IRO_TAG

            //log_info("PARENTAL_LEVEL: %i", sfo.GetValueFor<int>("PARENTAL_LEVEL")); 
        }
        i++;
        is_spec_icon = false;
    }

    // Skip one cover to only show it
    // place holder games
    if (i > 2 && i < MIN_NUMBER_OF_IF_APPS)
    {
        out_vec.resize(i + MIN_NUMBER_OF_IF_APPS - i);
        for (int j = i; j < MIN_NUMBER_OF_IF_APPS; j++)
        {
            log_info("[XMB OP] Adding Placeholder Game %d", i);
            out_vec[j].name = getLangSTR(PLACEHOLDER);
            out_vec[j].id = "ITEM0000" + std::to_string(j);
            out_vec[j].picpath = no_icon_path;
            out_vec[j].version = "9.99";
            out_vec[j].extra_sfo_data.clear();
            out_vec[j].is_ph = true;
        }
        i += MIN_NUMBER_OF_IF_APPS - i;
    }

    out_vec[0].token_c = i - 1;
    log_info("valid count: %d", i);
    HDD_count = i;
    HDD_count -= ext_count;
    log_info("HDD_count: %i, arr sz: %i", HDD_count, out_vec.size());
    if (out_vec[0].token_c >= 1)
        out_vec.resize(i);
    // bounds check
    if (out_vec[0].token_c < 0)
        out_vec[0].token_c = 0;

    log_info("token_c: %i : %i", out_vec[0].token_c, i);
    // report back
    if (0)
    {
        for (int i = 1; i < out_vec[0].token_c; i++)
            log_info("%3d: %s %p", i, out_vec[i].name.c_str(), out_vec[i].name.c_str());
    }

    cvEntries.clear();

    out_vec[0].picpath = no_icon_path;
    out_vec[0].name = "ItemzCore_ls_dir";
    out_vec[0].id = "ITEM99999";
    out_vec[0].HDD_count = HDD_count-1;

    if (out_vec.size() - 1 != out_vec[0].token_c &&
        out_vec.size() - 1 != out_vec[0].token_c + 1)
    {
        log_error("[LS_ERR] out_vec.size()|%i| != out_vec[0].token_c|%i| ", out_vec.size(), out_vec[0].token_c);
        // ani_notify(NOTIFI_WARNING, "Should NOT see this", fmt::format( "|{0:d}| != |{1:d}| ", out_vec.size(), out_vec[0].token_c));
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
            out_str += calculateSize(fileinfo.st_size);
            out_str += ", ";
            out_str += ctime(&fileinfo.st_mtime);
            out_str.pop_back();
            break;
        case S_IFSOCK:
            out_str = "Size: ";
            out_str += calculateSize(fileinfo.st_size);
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
