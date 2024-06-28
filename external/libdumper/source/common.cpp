#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "common.hpp"
#include <sqlite3.h>
#include <vector>
#include <tuple>
#include <filesystem>


void ProgUpdate(uint32_t prog, std::string fmt)
{
      
    int status = sceMsgDialogUpdateStatus();
	if (COMMON_DIALOG_STATUS_RUNNING == status) {
		sceMsgDialogProgressBarSetValue(0, prog);
        sceMsgDialogProgressBarSetMsg(0, fmt.c_str());
	}
}


bool copy_dir(const std::string& source_dir, const std::string& dest_dir) {
    std::filesystem::path src_dir_path(source_dir);
    std::filesystem::path dst_dir_path(dest_dir);
    std::error_code ec;

    if (!std::filesystem::exists(src_dir_path, ec)) {
        log_error("Source directory does not exist: %s", source_dir.c_str());
        return false;
    }

    std::filesystem::create_directory(dst_dir_path, ec);
    if (ec) {
        log_error("Failed to create destination directory: %s", ec.message().c_str());
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(src_dir_path, ec)) {
        if (ec) {
            log_error("Failed to iterate source directory: %s", ec.message().c_str());
            return false;
        }
        std::filesystem::path entry_path = entry.path();
        std::string file_name = entry_path.filename().string();

        if (file_name == "." || file_name == ".." || file_name.find("nobackup") != std::string::npos) {
            continue;
        }

        std::filesystem::path dst_path = dst_dir_path / entry_path.filename();

        if (std::filesystem::is_directory(entry_path, ec)) {
            if(ec) {
                log_error("Failed to check if %s is a directory: %s", entry_path.string().c_str(), ec.message().c_str());
                continue;
            }
            if (!copy_dir(entry_path.string(), dst_path.string())) {
                log_info("Failed to copy directory %s", entry_path.string().c_str());
                return false;
            }
        } else if (std::filesystem::is_regular_file(entry_path, ec)) {
            std::filesystem::copy(entry_path, dst_path, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                log_error("Failed to copy file %s: %s", entry_path.string().c_str(), ec.message().c_str());
                return false;
            }
        }
    }

    return true;
}
// Function that will be called for each row returned by the query
static int dlc_callback(void* data, int argc, char** argv, char** azColName) {
    auto* results = static_cast<std::vector<std::tuple<std::string, std::string, std::string>>*>(data);

    if (argc == 3 && argv[0] && argv[1] && argv[2]) {
        results->emplace_back(argv[0], argv[1], argv[2]);
    }

    return 0;
}

std::vector<std::tuple<std::string, std::string, std::string>> query_dlc_database(const std::string& title_id) {
    sqlite3* db;
    char* zErrMsg = 0;
    int rc;
    std::vector<std::tuple<std::string, std::string, std::string>> results;

    // Connect to the database
    rc = sqlite3_open_v2("/system_data/priv/mms/addcont.db", &db, SQLITE_OPEN_READWRITE, NULL);
    if (rc) {
        log_info("Can't open database");
        return results;
    } 

    std::string sql_query = "SELECT dir_name, title, content_id FROM addcont WHERE title_id='" + title_id + "';";

    // Execute SQL statement
    rc = sqlite3_exec(db, sql_query.c_str(), dlc_callback, &results, &zErrMsg);

    if (rc != SQLITE_OK) {
        log_error("SQL error: %s", zErrMsg);
        sqlite3_free(zErrMsg);
    }

    sqlite3_close(db);

    return results;
}

// This function is used because std::fs::relative path crashes on PS4 atleast with the OrbisDev SDK
// with a UaF heap erro, but this works fone
std::filesystem::path customRelative(std::filesystem::path from, std::filesystem::path to)
{
    // create absolute paths
    std::filesystem::path p = std::filesystem::absolute(from);
    std::filesystem::path r = std::filesystem::absolute(to);

    // if root paths are different, return absolute path
    if( p.root_path() != r.root_path() )
        return p;

    // initialize relative path
    std::filesystem::path result;

    // find out where the two paths diverge
    std::filesystem::path::const_iterator itr_path = p.begin();
    std::filesystem::path::const_iterator itr_relative_to = r.begin();
    while( itr_path != p.end() && itr_relative_to != r.end() && *itr_path == *itr_relative_to ) {
        ++itr_path;
        ++itr_relative_to;
    }

    // add "../" for each remaining token in relative_to
    if( itr_relative_to != r.end() ) {
        ++itr_relative_to;
        while( itr_relative_to != r.end() ) {
            result /= "..";
            ++itr_relative_to;
        }
    }

    // add remaining path
    while( itr_path != p.end() ) {
        result /= *itr_path;
        ++itr_path;
    }

    log_info("%s: from = %s, to = %s: res %s", __FUNCTION__, from.string().c_str(), to.string().c_str(), result.string().c_str());

    return result;
}

std::string customFileSize(const std::string& filename) {
    std::ifstream file(filename, std::ifstream::ate | std::ifstream::binary);
    if (!file.is_open()) {
        log_error("Could not open file: %s", filename.c_str());
        return 0;
    }
    return std::to_string(file.tellg());
}