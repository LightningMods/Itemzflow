// Copyright (c) 2021 Al Azif
// License: GPLv3

#include "common.hpp"
#include "elf.hpp"
#include "fself.hpp"
#include "gp4.hpp"
#include "npbind.hpp"
#include "pfs.hpp"
#include "pkg.hpp"
#include "dumper.h"
#include "dump.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include "log.h"
extern "C" {
#include "lang.h"
}

namespace dump {
bool __dump(const std::string &usb_device, const std::string &title_id, Dump_Options opt, const std::string& title) {

    
  if (!LoadDumperLangs(DumperGetLang())) {
        if (!LoadDumperLangs(0x01)) {
            if(!load_dumper_embdded_eng())
                return false;
        }
        else
            log_debug("Loaded the backup, lang %i failed to load", DumperGetLang());
  }
  if (opt > TOTAL_OF_OPTS)
  {
      log_error("OPT is out of range: %i", opt);
      return false;
  }
  

  std::string output_directory = title_id;
  if (opt == GAME_PATCH) {
    output_directory += "-patch"; // Add "-patch" to "patch" types so it doesn't overlap with "base"
  } else if (opt == THEME_UNLOCK) {
    output_directory += "-unlock"; // Add "-unlock" to theme type so we can differentiate between "install" and "unlock" PKGs
  }

  // Create base path
  std::filesystem::path output_path(usb_device);
  output_path /= output_directory;

  // Make sure the base path is a directory or can be created
  if (!std::filesystem::is_directory(output_path) && !std::filesystem::create_directories(output_path)) {
    log_error("Unable to create output directory");
    return false;
  }

  // Check for .dumping semaphore
  std::filesystem::path dumping_semaphore(usb_device);
  dumping_semaphore /= output_directory + ".dumping";
  if (std::filesystem::exists(dumping_semaphore)) {
    log_error("This dump is currently dumping or closed unexpectedly! Please delete existing dump to enable dumping.");
  }

  // Check for .complete semaphore
  std::filesystem::path complete_semaphore(usb_device);
  complete_semaphore /= output_directory + ".complete";
  if (std::filesystem::exists(complete_semaphore)) {
    log_error("This dump has already been completed! Please delete existing dump to enable dumping.");
  }


  // Create .dumping semaphore
  std::ofstream dumping_sem_touch(dumping_semaphore);
  dumping_sem_touch.close();
  if (std::filesystem::exists(dumping_semaphore)) {
    log_error("Unable to create dumping semaphore!");
  }


  // Create "sce_sys" directory in the output directory
  std::filesystem::path sce_sys_path(output_path);
  sce_sys_path /= "sce_sys";
  if (!std::filesystem::is_directory(sce_sys_path) && !std::filesystem::create_directories(sce_sys_path)) {
    log_error("Unable to create `sce_sys` directory");
    return false;
  }

  std::filesystem::path pkg_path;
  try {

      if (opt == THEME_UNLOCK || opt == ADDITIONAL_CONTENT_NO_DATA) {
          std::filesystem::path param_destination(sce_sys_path);
          param_destination /= "param.sfo";

          if (opt == THEME_UNLOCK) {
              // Copy theme install PKG
              std::filesystem::path install_source("/user/addcont/I00000002");
              install_source /= title_id;
              install_source /= "ac.pkg";

              std::filesystem::path install_destination(usb_device);
              install_destination /= title_id + "-install.pkg";

              // Copy param.sfo
              if (!std::filesystem::copy_file(install_source, install_destination, std::filesystem::copy_options::overwrite_existing)) {
                  log_error("Unable to copy %s", install_source.string().c_str());
              }

              std::filesystem::path param_source("/system_data/priv/appmeta/addcont/I00000002");
              param_source /= title_id;
              param_source /= "param.sfo";

              if (!std::filesystem::copy_file(param_source, param_destination, std::filesystem::copy_options::overwrite_existing)) {
                  log_error("Unable to copy %s", param_source.c_str());
                  return false;
              }
          }
          else {
              // TODO: Find and copy... or create... "param.sfo" for "additional-content-no-data" at param_destination
          }
      }
      else { // "base", "patch", "remaster", "theme", "additional-content-data"
     // UnPKG
          std::filesystem::path pkg_directory_path;
          pkg_directory_path /= "/user";
          if (opt == BASE_GAME || opt == REMASTER) {
              pkg_directory_path /= "app";
              pkg_directory_path /= title_id;
              pkg_directory_path /= "app.pkg";
          }
          else if (opt == GAME_PATCH) {
              pkg_directory_path /= "patch";
              pkg_directory_path /= title_id;
              pkg_directory_path /= "patch.pkg";
          }
          else if (opt == THEME) {
              pkg_directory_path /= "addcont/I00000002";
              pkg_directory_path /= title_id;
              pkg_directory_path /= "ac.pkg";
          }
          else if (opt == ADDITIONAL_CONTENT_DATA) {
              pkg_directory_path /= "addcont";
              // This regex will match because of the checks at the beginning of the function
          }
          // Detect if on extended storage and make pkg_path
          std::filesystem::path ext_path("/mnt/ext0");
          ext_path += pkg_directory_path;

          if (std::filesystem::exists(ext_path) && std::filesystem::is_regular_file(ext_path)) {
              log_info("found ext hdd");
              pkg_path = ext_path;  
          }
          else {

              pkg_path = pkg_directory_path;
          }
      }
  }
  catch (std::filesystem::filesystem_error& e)
  {
      log_error("SFO Opts failed");
  }

ProgUpdate(8, "%s\n\n%s %s\n%s %s\n\n %s ...", getDumperLangSTR(DUMP_INFO), getDumperLangSTR(APP_NAME),title.c_str(), getDumperLangSTR(TITLE_ID), title_id.c_str(), getDumperLangSTR(DUMPER_SC0));
try {
    if (std::filesystem::exists(pkg_path)) {
        if (!pkg::extract_sc0(pkg_path, sce_sys_path, title_id, title))
        {
            log_error("pkg::extract_sc0(\"%s\",\"%s\") failed", pkg_path.c_str(), sce_sys_path.c_str());
            return false;
        }
    }
    else {
        log_error("Unable to open %s", pkg_path.string().c_str());
        return false;
    }
}
catch (std::exception e)
{
    log_error("extracting sc0 failed");
    return false;
}

ProgUpdate(10, "%s\n\n%s %s\n%s %s\n\n %s ...", getDumperLangSTR(DUMP_INFO), getDumperLangSTR(APP_NAME),title.c_str(), getDumperLangSTR(TITLE_ID), title_id.c_str(), getDumperLangSTR(DUMPING_TROPHIES));

try {
    // Trophy "decryption"
    std::filesystem::path npbind_file(sce_sys_path);
    npbind_file /= "npbind.dat";
    if (std::filesystem::is_regular_file(npbind_file)) {
        log_info("npbind_file: %s\n", npbind_file.string().c_str());
        std::vector<npbind::NpBindEntry> npbind_entries = npbind::read(npbind_file); // Flawfinder: ignore
        for (auto&& entry : npbind_entries) {
            std::filesystem::path src("/user/trophy/conf");
            src /= std::string(entry.npcommid.data);
            src /= "TROPHY.TRP";

            std::filesystem::path dst(sce_sys_path);
            dst /= "trophy";
            // make folder if it doesnt exist OR ignore the error if it does exist
            mkdir(dst.string().c_str(), 0777);
            //BUG FIX above
            uint32_t zerofill = 0;
            if (std::strlen(entry.trophy_number.data) < 2) {        // Flawfinder: ignore
                zerofill = 2 - std::strlen(entry.trophy_number.data); // Flawfinder: ignore
            }
            dst /= "trophy" + std::string(zerofill, '0') + std::string(entry.trophy_number.data) + ".trp";

            ProgUpdate(11, "%s\n\n%s %s\n%s %s\n\n %s %s...", getDumperLangSTR(DUMP_INFO), getDumperLangSTR(APP_NAME), title.c_str(), getDumperLangSTR(TITLE_ID), title_id.c_str(), getDumperLangSTR(DUMPING_TROPHIES), src.c_str());
            if (std::filesystem::exists(src)) {
                log_debug("[%s] Dumping Trophy %s to %s ...", title_id.c_str(), src.string().c_str(), dst.string().c_str());
                if (!std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing)) {
                    log_error("Unable to copy %s", src.string().c_str());
                }
                else
                    log_info("Successfully Copied trophy");
            }
            else {
                log_error("Unable to open %s", src.string().c_str());
                return false;
            }
        }
    }
}
catch (std::filesystem::filesystem_error& e)
{
        log_error("Dumping Trophy failed");
        return false;
}

    if(std::filesystem::exists("/mnt/usb0/trophy_flag"))
        return true;

    // UnPFS
    std::filesystem::path pfs_path("/mnt/sandbox/pfsmnt");
    pfs_path /= title_id;
    if (opt == BASE_GAME || opt == REMASTER) {
      pfs_path += "-app0-nest";
    } else if (opt == GAME_PATCH) {
      pfs_path += "-patch0-nest";
    } else if (opt == THEME || opt == ADDITIONAL_CONTENT_DATA) {
      pfs_path += "-ac-nest";
    }
    pfs_path /= "pfs_image.dat";
    try {
        if (std::filesystem::exists(pfs_path)) {
            if (!pfs::extract(pfs_path.string(), output_path.string(), title_id.c_str(), (char*)title.c_str())) {
                return false;
            }
        }
        else {
            log_error("Unable to open %s", pfs_path.string().c_str());
            return false;
        }
    }
    catch (std::exception e)
    {
        log_error("Extracting PFS failed");
        return false;
    }



  // Vector of strings for locations of SELF files for decryption
  std::vector<std::string> self_files;

  // Generate GP4

  std::filesystem::path sfo_path(sce_sys_path);
  sfo_path /= "param.sfo";

  std::filesystem::path gp4_path(output_path);
  gp4_path /= output_directory + ".gp4";


ProgUpdate(98, "%s\n\n%s %s\n%s %s\n\n %s ...", getDumperLangSTR(DUMP_INFO), getDumperLangSTR(APP_NAME), title.c_str(), getDumperLangSTR(TITLE_ID), title_id.c_str(), getDumperLangSTR(CREATING_GP4));
try {
    if (!gp4::generate(sfo_path.string(), output_path.string(), gp4_path.string(), self_files, opt))
        return false;
}
catch (const std::exception& e)
{
    log_error("Gerneating GP4 failed");
}


  // Decrypt ELF files and make into FSELFs
std::string encrypted_path("/mnt/sandbox/pfsmnt/");
if (opt == BASE_GAME) {
    encrypted_path += title_id + "-app0/";
}
else if (opt == GAME_PATCH) {
    encrypted_path += title_id + "-patch0/";
}
else if (opt == THEME_UNLOCK) {
    encrypted_path += title_id + "-ac/";
}

log_info("Step %i", __LINE__);
std::string decrypted_path(output_path);

ProgUpdate(99, "%s\n\n%s %s\n%s %s\n\n %s %s...", getDumperLangSTR(DUMP_INFO), getDumperLangSTR(APP_NAME), title.c_str(), getDumperLangSTR(TITLE_ID), title_id.c_str(), getDumperLangSTR(DEC_BIN), encrypted_path.c_str());
log_debug("Decrypting Bins in %s ...", encrypted_path.c_str());

elf::decrypt_dir(encrypted_path, decrypted_path);

 // Delete .dumping semaphore
if (!std::filesystem::remove(dumping_semaphore)) 
    log_error("Unable to delete dumping semaphore");

// Create .complete semaphore
std::ofstream complete_sem_touch(complete_semaphore);
complete_sem_touch.close();
if (std::filesystem::exists(complete_semaphore)) 
    log_error("Unable to create completion semaphore!");


  return true;
}
} // namespace dump