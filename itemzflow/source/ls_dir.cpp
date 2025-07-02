/*
    listing local folder using getdents()
*/

#include <cstddef>
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
const char *SPECIAL_XMB_ICON_TID[] = {APP_HOME_DATA_TID, APP_HOME_HOST_TID, DEBUG_SETTINGS_TID, WORKSPACE0_TID};
std::vector<std::string> Host_app_mounted_dirs;
#if defined(__ORBIS__)
#include <sys/uio.h>
#include <sys/mount.h>
struct NonStupidIovec {
  const void *iov_base;
  size_t iov_length;

  constexpr NonStupidIovec(const char *str)
      : iov_base(str), iov_length(__builtin_strlen(str) + 1) {}
  constexpr NonStupidIovec(const char *str, size_t length)
      : iov_base(str), iov_length(length) {}
};

constexpr NonStupidIovec operator"" _iov(const char *str, unsigned long len) {
  return {str, len + 1};
}
/// extern "C" int nmount(struct iovec *, unsigned long, int);
bool remount(const char *dev, const char *path) {
  NonStupidIovec iov[]{
      "fstype"_iov, "nullfs"_iov, "fspath"_iov, {path},
      "target"_iov, {dev},        "rw"_iov,     {nullptr, 0},
  };
  constexpr size_t iovlen = sizeof(iov) / sizeof(iov[0]);
  bool success = nmount(reinterpret_cast<struct iovec *>(iov), iovlen, 0x80000LL) == 0;
  log_info("Remount status: %s", success ? "Success" : strerror(errno));
  log_info("From : %s, To: %s", dev, path);
  if(errno == ENOTCONN){
    msgok(MSG_DIALOG::WARNING, "Restart your PS4 and make sure next time when you close itemzflow you do it via pressing circle twice");
    return success;
  }
  if(errno == EBADF || errno == EPERM || errno == EIO){
    log_info("trying to unmount");
    sys_unmount(path, 0x80000LL);
    success = nmount(reinterpret_cast<struct iovec *>(iov), iovlen, 0x80000LL) == 0;
    log_info("Remount status: %s", success ? "Success" : strerror(errno));
  }
  if(success && strstr(dev, "/hostapp") != NULL){
    Host_app_mounted_dirs.push_back(path);
  }
  return success;
}

void unmount_atexit(){
    for(auto dir : Host_app_mounted_dirs){
        log_info("Unmounting %s", dir.c_str());
        ForceUnmountVapp(dir);
    }
}
#endif

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

int getDirectoryDepth(const std::string& path) {
    int depth = 0;

    for (char character : path) {
        if (character == '/' || character == '\\') {
            ++depth;
        }
    }

    return depth;
}
std::string getFolderName(const std::string &fullPath) {
  size_t lastSlash = fullPath.find_last_of('/');
  if (lastSlash != std::string::npos) {
    return fullPath.substr(lastSlash + 1);
  } else {
    // No folder separator found, return the full path
    return fullPath;
  }
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
        if (tid == WORKSPACE0_TID)
            return true;

        for (auto item : all_apps) {
          if (item.info.id == tid) {
            return item.flags.is_vapp;
          }
        }
    }
    

    return false;
}

int countTotalDirectories(const std::string &dir) {
  int count = 0;
  DIR *dp = opendir(dir.c_str());
  if (dp != nullptr) {
    struct dirent *entry;
    while ((entry = readdir(dp)) != nullptr) {
      if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 &&
          strcmp(entry->d_name, "..") != 0) {
        std::string path = dir + "/" + entry->d_name;
        if (getDirectoryDepth(path) > 5) {
          log_debug("Skipping %s for depth", path.c_str());
          continue;
        }
        count +=
            1 + countTotalDirectories(path); // Count this directory and recurse
      }
    }
    closedir(dp);
  }
  return count;
}

bool checkSELFMagic(const std::string &ebootPath) {
  std::ifstream SelfFile(ebootPath, std::ios::binary);
  if (!SelfFile.good())
    return false;

  char magic[4];
  SelfFile.read(magic, 4);
  SelfFile.close();

  const char expectedMagic[4] = {0x4F, 0x15, 0x3D, 0x1D};
  return std::memcmp(magic, expectedMagic, 4) == 0;
}

bool ChechSelfsinDir(const std::string& directory) {
    DIR* dir = NULL;
    struct dirent* entry = NULL;

    // Open the directory
    dir = opendir(directory.c_str());
    if (!dir) {
        perror("Unable to open directory");
        return false;
    }

    // Read the directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Skip the current and parent directories
        if (std::string(entry->d_name) == "." || std::string(entry->d_name) == "..") {
            continue;
        }

        // Construct the full path
        std::string fullPath = directory + "/" + entry->d_name;

        // Check file extensions
        if (fullPath.size() >= 4 && (fullPath.compare(fullPath.size() - 4, 4, ".prx") == 0 || 
                                     fullPath.compare(fullPath.size() - 5, 5, ".sprx") == 0)) {
            // Check the magic number
            if (checkSELFMagic(fullPath)) {
                log_info("File %s has the correct magic number.", fullPath.c_str());
            } else {
                log_error("File %s does not have the correct magic number.", fullPath.c_str());
                return false;
            }
        }
    }

    // Close the directory
    closedir(dir);
    return true;
}

void findGamesRecursive(const std::string &dir, int depth,
                        std::vector<item_t> &games, int &directoriesScanned,
                        int totalDirectories) {
  const char *directory = dir.c_str();

  if (depth == 0) {
    log_debug("Reached max depth @ %s", directory);
    return;
  }

  if (dir.rfind("/user/data") != std::string::npos) {
    log_debug("%s is not allowed", directory);
    return;
  }

  DIR *dp = opendir(directory);
  if (dp == nullptr) {
    log_error("Error opening directory: %s", directory);
    return;
  }
#if defined(__ORBIS__)
  sceMsgDialogProgressBarSetMsg(
      0,
      fmt::format("Scanning for FG Apps\n\nCurrent dir: {}", dir).c_str());
#endif

  struct dirent *entry;
  while ((entry = readdir(dp)) != nullptr) {
    if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 &&
        strcmp(entry->d_name, "..") != 0) {
      // Check for game files in the current directory
      std::string filePath = dir + "/" + entry->d_name;
      item_t gameInfo;
      gameInfo.info.sfopath = filePath + "/sce_sys/param.sfo";
      gameInfo.info.picpath = filePath + "/sce_sys/icon0.png";
      gameInfo.info.vapp_path = filePath;

      if (if_exists(gameInfo.info.sfopath.c_str())) {
#if defined(__ORBIS__)
          if (!GetVappDetails(gameInfo)) {
            log_error("Failed to get json values for %s", gameInfo.info.sfopath.c_str());
            continue;
          }
#endif          
          log_info("Found game: %s", gameInfo.info.name.c_str());
          games.push_back(gameInfo);
      }
      // Recurse into subdirectory
      findGamesRecursive(dir + "/" + entry->d_name, depth - 1, games,
                         directoriesScanned, totalDirectories);
    }
  }
  closedir(dp);

  directoriesScanned++;
#if defined(__ORBIS__)
  float progress =
      (static_cast<float>(directoriesScanned) / totalDirectories) * 100;
  sceMsgDialogProgressBarSetValue(0, progress);
#endif
}

// Modified findGames function
bool ScanForVapps() {
  Timer timer;
  std::vector<item_t> games;
  int maxDepth = 5; // Set the recursion depth
  std::vector<std::string> directories = {"/data",     "/mnt/usb0", "/mnt/usb1",
                                          "/mnt/usb2", "/mnt/usb3", "/mnt/ext1",
                                          "/mnt/ext0", "/user", "/hostapp"};
#if defined(__ORBIS__)
  progstart("Scanning for FG apps...\n\nCounting the directories");
#endif
  int totalDirectories = 0;
  for (const auto &dir : directories) {
    totalDirectories += countTotalDirectories(dir);
  }

  int directoriesScanned = 0;
  for (const auto &dir : directories) {
    findGamesRecursive(dir, maxDepth, games, directoriesScanned,
                       totalDirectories);
  }
  // display all the apps in the vector
  std::stringstream ss;
  ss << "Found " << games.size()
     << " FG Apps, Would you like to Add them? (existing Games will be "
        "replaced, only 1 app per title id)\n";
  ss << "----------------------------------------\n";
  for (auto game : games) {
    log_debug("Found game: %s", game.info.name.c_str());
    ss << "Game Name: " << game.info.name << " (" << game.info.vapp_path
       << ")\n";
  }
  if (games.empty()) {
    msgok(MSG_DIALOG::WARNING, "No FG Apps found");
    return false;
  }
#if defined(__ORBIS__)
  if (Confirmation_Msg(ss.str().c_str()) == NO) {
    log_debug("User cancelled");
    return false;
  }
#endif

  loadmsg("Attempting to add FG Apps...");

  int app_added = 0;
  std::stringstream add_games_msg;
  add_games_msg << "---------------------- FG Apps Added --------------------\n";
  for (auto &game : games) {
    std::string sys_path = "/system_ex/app/" + game.info.id;
    std::string metapath = "/user/appmeta/" + game.info.id + "/";
    std::string sce_sys = sys_path + "/sce_sys";
    std::string trophy_path = sce_sys + "/trophy/trophy00.trp"; ///mnt/sandbox/CUSA03980_000/download0
    std::string dl0_info = "/user/download/" + game.info.id + "/download0_info.dat";
    mkdir(sys_path.c_str(), 0777);

    mkdir("/user/download/", 0777);

    log_info("fullpath: %s", game.info.vapp_path.c_str());
#if defined(__ORBIS__)
    if (EditDataIFPS5DB(game.info.id, game.info.name, game.info.vapp_path) &&
        Fix_Game_In_DB(game, false)) {

       if(!IsVappmounted(game.info.vapp_path)){
           if (!remount(game.info.vapp_path.c_str(), sys_path.c_str())) {
               log_info("Remount failed");
               continue;
            }
        }

        if(checkTrophyMagic(trophy_path.c_str())){
           log_error("Trophy file is signed, removing...");
           unlink(trophy_path.c_str());
        }
        else{
           log_error("Trophy file is not signed, skipping...");
        }


        mkdir(std::string("/user/download/" + game.info.id).c_str(), 0777);
        touch_file(dl0_info.c_str());

       // if (!if_exists(metapath.c_str())){
       rmtree(metapath.c_str());
       unlink(metapath.c_str());
       mkdir(metapath.c_str(), 0777);
       copy_dir(game.info.vapp_path + "/sce_sys", metapath);
       // }

      add_games_msg << "Game Name: " << game.info.name << " ("
                    << game.info.vapp_path << ")\n";
      app_added++;
    }
#endif
  }

  msgok(MSG_DIALOG::NORMAL,
        fmt::format("Added {}/{} FG Apps Successfully\n{}", app_added, games.size(), add_games_msg.str()));

  return true;
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

    file.close();

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
    if_app.flags.is_ph = true;
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
            else if(fname == WORKSPACE0_TID){
                item.info.picpath = asset_path("workspace0.png");
                item.info.name = "(Experimental) FG Games";
                item.info.id = WORKSPACE0_TID;
            }
            else
            {
                item.info.picpath = no_icon_path;
                item.info.name = "ItemzCore";
                item.info.id = "ITEM00001";
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
#if defined(__ORBIS__)
    log_info("Vapps available: %d", retrieveFVapps(out_vec, category, favorites));
#endif

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
#if defined(__ORBIS__)

bool overwrite_all = false;
bool asked_to_ow = false;
bool action_called = false;

bool file_action(std::string src, std::string dst, bool silent) {
    std::string src_p = src;
loop:
    if (custom_Confirmation_Msg(fmt::format("{} has failed to copy", src), "Other Options", "Retry Copy") == 2) {
        if (!copyFile(src, dst, silent ? false : true)) {
            goto loop;
        }
        else {
            return true;
        }
    }
    else {
        if (custom_Confirmation_Msg(fmt::format("{} has failed to copy", src), "Ignore", "Cancel") == 2) {
            action_called = true;
            return false;
        }
        else {
            return true;
        }
    }
    action_called = true;
    return false;
}
bool copy_dir(const char* sourcedir, const char* destdir, bool silent) {

    if (action_called) {
        log_error("user asked to cancel");
        return false;
    }

    log_info("s: %s d: %s", sourcedir, destdir);
    DIR* dir = opendir(sourcedir);
    if(!dir) {
		log_info("cannot open sourcedir");
		return false;
	}

    struct dirent* dp;
    struct stat info;
    char src_path[1024];
    char dst_path[1024];

    if (!dir) {
        log_info("cannot open destdir");
        action_called = true;
        return false;
    }
    mkdir(destdir, 0777);
    while ((dp = readdir(dir)) != NULL) {
        if (strstr(dp->d_name, "save.ini") != NULL)
            log_info("Save.ini file skipped...");
        else if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")
            || strstr(dp->d_name, "nobackup") != NULL) {
            continue;
        }
        else {
            snprintf(src_path, sizeof(src_path)-1, "%s/%s", sourcedir, dp->d_name);
            snprintf(dst_path, sizeof(dst_path)-1, "%s/%s", destdir, dp->d_name);

            if (!stat(src_path, &info)) {
                if (S_ISDIR(info.st_mode)) {
                    copy_dir(src_path, dst_path, silent);
                }
                else if (S_ISREG(info.st_mode)) {

                    if (!asked_to_ow) {
                        if (!silent &&  Confirmation_Msg("Would you like to overwrite ALL existing files?") == YES)
                        {
                            overwrite_all = true;
                        }
                        asked_to_ow = true;
                    }

                    // Skip if file already exists in destination
                    struct stat dst_info;
                    if (!stat(dst_path, &dst_info)) {
                        if (!overwrite_all) {
                            continue;
                        }
                    }

                    // Skip if file size is less than 100 MB
                    if (info.st_size < 100 * 1024 * 1024) {
                        if (!copyFile(src_path, dst_path, false)) {
                            return file_action(src_path, dst_path, silent);
                        }
                        else {
                            continue;
                        }
                    }

                    if (!copyFile(src_path, dst_path, silent ? false : true)) {
                        return file_action(src_path, dst_path, silent);
                    }
                }

            }
        }
    }
    closedir(dir);

    return true;
}

void chmod_recursive(const char* path, mode_t mode) {
    struct stat st;

    if (stat(path, &st) != 0) {
        log_info("Error accessing file/directory: %s | error %s", path, strerror(errno));
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        // If it's a directory, apply chmod and recursively call on its contents
        if (chmod(path, mode) != 0) {
            log_info("Error changing permissions for directory: %s", path);
            return;
        }

        DIR* dir = opendir(path);
        if (dir == nullptr) {
            log_error("Error opening directory: %s", path);
            return;
        }

        dirent* entry = nullptr;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                // Ignore "." and ".." entries
                if (entry->d_type == DT_DIR) {
                    char childPath[PATH_MAX];
                    snprintf(childPath, PATH_MAX, "%s/%s", path, entry->d_name);
                    chmod_recursive(childPath, mode);
                }
            }
        }

        closedir(dir);
    }
    else if (S_ISREG(st.st_mode)) {
        // If it's a regular file, apply chmod
        if (chmod(path, mode) != 0) {
            log_info("Error changing permissions for file: %s", path);
            return;
        }
    }
}


bool copyFolder(const char* src, const char* dest, long size, long *bytes_done, bool move) {
    DIR* dir;
    struct dirent* entry;
    struct stat statBuf;

    std::string dir_size = src;
    if ((dir = opendir(src)) == NULL) {
        perror("Error opening source directory");
        return false;
    }

    if (stat(dest, &statBuf) == -1) {
        mkdir(dest, 0700); // Create destination directory if it doesn't exist
    }

    if (action_called) {
        log_error("user asked to cancel");
        return false;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; // Skip current and parent directories
        }

        char srcPath[PATH_MAX];
        char destPath[PATH_MAX];

        snprintf(srcPath, sizeof(srcPath), "%s/%s", src, entry->d_name);
        snprintf(destPath, sizeof(destPath), "%s/%s", dest, entry->d_name);

        if (stat(srcPath, &statBuf) == -1) {
            perror("Error getting file status");
            log_error("Error getting file status for %s", srcPath);
            closedir(dir);
            return file_action(srcPath, destPath, false);
        }

        if (S_ISDIR(statBuf.st_mode)) {
            copyFolder(srcPath, destPath, size, bytes_done, move); // Recursive call for subdirectories
        }
        else {
           
            std::string in = srcPath;
            std::vector<char> buffer(MB(10));
            size_t bytes = 0;

            int src = sceKernelOpen(srcPath, 0x0000, 0);
            if (src < 0) {
                return file_action(in, destPath, false);
            }

            int out_file = sceKernelOpen(destPath, 0x0001 | 0x0200 | 0x0400, 0777);
            if (out_file < 0) {
                return file_action(in, destPath, false);
            }

            if (buffer.size() > 0)
            {
                while (0 < (bytes = sceKernelRead(src, &buffer[0], MB(10)))) {

                    sceKernelWrite(out_file, &buffer[0], bytes);
                    *bytes_done += bytes;
                    int status = sceMsgDialogUpdateStatus();
                    if (ORBIS_COMMON_DIALOG_STATUS_RUNNING == status) {
                        std::string buf = fmt::format("{}...\nFolder Size: {}\nCurrent file: {}\nDestination Folder: {}", move ? "Moving Folder" : "Copying Folder", calculateSize(size), srcPath, dest);
                        sceMsgDialogProgressBarSetValue(0, (*bytes_done * 100.0) / size);
                        sceMsgDialogProgressBarSetMsg(0, buf.c_str());
                    }
                }
            }

            buffer.clear();

            close(src);
            close(out_file);
            chmod(destPath, 0777);

            if (move) {
                unlink(srcPath);
            }
        }

        // Calculate and display overall copy percentage
       // printf("Overall copy percentage: %.2f%%\n", ((float)ftell(stdout) / (float)statBuf.st_size) * 100);
        //sceMsgDialogProgressBarSetMsg(((float)ftell(stdout) / (float)statBuf.st_size) * 100, "Copying Folder...");
    }

    closedir(dir);

    return true;
}
#endif
bool StartAppIOOP(const char* src,
    const char* dest, bool move) {
    struct statfs s;
    std::string dir_size = src;
#if defined(__ORBIS__)
    progstart(move ? "Starting folder move ...\nCalculating Folder Size ...\n(This may take many mins and will remain at %0)" : "Starting folder copy ...\nCalculating Folder Size ...\n(This may take many mins and will remain at %0)");
    if (!if_exists(src) || statfs(src, &s) != 0) {
        msgok(MSG_DIALOG::WARNING, "Failed to get the folder size");
        return false;
    }
    uint64_t totalSize = s.f_bfree * s.f_bsize;
    if (totalSize <= 0) {
        log_error("Error calculating folder size, size reported %llx", totalSize);
        return false;
    }

    if (statfs(dest, &s) != 0)
    {
        //log_error("df cannot open %s", dest);
        msgok(MSG_DIALOG::WARNING, "Failed to get the folder size");
        return false;
    }

    // Calculate free space in bytes
    long long free_space = s.f_bsize * s.f_bavail;
    log_info("Free space: %lld bytes | required space %lld bytes", free_space, totalSize);

    if (free_space < totalSize) {
        log_error("Not enough free space");
        msgok(MSG_DIALOG::WARNING, fmt::format("Not enough free space\n\nRequired: {}\nAvailable: {}", calculateSize(totalSize), calculateSize(free_space)));
        return false;
    }

    long copiedSoFar = 0;
    mkdir(dest, 0777);
    bool ret = copyFolder(src, dest, totalSize, &copiedSoFar, move);
    if (move) {
        rmtree(src);
    }

    return ret;
    #else
    return true;
    #endif
}