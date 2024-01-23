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
#include <regex>
#include <string>
#include "lang.h"
#include "fself.hpp"

namespace dump {

bool DecryptAndMakeGP4(const std::string &output_path, const std::string &title_id, Dump_Options opt, const std::string& title, const std::string& DLC_Content_id = ""){

  log_info("DecryptAndMakeGP4(%s, %s, %i, %s, %s)", output_path.c_str(), title_id.c_str(), opt, title.c_str(), DLC_Content_id.c_str());
  std::filesystem::path sce_sys_path(output_path);
  sce_sys_path /= "sce_sys";
  // Generate GP4
  std::filesystem::path sfo_path(sce_sys_path);
  sfo_path /= "param.sfo";

  std::filesystem::path gp4_path(output_path);
  gp4_path /= (opt == ADDITIONAL_CONTENT_DATA) ? DLC_Content_id + ".gp4" : title_id + ".gp4";

  std::filesystem::path sb_path("/mnt/sandbox/pfsmnt/");
   if (opt == BASE_GAME ||  opt == REMASTER) {
         sb_path += title_id + "-app0/";
      }
      else if (opt == GAME_PATCH) {
         sb_path += title_id + "-patch0/";
      }
      else if (opt == THEME_UNLOCK || opt == ADDITIONAL_CONTENT_DATA || opt == THEME || opt == ADDITIONAL_CONTENT_NO_DATA) {
         sb_path += title_id + "-ac/";
     }
     else {
         log_error("Invalid OPT");
         return false;
     }


   ProgUpdate(98,  getDumperLangSTR(DUMP_INFO) + "\n\n" + getDumperLangSTR(APP_NAME) + ": " + title + "\n" + getDumperLangSTR(TITLE_ID) + " " + title_id + "\n\n" + getDumperLangSTR(DEC_BIN) + " " + sb_path.string());  
   log_debug("Decrypting Bins in %s ...", sb_path.string().c_str());
   
    bool fself = false;
    if(fself::is_fself(sb_path.string() + "/eboot.bin")){
        fself = true;
        log_info("eboot is an FSELF");
    }

    elf::decrypt_dir(sb_path.string(), output_path, fself);

  
ProgUpdate(99,  getDumperLangSTR(DUMP_INFO) + "\n\n" + getDumperLangSTR(APP_NAME) + ": " + title + "\n" + getDumperLangSTR(TITLE_ID) + " " + title_id + "\n\n" + getDumperLangSTR(CREATING_GP4));  
try {
    std::vector<std::string> self_files;
    if (!gp4::generate(sfo_path.string(), output_path, gp4_path.string(), self_files, opt))
        return false;
}
catch (const std::exception& e)
{
    log_error("Gerneating GP4 failed");
    return false;
}

return true;
}
    
bool dump_dlc(const std::string & usb_device, const std::string & title_id) {

  // Vector of strings for locations of SELF files for decryption
  std::vector<std::string> self_files;
  bool ac_wo_data = false;
  std::string full_content_id;

  std::vector <std::tuple<std::string, std::string, std::string>> dlc_info = query_dlc_database(title_id);
  std::string addcont_path_str;
  std::string user_addcont_dir = "/user/addcont/" + title_id;
  std::string ext_addcont_dir = "/mnt/ext0/user/addcont/" + title_id;
  std::string disc_addcont_dir = "/mnt/disc/addcont/" + title_id;

  if(std::filesystem::exists(user_addcont_dir)){
        addcont_path_str = user_addcont_dir;
  }
  else if(std::filesystem::exists(ext_addcont_dir)){
        addcont_path_str = ext_addcont_dir;
  }
  else {
        addcont_path_str = disc_addcont_dir;
  }

  for (const auto & result: dlc_info) {
    std::filesystem::path pkg_path(addcont_path_str + "/" + std::get<0>(result));
    std::string title = std::get<1>(result);
    pkg_path /= "ac.pkg";   
    
    if (!std::filesystem::exists(pkg_path) || !std::filesystem::is_regular_file(pkg_path)) {
      log_error("Unable to find `pkg_path`");
      return false;
    }

    ac_wo_data = std::filesystem::file_size(pkg_path) < MB(5);
     
    std::string out_path = usb_device + "/" + title_id + "-DLC-" + std::get<0>(result);
    log_info("output directory %s", out_path.c_str());
    if (!std::filesystem::is_directory(out_path) && !std::filesystem::create_directories(out_path)) {
      log_error("Unable to create `out_path` directory");
      return false;
    }
  
      if(!ac_wo_data){
         // Construct path to pfs_image.dat file
         std::filesystem::path pfs_path("/mnt/sandbox/pfsmnt/");
         std::filesystem::path sb_path("/mnt/sandbox/pfsmnt/");
         full_content_id = std::get<2>(result);
         pfs_path /= full_content_id + "-ac-nest"; // Use stem to get filename without extension
         sb_path /= full_content_id + "-ac"; // Use stem to get filename without extension
         pfs_path /= "pfs_image.dat";

         if (std::filesystem::exists(pfs_path) && std::filesystem::is_regular_file(pfs_path)) {
            log_info("Found pfs at %s", pfs_path.string().c_str());
            if (!pfs::extract(pfs_path.string(), out_path, title_id, title)) {
                log_error("Failed to extract pfs");
                return false;
            }
          }
      }

      // Found pkg path
      log_info("Found pkg at %s", pkg_path.string().c_str());
      ProgUpdate(8, getDumperLangSTR(DUMP_INFO) + "\n\n" + getDumperLangSTR(DLC_NAME) + ": " + title + "\n" + getDumperLangSTR(TITLE_ID) + " " + title_id + "\n\n" + getDumperLangSTR(DUMPER_SC0));
      std::filesystem::path sce_sys_path(out_path);
      sce_sys_path /= "sce_sys";
      if (!std::filesystem::is_directory(sce_sys_path.string()) && !std::filesystem::create_directories(sce_sys_path.string())) {
        log_error("Unable to create `sce_sys` directory");
        return false;
      }
      if (!pkg::extract_sc0(pkg_path, sce_sys_path.string(), title_id, title)) {
        log_error("pkg::extract_sc0(\"%s\",\"%s\") failed", pkg_path.c_str(), sce_sys_path.string().c_str());
        return false;
      }
      
      std::filesystem::path gp4_path(out_path);
      gp4_path /= full_content_id + "-DLC.gp4";
      // Generate GP4
      std::filesystem::path sfo_path(sce_sys_path);
      sfo_path /= "param.sfo";

     
     if(!DecryptAndMakeGP4(out_path, title_id, ADDITIONAL_CONTENT_DATA, title, full_content_id)){
       log_error("Decrypting and making GP4 failed");
       return false;
     }
  }

  return true;
}

bool __dump(const std::string &usb_device, const std::string &title_id, Dump_Options opt, const std::string& title) {

  if (opt > TOTAL_OF_OPTS)
  {
      log_error("OPT is out of range: %i", opt);
      return false;
  }


  std::string output_directory = title_id;

   // Check for .dumping semaphore
  std::filesystem::path dumping_semaphore(usb_device);
  std::filesystem::path complete_semaphore(usb_device);
  std::ofstream dumping_sem_touch(dumping_semaphore);
  
  if (opt == GAME_PATCH) {
    output_directory += "-patch"; // Add "-patch" to "patch" types so it doesn't overlap with "base"
  } 
  else if (opt == THEME_UNLOCK) {
    output_directory += "-unlock"; // Add "-unlock" to theme type so we can differentiate between "install" and "unlock" PKGs
  }

  
  dumping_semaphore /= output_directory + ".dumping";
  if (std::filesystem::exists(dumping_semaphore)) {
    log_error("This dump is currently dumping or closed unexpectedly! Please delete existing dump to enable dumping.");
  }

  // Check for .complete semaphore
  complete_semaphore /= output_directory + ".complete";
  if (std::filesystem::exists(complete_semaphore)) {
    log_error("This dump has already been completed! Please delete existing dump to enable dumping.");
  }


  // Create .dumping semaphore
  dumping_sem_touch.close();
  if (std::filesystem::exists(dumping_semaphore)) {
    log_error("Unable to create dumping semaphore!");
  }

  // Create base path
  std::filesystem::path output_path(usb_device);
  output_path /= output_directory;

  // Make sure the base path is a directory or can be created
  if (!std::filesystem::is_directory(output_path) && !std::filesystem::create_directories(output_path)) {
    log_error("Unable to create output directory");
    return false;
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
      return false;
  }

ProgUpdate(8,  getDumperLangSTR(DUMP_INFO) + "\n\n" + getDumperLangSTR(APP_NAME) + ": " + title + "\n" + getDumperLangSTR(TITLE_ID) + " " + title_id + "\n\n" + getDumperLangSTR(DUMPER_SC0));  
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

ProgUpdate(10,  getDumperLangSTR(DUMP_INFO) + "\n\n" + getDumperLangSTR(APP_NAME) + ": " + title + "\n" + getDumperLangSTR(TITLE_ID) + " " + title_id + "\n\n" + getDumperLangSTR(DUMPING_TROPHIES));;

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

                ProgUpdate(11,  getDumperLangSTR(DUMP_INFO) + "\n\n" + getDumperLangSTR(APP_NAME) + ": " + title + "\n" + getDumperLangSTR(TITLE_ID) + " " + title_id + "\n\n" + getDumperLangSTR(DUMPING_TROPHIES) + " " + src.string());
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
            if (!pfs::extract(pfs_path.string(), output_path.string(), title_id, title)) {
                log_error("Failed to extract pfs");
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

if(!DecryptAndMakeGP4(output_path.string(), title_id, opt, title)){
    log_error("Decrypting and making GP4 failed");
    return false;
}

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