

#include "utils.h"
#include "defines.h"
#include <sqlite3.h>
#include <iosfwd>
#include <string>
#include <fstream>
#include <map>
#include "game_saves.h"
#ifdef __ORBIS__
#include "installpkg.h"
#endif
#include "feature_classes.hpp"
char *err_msg = 0;

sqlite3 *db = NULL;
int count = 0;
//static const char* IF_PS5_db_path = "/data/itemzflow/USB_Apps.db";

// Function to sanitize a string for use in SQL
std::string sanitizeString(const std::string& str)
{
    std::string results;
    for ( std::string::const_iterator current = str.begin();
            current != str.end();
            ++ current ) {
        if ( *current == '\'' ) {
            results.push_back( '\'');
        }
        results.push_back( *current );
    }
    return results;
}

sqlite3 *SQLiteOpenDb(char *path)
{
    int ret;
    struct stat sb;

    // check if our sqlite db is already present in our path
    ret = sceKernelStat(path, &sb);
    if (ret != 0)
    {
        log_error("[ERROR][SQLLDR][%s][%d] sceKernelStat(%s) failed: 0x%08X", __FUNCTION__, __LINE__, path, ret);
        goto sqlite_error_out;
    }
    ret = sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE, NULL);
sqlite_error_out:
    // return error is something went wrong errmsg on klogs
    if (ret != SQLITE_OK)
    {
        log_error("[ERROR][SQLLDR][%s][%d] ERROR 0x%08X: %s", __FUNCTION__, __LINE__, ret, sqlite3_errmsg(db));
        return NULL;
    }
    else
        return db;
}

int callback(void *, int, char **, char **);

bool SQL_Load_DB(const char *dir)
{
    if(db){
        sqlite3_close(db), db = NULL;
    }
    db = SQLiteOpenDb((char *)dir);
    if (db == NULL)
    {
        log_error("[SQLLDR] Cannot open database: %s", sqlite3_errmsg(db));
        return false;
    }

    return true;
}

bool SQL_Exec(const char *smt, int (*cb)(void *, int, char **, char **))
{
    log_info("Executing SQL Statement: %s", smt);
    if (sqlite3_exec(db, smt, cb, 0, &err_msg) != SQLITE_OK)
    {

        log_info("Failed to select data");
        log_info("SQL error: %s", err_msg);

        sqlite3_free(err_msg);

        return false;
    }
    else
        return true;
}

int int_cb(void *in, int idc, char **argv, char **idc_2)
{
    if (argv[0] != NULL) {  // Check if argv[0] is not NULL
        int *numb = (int *)in;
        *numb = atoi(argv[0]);
    }
    else{
        log_error("SQL CB argv[0] is NULL");
    }
    return 0;
}

/*
 * Arguments:
 *
 *   unused - Ignored in this case, see the documentation for sqlite3_exec
 *    count - The number of columns in the result set
 *     data - The row's data
 *  columns - The column names
 
static int token_cb(void *in, int count, char **data, char **columns)
{
    char **result_str = (char **)in;
    *result_str = sqlite3_mprintf("%q", data[0]);

    return 0;
}
*/
/*
appdb = open_sqlite_db("/system_data/priv/mms/app.db");
    read_hdd_savegames(userPath, list, appdb);
    sqlite3_close(appdb);

    
typedef struct save_entry
{
    char* name;
    char* title_id;
    char* path;
    char* dir_name;
    char* main_title;
    char* sub_title;
    char* detail;
    char* mtime;
    int numb_of_saves;
    uint32_t userid;
    uint32_t blocks;
    uint16_t flags;
    uint16_t type;
} save_entry_t;
*/
//SELECT COUNT(*) FROM savedata WHERE title_id = 'CUSA00744'

bool AppDBVisiable(std::string tid, APP_DB_VIS opt, int write_value)
{
    std::string tmp;
    u32 userId = -1;
    int ret, vis = -1;

    //log_info("[SQL] tid %s write_value %i", tid.c_str(), write_value);

    if (!SQL_Load_DB(APP_DB))
        return false;

    if ((ret = sceUserServiceGetForegroundUser(&userId)) != ITEMZCORE_SUCCESS)
    {
        log_error("[ERROR] sceUserServiceGetForegroundUser ERROR: %x", ret);
        goto error;
    }

    if (opt == VIS_READ)
        tmp = fmt::format("SELECT visible FROM tbl_appbrowse_0{0:d} WHERE titleId = '{1:}'", userId, tid);
    else
        tmp = fmt::format("UPDATE tbl_appbrowse_0{0:d} SET visible = {1:d} WHERE titleId = '{2:}'", userId, write_value, tid);

    log_info("tmp: %s", tmp.c_str());

    if (sqlite3_exec(db, tmp.c_str(), int_cb, &vis, &err_msg) != SQLITE_OK && !err_msg )
    {
        log_error("[APPDB] Failed to fetch data: %s", sqlite3_errmsg(db));
        
        if(err_msg)
           sqlite3_free(err_msg);

        goto error;
    }

    sqlite3_close(db);

    if (opt == VIS_WRTIE)
    {
        fmt::print("[APPDB] Changed to {}", write_value);
        return true;
    }

    return vis == 1;

error:
    sqlite3_close(db);
    return false;
}

bool Reactivate_external_content(bool is_addcont_open = true) {

  OrbisUserServiceRegisteredUserIdList userIdList;
  std::string query;
  int ret = -1;
  char name[20];

  if (!is_addcont_open && !SQL_Load_DB("/system_data/priv/mms/addcont.db")) {
    log_error("Failed to open db");
    return false;
  }

  if (sqlite3_exec(db, "UPDATE addcont SET status = 0", NULL, NULL, & err_msg) != SQLITE_OK || err_msg) {
    log_error("[Addcont] Failed: %s | UPDATE addcont SET status = 0", sqlite3_errmsg(db));
    sqlite3_free(err_msg);
  }

  sqlite3_close(db), db = NULL;

  memset(&userIdList, 0, sizeof(userIdList));

  if (!SQL_Load_DB(APP_DB)) {
    log_error("Failed to open app.db");
    return false;
  }

  for (int i = 0; i < ORBIS_USER_SERVICE_MAX_REGISTER_USERS; i++) {
    if (userIdList.userId[i] != 0xFF) {

      memset(name, 0, sizeof(name));
      if ((ret = sceUserServiceGetUserName(userIdList.userId[i], name, sizeof(name))) != ITEMZCORE_SUCCESS) {
        log_error("[ERROR] sceUserServiceGetUserName ERROR: 0x%X for %d 0x%x", ret, i, userIdList.userId[i]);
        continue;
      }
      //do the following sql commands below
      //UPDATE tbl_appinfo SET val = 0 WHERE key = "_external_hdd_app_status";
      //UPDATE tbl_appbrowse_#### SET externalHddAppStatus = 0;
      query = fmt::format("UPDATE tbl_appbrowse_0{0:d} SET externalHddAppStatus = 0", userIdList.userId[i]);
      if (sqlite3_exec(db, query.c_str(), NULL, NULL, & err_msg) != SQLITE_OK || err_msg) {
        log_error("[AppDB] Failed: %s | %s", sqlite3_errmsg(db), query.c_str());
        sqlite3_free(err_msg);
        return false;
      }
    }

  }

  if (sqlite3_exec(db, "UPDATE tbl_appinfo SET val = 0 WHERE key = \"_external_hdd_app_status\"", NULL, NULL, & err_msg) != SQLITE_OK || err_msg) {
    log_error("[AppDB] Failed: %s | UPDATE tbl_appinfo SET val = 0 WHERE key = \"_external_hdd_app_status\"", sqlite3_errmsg(db));
    sqlite3_free(err_msg);
    return false;
  }

  sqlite3_close(db), db = NULL;
  return true;
}

bool change_game_name(std::string new_title, std::string tid, std::string sfo_path)
{
    std::string tmp;
    std::string tmp2;
    int ret =0;
    u32 userId = -1;

    std::string ini_content = fmt::format("[APP_NAME]\nCUSTOM_NAME={0:}", new_title);

    if (!SQL_Load_DB(APP_DB))
        return false;

    if ((ret = sceUserServiceGetForegroundUser(&userId)) != ITEMZCORE_SUCCESS)
    {
        log_error("[ERROR] sceUserServiceGetForegroundUser ERROR: 0x%X", ret);
        
        sqlite3_close(db);
        return false;
    }

    tmp = fmt::format("UPDATE tbl_appbrowse_0{0:d} SET titleName='{1:}' WHERE titleId='{2:}'", userId, new_title, tid);

    if (sqlite3_exec(db, tmp.c_str(), NULL, NULL, &err_msg) != SQLITE_OK || err_msg)
    {
        log_error("[TID] Failed to fetch data: %s", sqlite3_errmsg(db));
        sqlite3_free(err_msg);

        sqlite3_close(db);
        return false;
    }

    sqlite3_close(db);

    fmt::print("[TID] Saving Title Name: {}", new_title);

    std::ofstream out(fmt::format("/user/app/ITEM00001/custom_app_names/{}.ini", tid));
    if(out.bad()){
       log_error("Failed to create custom_name ini file");
       return false;
    }
    
    out << ini_content;
    out.close();

    return true;
}

/* stolen from Buc at the request of a user*/
/* https://github.com/bucanero/apollo-ps4/commit/c7bbd262f9b243878372ae7134f581e016467c9c */
bool addcont_dlc_rebuild(const char* db_path, bool ext_hdd)
{
	char path[1024];
	DIR *dp, *dp2;
    int ret = 0;
	struct dirent *dirp, *dirp2;
	std::string query;
    DLC_PKG_DETAILS dlc_details;
    OrbisUserServiceRegisteredUserIdList userIdList;
   
	dp = opendir(ext_hdd ?  "/mnt/ext0/user/addcont" : "/user/addcont");
	if (!dp)
	{
		log_error("Failed to open %s", ext_hdd ?  "/mnt/ext0/user/addcont" : "/user/addcont");
		return false;
	}

	if (!SQL_Load_DB(db_path)){
        log_error("Failed to open %s", db_path);
        return false;
    }

    if ((ret = sceUserServiceGetRegisteredUserIdList(&userIdList) != ITEMZCORE_SUCCESS)) {
        log_error("[ERROR] sceUserServiceGetForegroundUser ERROR: 0x%X", ret);
        sqlite3_close(db), db = NULL;
        return false;
    }

	while ((dirp = readdir(dp)) != NULL)
	{
		if (dirp->d_type != DT_DIR || dirp->d_namlen != 9)
			continue;

        snprintf(path, sizeof(path), "%s/%s", ext_hdd ?  "/mnt/ext0/user/addcont" : "/user/addcont", dirp->d_name);

		dp2 = opendir(path);
		if (!dp2)
			continue;

		while ((dirp2 = readdir(dp2)) != NULL)
		{
			if (dirp2->d_type != DT_DIR || dirp2->d_namlen != 16)
				continue;

			snprintf(path, sizeof(path), "%s/%s/%s/ac.pkg", ext_hdd ? "/mnt/ext0/user/addcont" : "/user/addcont", dirp->d_name, dirp2->d_name);

			if(!dlc_pkg_details(path, dlc_details)){
                log_error("Failed to get dlc details for %s", path);
                continue;
            }

			log_info("Adding %s to addcont...", dirp2->d_name);
            //TODO "IRO_TAG"
            query = fmt::format("INSERT OR IGNORE INTO addcont(title_id, dir_name, content_id, title, version, attribute, status, titles_JAPANESE, titles_ENGLISH_US, titles_FRENCH, titles_SPANISH, titles_GERMAN, titles_ITALIAN, titles_DUTCH, titles_PORTUGUESE_PT, titles_RUSSIAN, titles_KOREAN, titles_CHINESE_T, titles_CHINESE_S, titles_FINNISH, titles_SWEDISH, titles_DANISH, titles_NORWEGIAN, titles_POLISH, titles_PORTUGUESE_BR, titles_ENGLISH_GB, titles_TURKISH, titles_SPANISH_LA, titles_ARABIC, titles_FRENCH_CA, titles_CZECH, titles_HUNGARIAN, titles_GREEK, titles_ROMANIAN, titles_THAI, titles_VIETNAMESE, titles_INDONESIAN) VALUES('{0:}', \"{1:}\", '{2:}', '{3:}', {4:}, '{5:}', '{6:}', '{7:}', '{8:}', '{9:}', '{10:}', '{11:}', '{12:}', '{13:}', '{14:}', '{15:}', '{16:}', '{17:}', '{18:}', '{19:}', '{20:}', '{21:}', '{22:}', '{23:}', '{24:}', '{25:}', '{26:}', '{27:}', '{28:}', '{29:}', '{30:}', '{31:}', '{32:}', '{33:}', '{34:}', '{35:}', '{36:}')",dirp->d_name, dirp2->d_name, dlc_details.contentid, dlc_details.name, ext_hdd ? "1610612736" : "536870912", dlc_details.version.c_str(), dlc_details.esd["IRO_TAG"].empty() ? "0" : dlc_details.esd["IRO_TAG"].c_str(), dlc_details.esd["TITLE_00"].c_str(), dlc_details.esd["TITLE_01"].c_str(), dlc_details.esd["TITLE_02"].c_str(), dlc_details.esd["TITLE_03"].c_str(), dlc_details.esd["TITLE_04"].c_str(), dlc_details.esd["TITLE_05"].c_str(), dlc_details.esd["TITLE_06"].c_str(), dlc_details.esd["TITLE_07"].c_str(), dlc_details.esd["TITLE_08"].c_str(), dlc_details.esd["TITLE_09"].c_str(), dlc_details.esd["TITLE_10"].c_str(), dlc_details.esd["TITLE_11"].c_str(), dlc_details.esd["TITLE_12"].c_str(), dlc_details.esd["TITLE_13"].c_str(), dlc_details.esd["TITLE_14"].c_str(), dlc_details.esd["TITLE_15"].c_str(), dlc_details.esd["TITLE_16"].c_str(), dlc_details.esd["TITLE_17"].c_str(), dlc_details.esd["TITLE_18"].c_str(), dlc_details.esd["TITLE_19"].c_str(), dlc_details.esd["TITLE_20"].c_str(), dlc_details.esd["TITLE_21"].c_str(), dlc_details.esd["TITLE_22"].c_str(), dlc_details.esd["TITLE_23"].c_str(), dlc_details.esd["TITLE_24"].c_str(), dlc_details.esd["TITLE_25"].c_str(), dlc_details.esd["TITLE_26"].c_str(), dlc_details.esd["TITLE_27"].c_str(), dlc_details.esd["TITLE_28"].c_str(), dlc_details.esd["TITLE_29"].c_str());			
            if (sqlite3_exec(db, query.c_str(), NULL, NULL, NULL) != SQLITE_OK){
				log_info("[PASS1] addcont insert failed: %s", sqlite3_errmsg(db));

                std::replace_if(dlc_details.name.begin(), dlc_details.name.end(), [](auto ch) { return ::ispunct(ch); }, ' ');

                query = fmt::format("INSERT OR IGNORE INTO addcont(title_id, dir_name, content_id, title, version, attribute, status, titles_JAPANESE, titles_ENGLISH_US, titles_FRENCH, titles_SPANISH, titles_GERMAN, titles_ITALIAN, titles_DUTCH, titles_PORTUGUESE_PT, titles_RUSSIAN, titles_KOREAN, titles_CHINESE_T, titles_CHINESE_S, titles_FINNISH, titles_SWEDISH, titles_DANISH, titles_NORWEGIAN, titles_POLISH, titles_PORTUGUESE_BR, titles_ENGLISH_GB, titles_TURKISH, titles_SPANISH_LA, titles_ARABIC, titles_FRENCH_CA, titles_CZECH, titles_HUNGARIAN, titles_GREEK, titles_ROMANIAN, titles_THAI, titles_VIETNAMESE, titles_INDONESIAN) VALUES('{0:}', \"{1:}\", '{2:}', '{3:}', {4:}, '{5:}', '{6:}', '{7:}', '{8:}', '{9:}', '{10:}', '{11:}', '{12:}', '{13:}', '{14:}', '{15:}', '{16:}', '{17:}', '{18:}', '{19:}', '{20:}', '{21:}', '{22:}', '{23:}', '{24:}', '{25:}', '{26:}', '{27:}', '{28:}', '{29:}', '{30:}', '{31:}', '{32:}', '{33:}', '{34:}', '{35:}', '{36:}')", dirp->d_name, dirp2->d_name, dlc_details.contentid, dlc_details.name, ext_hdd ? "1610612736" : "536870912", dlc_details.version.c_str(), dlc_details.esd["IRO_TAG"].empty() ? "0" : dlc_details.esd["IRO_TAG"].c_str(), dlc_details.esd["TITLE_00"].c_str(), dlc_details.esd["TITLE_01"].c_str(), dlc_details.esd["TITLE_02"].c_str(), dlc_details.esd["TITLE_03"].c_str(), dlc_details.esd["TITLE_04"].c_str(), dlc_details.esd["TITLE_05"].c_str(), dlc_details.esd["TITLE_06"].c_str(), dlc_details.esd["TITLE_07"].c_str(), dlc_details.esd["TITLE_08"].c_str(), dlc_details.esd["TITLE_09"].c_str(), dlc_details.esd["TITLE_10"].c_str(), dlc_details.esd["TITLE_11"].c_str(), dlc_details.esd["TITLE_12"].c_str(), dlc_details.esd["TITLE_13"].c_str(), dlc_details.esd["TITLE_14"].c_str(), dlc_details.esd["TITLE_15"].c_str(), dlc_details.esd["TITLE_16"].c_str(), dlc_details.esd["TITLE_17"].c_str(), dlc_details.esd["TITLE_18"].c_str(), dlc_details.esd["TITLE_19"].c_str(), dlc_details.esd["TITLE_20"].c_str(), dlc_details.esd["TITLE_21"].c_str(), dlc_details.esd["TITLE_22"].c_str(), dlc_details.esd["TITLE_23"].c_str(), dlc_details.esd["TITLE_24"].c_str(), dlc_details.esd["TITLE_25"].c_str(), dlc_details.esd["TITLE_26"].c_str(), dlc_details.esd["TITLE_27"].c_str(), dlc_details.esd["TITLE_28"].c_str(), dlc_details.esd["TITLE_29"].c_str());			
                if (sqlite3_exec(db, query.c_str(), NULL, NULL, NULL) != SQLITE_OK){
                    log_info("[PASS2] addcont insert failed: %s", sqlite3_errmsg(db));
                    log_info("addcont insert failed for %s", query.c_str());
                }
            
            }
            log_info("addcont insert success %s", query.c_str());
            dlc_details.esd.clear();
            dlc_details.name.clear();
            dlc_details.version.clear(); 
            dlc_details.contentid.clear();  
		}
		closedir(dp2);
	}
	closedir(dp);

    Reactivate_external_content();

	return true;
}


bool insert_app_info(std::string titleId, std::string key, std::string value = std::string(), bool is_string = true){

    std::string tmp;
    long numb = 0;

    if(!value.empty() && !is_string){
        numb = std::stol(value);
    }

    if(is_string){
        tmp = fmt::format("INSERT OR IGNORE INTO tbl_appinfo(titleId, key, val) VALUES('{0:}', '{1:}', \"{2:}\")", titleId, key, value);
    }
    else
        tmp = fmt::format("INSERT OR IGNORE INTO tbl_appinfo(titleId, key, val) VALUES('{0:}', '{1:}', {2:d})", titleId, key, numb);

    if (sqlite3_exec(db, tmp.c_str(), NULL, NULL, &err_msg) != SQLITE_OK || err_msg)
    {
        log_error("[AppDB] Failed: %s | %s", sqlite3_errmsg(db), tmp.c_str());
        sqlite3_free(err_msg);
        return false;
    }

    //log_info("AppDB insert success %s", tmp.c_str());
    return true;

}
 
bool Fix_Game_In_DB(item_t &item, bool is_ext_hdd) {
  std::string tmp;
  std::string tmp2;
  int ret = -1;
  u32 userId = -1;
  OrbisUserServiceRegisteredUserIdList userIdList;
  char name[20];

  memset(&userIdList, 0, sizeof(userIdList));

  if (!SQL_Load_DB(APP_DB))
    return false;

  if ((ret = sceUserServiceGetRegisteredUserIdList(&userIdList) != ITEMZCORE_SUCCESS)) {
      log_error("[ERROR] sceUserServiceGetRegisteredUserIdList ERROR: 0x%X", ret);
      sqlite3_close(db), db = NULL;
      return false;
  }

  for (int i = 0; i < ORBIS_USER_SERVICE_MAX_REGISTER_USERS; i++) {
    if (userIdList.userId[i] != 0xFF) {

      userId = userIdList.userId[i];
      memset(name, 0, sizeof(name));
	  if ((ret = sceUserServiceGetUserName(userIdList.userId[i], name, sizeof(name))) != ITEMZCORE_SUCCESS){
        log_error("[ERROR] sceUserServiceGetUserName ERROR: 0x%X for %d 0x%x", ret, i, userId);
        continue;
      }
      log_error("Running for User ID: 0x%x | %d name: %s", userId, i, name);

      int ver = (ps4_fw_version() >> 16);

      tmp = fmt::format("INSERT OR IGNORE INTO tbl_appbrowse_0{0:d}(titleId, contentId, titleName, metaDataPath, lastAccessTime, contentStatus, onDisc, parentalLevel, visible, sortPriority, pathInfo, lastAccessIndex, dispLocation, canRemove, category, contentType, pathInfo2, presentBoxStatus, entitlement, thumbnailUrl, lastUpdateTime, playableDate, contentSize, installDate, platform, uiCategory, skuId, disableLiveDetail, linkType, linkUri, serviceIdAddCont1, serviceIdAddCont2, serviceIdAddCont3, serviceIdAddCont4, serviceIdAddCont5, serviceIdAddCont6, serviceIdAddCont7, folderType, folderInfo, parentFolderId, positionInFolder, activeDate, entitlementTitleName, hddLocation, externalHddAppStatus, entitlementIdKamaji, mTime {1:}) VALUES('{2:}', \"{3:}\", '{4:}', '{5:}', '2022-02-06 14:03:08.359', 0, 0, '{6:}', 1, 100, 0, 0, 5, 1, '{7:}', 0, 0, 0, 0, NULL, NULL, NULL, '{8:d}', '2018-07-27 15:06:46.802', 0, '{9:}', NULL, 0, 0, NULL, '{10:}', '{11:}', '{12:}','{13:}', '{14:}', '{15:}', '{16:}', 0, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, '2022-05-06 18:02:21.702'{17:})", userId, (ver > 0x555) ? ",freePsPlusContent, entitlementActiveFlag, sizeOtherHdd, entitlementHidden, preorderPlaceholderFlag, gatingEntitlementJson" : "", item.info.id, item.extra_data.extra_sfo_data["CONTENT_ID"], item.info.name, item.flags.is_ext_hdd ? "/user/appmeta/external/" + item.info.id : "/user/appmeta/" + item.info.id, item.extra_data.extra_sfo_data["PARENTAL_LEVEL"], item.extra_data.extra_sfo_data["CATEGORY"], file_size(item.info.package.c_str()), (item.extra_data.extra_sfo_data["CATEGORY"] == "gde") ? "app" : "game", item.extra_data.extra_sfo_data["SERVICE_ID_ADDCONT_ADD_1"], item.extra_data.extra_sfo_data["SERVICE_ID_ADDCONT_ADD_2"], item.extra_data.extra_sfo_data["SERVICE_ID_ADDCONT_ADD_3"], item.extra_data.extra_sfo_data["SERVICE_ID_ADDCONT_ADD_4"], item.extra_data.extra_sfo_data["SERVICE_ID_ADDCONT_ADD_5"], item.extra_data.extra_sfo_data["SERVICE_ID_ADDCONT_ADD_6"], item.extra_data.extra_sfo_data["SERVICE_ID_ADDCONT_ADD_7"], (ver > 0x555) ? ", 0,0, 0, 0, 0, NULL" : "");
      if (sqlite3_exec(db, tmp.c_str(), NULL, NULL, & err_msg) != SQLITE_OK || err_msg) {
        log_error("[AppDB][PASS1] Failed: %s", sqlite3_errmsg(db));
        sqlite3_free(err_msg);
        log_info("[AppDB] Trying without special Chars ...");
        //remove spec char
       // std::replace_if(item.name.begin(), item.name.end(), [](auto ch) {
        //return ::ispunct(ch);
        //}, ' ');

        tmp = fmt::format("INSERT OR IGNORE INTO tbl_appbrowse_0{0:d}(titleId, contentId, titleName, metaDataPath, lastAccessTime, contentStatus, onDisc, parentalLevel, visible, sortPriority, pathInfo, lastAccessIndex, dispLocation, canRemove, category, contentType, pathInfo2, presentBoxStatus, entitlement, thumbnailUrl, lastUpdateTime, playableDate, contentSize, installDate, platform, uiCategory, skuId, disableLiveDetail, linkType, linkUri, serviceIdAddCont1, serviceIdAddCont2, serviceIdAddCont3, serviceIdAddCont4, serviceIdAddCont5, serviceIdAddCont6, serviceIdAddCont7, folderType, folderInfo, parentFolderId, positionInFolder, activeDate, entitlementTitleName, hddLocation, externalHddAppStatus, entitlementIdKamaji, mTime {1:}) VALUES('{2:}', \"{3:}\", '{4:}', '{5:}', '2022-02-06 14:03:08.359', 0, 0, '{6:}', 1, 100, 0, 0, 5, 1, '{7:}', 0, 0, 0, 0, NULL, NULL, NULL, '{8:d}', '2018-07-27 15:06:46.802', 0, '{9:}', NULL, 0, 0, NULL, '{10:}', '{11:}', '{12:}','{13:}', '{14:}', '{15:}', '{16:}', 0, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, '2022-05-06 18:02:21.702'{17:})", userId, (ver > 0x555) ? ",freePsPlusContent, entitlementActiveFlag, sizeOtherHdd, entitlementHidden, preorderPlaceholderFlag, gatingEntitlementJson" : "", item.info.id, item.extra_data.extra_sfo_data["CONTENT_ID"], sanitizeString(item.info.name), item.flags.is_ext_hdd ? "/user/appmeta/external/" + item.info.id : "/user/appmeta/" + item.info.id, item.extra_data.extra_sfo_data["PARENTAL_LEVEL"], item.extra_data.extra_sfo_data["CATEGORY"], file_size(item.info.package.c_str()), (item.extra_data.extra_sfo_data["CATEGORY"] == "gde") ? "app" : "game", item.extra_data.extra_sfo_data["SERVICE_ID_ADDCONT_ADD_1"], item.extra_data.extra_sfo_data["SERVICE_ID_ADDCONT_ADD_2"], item.extra_data.extra_sfo_data["SERVICE_ID_ADDCONT_ADD_3"], item.extra_data.extra_sfo_data["SERVICE_ID_ADDCONT_ADD_4"], item.extra_data.extra_sfo_data["SERVICE_ID_ADDCONT_ADD_5"], item.extra_data.extra_sfo_data["SERVICE_ID_ADDCONT_ADD_6"], item.extra_data.extra_sfo_data["SERVICE_ID_ADDCONT_ADD_7"], (ver > 0x555) ? ", 0,0, 0, 0, 0, NULL" : "");
        if (sqlite3_exec(db, tmp.c_str(), NULL, NULL, & err_msg) != SQLITE_OK || err_msg) {
          log_error("[AppDB][PASS2][FATAL] Failed: %s", sqlite3_errmsg(db));
          sqlite3_free(err_msg);
          sqlite3_close(db);
          return false;
        }

      }
      for (int z = 1; z <= 7; z++) {
        // Construct the UPDATE statement as a string
        std::string sql = fmt::format("UPDATE tbl_appbrowse_0{0:d} SET serviceIdAddCont{1:d} = NULL WHERE serviceIdAddCont{2:d} = ''", userId, z, z);
        // Execute the UPDATE statement
        char * error_message = 0;
        ret = sqlite3_exec(db, sql.c_str(), 0, 0, & error_message);
        if (ret) {
          sqlite3_free(error_message);
          log_error("[AppDB][SASS][FATAL] Failed: %s | %s", sqlite3_errmsg(db), tmp.c_str());
        }
      }
    }
  }

  //print out extra sfo data
  if (!insert_app_info(item.info.id, "TITLE", item.info.name)) {
    insert_app_info(item.info.id, "TITLE", sanitizeString(item.info.name));
  }
  insert_app_info(item.info.id, "TITLE_ID", item.info.id);
  insert_app_info(item.info.id, "_metadata_path", item.flags.is_ext_hdd ? "/user/appmeta/external/" + item.info.id : "/user/appmeta/" + item.info.id);
  insert_app_info(item.info.id, "_org_path", item.flags.is_ext_hdd ? "/mnt/ext0/user/app/" + item.info.id : "/user/app/" + item.info.id);
  insert_app_info(item.info.id, "APP_VER", item.info.version);
  insert_app_info(item.info.id, "ATTRIBUTE", item.extra_data.extra_sfo_data["ATTRIBUTE"], false);
  insert_app_info(item.info.id, "CATEGORY", item.extra_data.extra_sfo_data["CATEGORY"]);
  insert_app_info(item.info.id, "CONTENT_ID", item.extra_data.extra_sfo_data["CONTENT_ID"]);
  insert_app_info(item.info.id, "VERSION", item.extra_data.extra_sfo_data["VERSION"]);
  insert_app_info(item.info.id, "#_access_index", "1", false);
  insert_app_info(item.info.id, "#_last_access_time", "2022-01-21 15:04:39.822");
  insert_app_info(item.info.id, "#_contents_status", "0", false);
  insert_app_info(item.info.id, "#_mtime", "2022-01-21 15:04:39.822");
  insert_app_info(item.info.id, "_contents_location", "0", false);
  insert_app_info(item.info.id, "#_update_index", "0", false);
  insert_app_info(item.info.id, "#exit_type", "0", false);
  insert_app_info(item.info.id, "DISP_LOCATION_1", "0", false);
  insert_app_info(item.info.id, "DISP_LOCATION_2", "0", false);
  insert_app_info(item.info.id, "DOWNLOAD_DATA_SIZE", item.extra_data.extra_sfo_data["DOWNLOAD_DATA_SIZE"], false);
  insert_app_info(item.info.id, "FORMAT", "obs");
  insert_app_info(item.info.id, "PARENTAL_LEVEL", item.extra_data.extra_sfo_data["PARENTAL_LEVEL"], false);
  insert_app_info(item.info.id, "SYSTEM_VER", item.extra_data.extra_sfo_data["SYSTEM_VER"], false);

  if (!item.extra_data.extra_sfo_data["INSTALL_DIR_SAVEDATA"].empty())
    insert_app_info(item.info.id, "INSTALL_DIR_SAVEDATA", item.extra_data.extra_sfo_data["INSTALL_DIR_SAVEDATA"]);

  for (int i = 1; i <= 4; i++) {
    insert_app_info(item.info.id, fmt::format("USER_DEFINED_PARAM_{0:d}", i), item.extra_data.extra_sfo_data[fmt::format("USER_DEFINED_PARAM_{0:d}", i)], false);
  }
  for (int i = 1; i <= 7; i++) {
    std::string SERV_ID = fmt::format("SERVICE_ID_ADDCONT_APPINFO_ADD_{0:d}", i);
    std::string SERV_ID2 = fmt::format("SERVICE_ID_ADDCONT_ADD_{0:d}", i);
    if (!item.extra_data.extra_sfo_data[SERV_ID].empty())
      insert_app_info(item.info.id, SERV_ID2, item.extra_data.extra_sfo_data[SERV_ID]);
    else
      insert_app_info(item.info.id, SERV_ID2, "0", false);
  }
  for (int i = 0; i < 30; i++) {
    std::string title = fmt::format("TITLE_{0:}{1:d}", (i < 10) ? "0" : "", i);
    if (!item.extra_data.extra_sfo_data[title].empty()) {
      if (!insert_app_info(item.info.id, title, item.extra_data.extra_sfo_data[title])) {

        //std::replace_if(item.extra_data.extra_sfo_data[title].begin(), item.extra_data.extra_sfo_data[title].end(), [](auto ch) {
       // return ::ispunct(ch);
        //}, ' ');

        insert_app_info(item.info.id, title, sanitizeString(item.extra_data.extra_sfo_data[title]));
      }
    }
  }
  insert_app_info(item.info.id, "_contents_ext_type", "0", false);
  insert_app_info(item.info.id, "_current_slot", "0", false);
  insert_app_info(item.info.id, "_disable_live_detail", "0", false);
  insert_app_info(item.info.id, "_external_hdd_app_status", "0", false);
  insert_app_info(item.info.id, "_hdd_location", "0", false);
  insert_app_info(item.info.id, "_path_info", "0", false);
  insert_app_info(item.info.id, "_path_info_2", "0", false);
  insert_app_info(item.info.id, "_size_other_hdd", "0", false);
  insert_app_info(item.info.id, "_sort_priority", "100", false);
  insert_app_info(item.info.id, "_uninstallable", "1", false);
  insert_app_info(item.info.id, "_view_category", "0", false);
  insert_app_info(item.info.id, "_working_status", "0", false);
  insert_app_info(item.info.id, "ATTRIBUTE2", item.extra_data.extra_sfo_data["ATTRIBUTE2"], false);
  insert_app_info(item.info.id, "ATTRIBUTE_INTERNAL", item.extra_data.extra_sfo_data["ATTRIBUTE_INTERNAL"], false);
  insert_app_info(item.info.id, "PT_PARAM", item.extra_data.extra_sfo_data["PT_PARAM"], false);
  insert_app_info(item.info.id, "APP_TYPE", item.extra_data.extra_sfo_data["APP_TYPE"], false);
  insert_app_info(item.info.id, "REMOTE_PLAY_KEY_ASSIGN", item.extra_data.extra_sfo_data["REMOTE_PLAY_KEY_ASSIGN"], false);
  insert_app_info(item.info.id, "#_promote_time", "2022-01-21 15:04:39.822");
  insert_app_info(item.info.id, "#_booted", "1", false);
  insert_app_info(item.info.id, "#_size", std::to_string(file_size(item.info.package.c_str())), false);

  sqlite3_close(db), db = NULL;

  return true;
}


int retrieveFVapps(ThreadSafeVector<item_t>& out_vec, Sort_Category category, Favorites &favs) {
    // EXPERIMENTAL CODE NOT AVAILABLE AT RELEASE
    // COME BACK WHEN ITS NO LONGER EXPERIMENTAL
    return 0;
}

bool Inject_SQL_app(const std::string& tidValue, const std::string& sys_path, const std::string& contentID, const std::string& AppTitle) {
     // EXPERIMENTAL CODE NOT AVAILABLE AT RELEASE
     // COME BACK WHEN ITS NO LONGER EXPERIMENTAL
     return false;
}

bool EditDataIFPS5DB(const std::string& tidValue, const std::string& title,const std::string& gmPathValue, bool delete_record) {
     // EXPERIMENTAL CODE NOT AVAILABLE AT RELEASE
     // COME BACK WHEN ITS NO LONGER EXPERIMENTAL
     return true;
}