// Copyright (c) 2021 Al Azif
// License: GPLv3

#include "pkg.hpp"
#include "common.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include "log.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory>
#include <user_mem.h>

namespace pkg {
std::string get_entry_name_by_type(uint32_t type) {
  std::stringstream ss;

  if (type == 0x0001) {
    ss << ".digests";
  } else if (type == 0x0010) {
    ss << ".entry_keys";
  } else if (type == 0x0020) {
    ss << ".image_key";
  } else if (type == 0x0021) {
    ss << ".unknown_0x21";
  } else if (type == 0x0080) {
    ss << ".general_digests";
  } else if (type == 0x00C0) {
    ss << ".unknown_0xC0";
  } else if (type == 0x0100) {
    ss << ".metas";
  } else if (type == 0x0200) {
    ss << ".entry_names";
  } else if (type == 0x0400) {
    ss << "license.dat";
  } else if (type == 0x0401) {
    ss << "license.info";
  } else if (type == 0x0402) {
    ss << "nptitle.dat";
  } else if (type == 0x0403) {
    ss << "npbind.dat";
  } else if (type == 0x0404) {
    ss << "selfinfo.dat";
  } else if (type == 0x0406) {
    ss << "imageinfo.dat";
  } else if (type == 0x0407) {
    ss << "target-deltainfo.dat";
  } else if (type == 0x0408) {
    ss << "origin-deltainfo.dat";
  } else if (type == 0x0409) {
    ss << "psreserved.dat";
  } else if (type == 0x1000) {
    ss << "param.sfo";
  } else if (type == 0x1001) {
    ss << "playgo-chunk.dat";
  } else if (type == 0x1002) {
    ss << "playgo-chunk.sha";
  } else if (type == 0x1003) {
    ss << "playgo-manifest.xml";
  } else if (type == 0x1004) {
    ss << "pronunciation.xml";
  } else if (type == 0x1005) {
    ss << "pronunciation.sig";
  } else if (type == 0x1006) {
    ss << "pic1.png";
  } else if (type == 0x1007) {
    ss << "pubtoolinfo.dat";
  } else if (type == 0x1008) {
    ss << "app/playgo-chunk.dat";
  } else if (type == 0x1009) {
    ss << "app/playgo-chunk.sha";
  } else if (type == 0x100A) {
    ss << "app/playgo-manifest.xml";
  } else if (type == 0x100B) {
    ss << "shareparam.json";
  } else if (type == 0x100C) {
    ss << "shareoverlayimage.png";
  } else if (type == 0x100D) {
    ss << "save_data.png";
  } else if (type == 0x100E) {
    ss << "shareprivacyguardimage.png";
  } else if (type == 0x1200) {
    ss << "icon0.png";
  } else if ((type >= 0x1201) && (type <= 0x121F)) {
    ss << "icon0_" << std::setfill('0') << std::setw(2) << type - 0x1201 << ".png";
  } else if (type == 0x1220) {
    ss << "pic0.png";
  } else if (type == 0x1240) {
    ss << "snd0.at9";
  } else if ((type >= 0x1241) && (type <= 0x125F)) {
    ss << "pic1_" << std::setfill('0') << std::setw(2) << type - 0x1241 << ".png";
  } else if (type == 0x1260) {
    ss << "changeinfo/changeinfo.xml";
  } else if ((type >= 0x1261) && (type <= 0x127F)) {
    ss << "changeinfo/changeinfo_" << std::setfill('0') << std::setw(2) << type - 0x1261 << ".xml";
  } else if (type == 0x1280) {
    ss << "icon0.dds";
  } else if ((type >= 0x1281) && (type <= 0x129F)) {
    ss << "icon0_" << std::setfill('0') << std::setw(2) << type - 0x1281 << ".dds";
  } else if (type == 0x12A0) {
    ss << "pic0.dds";
  } else if (type == 0x12C0) {
    ss << "pic1.dds";
  } else if ((type >= 0x12C1) && (type <= 0x12DF)) {
    ss << "pic1_" << std::setfill('0') << std::setw(2) << type - 0x12C1 << ".dds";
  } else if ((type >= 0x1400) && (type <= 0x147F)) {
    ss << "trophy/trophy" << std::setfill('0') << std::setw(2) << type - 0x1400 << ".trp";
  } else if ((type >= 0x1600) && (type <= 0x1609)) {
    ss << "keymap_rp/" << std::setfill('0') << std::setw(3) << type - 0x15FF << ".png";
  } else if ((type >= 0x1610) && (type <= 0x16F5)) {
    ss << "keymap_rp/" << std::setfill('0') << std::setw(2) << (type - 0x1610) / 10 << "/" << std::setfill('0') << std::setw(3) << (((type - 0x160F) % 10) ? (type - 0x160F) % 10 : 10) << ".png";
  }

  return ss.str();
}

bool extract_sc0(const std::string &pkg_path, const std::string &output_path, const std::string& tid, const std::string& title) {
  // Check for empty or pure whitespace path
  if (pkg_path.empty() || std::all_of(pkg_path.begin(), pkg_path.end(), [](char c) { return std::isspace(c); })) {
    log_error("Empty input path argument!");
    return false;
  }

  // Check if file exists and is file
  if (!std::filesystem::is_regular_file(pkg_path)) {
    log_error("Input path does not exist or is not a file!");
    return false;
  }

  // Open path
  std::ifstream pkg_input(pkg_path, std::ios::in | std::ios::binary);
  if (!pkg_input || !pkg_input.good()) {
    pkg_input.close();
    log_error("Cannot open input file: %s" ,pkg_path.c_str());
    return false;
  }

  // Check file magic (Read in whole header)
  PkgHeader header;
  pkg_input.read((char *)&header, sizeof(header)); // Flawfinder: ignore
  if (!pkg_input.good()) {
    pkg_input.close();
    log_error("Error reading PKG header!");
    return false;
  }
  if (__builtin_bswap32(header.magic) != PKG_MAGIC) {
    pkg_input.close();
    log_error("Input path is not a PKG!");
    return false;
  }

  

  // Read PKG entry table entries
  std::vector<PkgTableEntry> entries;
  pkg_input.seekg(__builtin_bswap32(header.entry_table_offset), pkg_input.beg);
  for (uint32_t i = 0; i < __builtin_bswap32(header.entry_count); i++) {
    PkgTableEntry temp_entry;
    pkg_input.read((char *)&temp_entry, sizeof(temp_entry)); // Flawfinder: ignore
    if (!pkg_input.good()) {
      pkg_input.close();
      log_error("Error reading entry table!");
      return false;
    }
    entries.push_back(temp_entry);
  }

  
  // Check for empty or pure whitespace path
  if (output_path.empty() || std::all_of(output_path.begin(), output_path.end(), [](char c) { return std::isspace(c); })) {
    log_error("Empty output path argument!");
    return false;
  }

  // Make sure output directory path exists or can be created
  if (!std::filesystem::is_directory(output_path) && !std::filesystem::create_directories(output_path)) {
    pkg_input.close();
    log_error("Unable to open/create output directory");
    return false;
  }


  // Extract sc0 entries
  for (auto &&entry : entries) {
    std::string entry_name = get_entry_name_by_type(__builtin_bswap32(entry.id));
    if (!entry_name.empty()){//is_PKG_entry_needed(entry_name)) {
      bool entry_encrpyted((__builtin_bswap32(entry.flags1) & 0x80000000) != 0);
      uint32_t entry_key_index = (__builtin_bswap32(entry.flags2) & 0xF000) >> 12;
      std::filesystem::path temp_output_path(output_path);
      temp_output_path /= entry_name;

      pkg_input.seekg(__builtin_bswap32(entry.offset), pkg_input.beg);
      std::unique_ptr<char[]> buf = std::make_unique<char[]>(__builtin_bswap32(entry.size));
      pkg_input.read(buf.get(), __builtin_bswap32(entry.size)); // Flawfinder: ignore
      if (!pkg_input.good()) {
          pkg_input.close();
          log_error("Error reading entry data!");
          return false;
      }

      std::string temp_output_dir = temp_output_path;

      const size_t last_slash_idx = temp_output_dir.rfind('/');
      if (std::string::npos != last_slash_idx)
      {
          temp_output_dir = temp_output_dir.substr(0, last_slash_idx);
      }


      if (!std::filesystem::is_directory(temp_output_dir) && mkdir(temp_output_dir.c_str(), 0777) == 0) {
       // pkg_input.close();
        log_error("Unable to open/create output subdirectory");
      }

      if (entry_encrpyted) {
        // Only have key at index 3
        if (entry_key_index == 3) {
          // TODO: Decrypt and save as `temp_output_path`
        }
        temp_output_path += ".encrypted";
      }


      // Write to file
      //unlink anyways idc
      unlink(temp_output_path.c_str());
      int fd = open(temp_output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0777);
      if (fd >= 0) {

          write(fd, buf.get(), __builtin_bswap32(entry.size));
          close(fd);
      }
      else {

          log_error("Write to file failed\n");
          pkg_input.close();
          return false;
      }

    }
  }
  pkg_input.close();

  std::string npbind_out = output_path + "/npbind.dat";
  std::string nptitle_out = output_path + "/nptitle.dat";
  std::string npbind_in = "/system_data/priv/appmeta/" + tid + "/npbind.dat";
  std::string nptitle_in = "/system_data/priv/appmeta/" + tid + "/nptitle.dat";

  if (std::filesystem::is_regular_file(npbind_in)) {
      if (!std::filesystem::copy_file(npbind_in, npbind_out, std::filesystem::copy_options::overwrite_existing)) {
          log_error("Unable to copy %s to %s", npbind_out.c_str(), npbind_in.c_str());
      }
      else
          log_info("Copied npbind succssfully");
  }
  if (std::filesystem::is_regular_file(nptitle_in)) {
      if (!std::filesystem::copy_file(nptitle_in, nptitle_out, std::filesystem::copy_options::overwrite_existing)) {
          log_error("Unable to copy %s to %s", nptitle_out.c_str(), nptitle_in.c_str());
      }
      else
          log_info("Copied nptitle succssfully");
  }



  log_error("UNPKG ENDED %i\n", __LINE__);

  return true;
}
} // namespace pkg
