// Copyright (c) 2021 Al Azif
// License: GPLv3

#include "common.hpp"
#include "elf.hpp" // `bool is_self(const std::string &path);`
#include "sfo.hpp"
#include "dump.hpp"
#include "pugixml.hpp"

#include <algorithm>
#include <cstring>
// #include <ctime> // TODO: Not including doesn't generate and error even though "strftime" is used?
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include "log.h"
#include <user_mem.h>
#include <time.h>
#include <string.h>
#include <sha256.h>
#include <crc32.h>
#include <sha1.h>
#include <md5.h>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>

namespace gp4 {

void recursive_directory(const std::string &path, pugi::xml_node &node) {
  // Check for empty or pure whitespace path
  if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
    log_error("Empty path argument!");
    return;
  }

  for (auto &&p : std::filesystem::directory_iterator(path)) {
    if (std::filesystem::is_directory(p.path())) {

      //check for sce_sys/about
        log_info("folder %s", std::string(p.path()).c_str());
      if (strstr(std::string(p.path()).c_str(), "sce_sys/about") != NULL) {
          log_info("continuing...");
            continue;
      }

      pugi::xml_node dir_node = node.append_child("dir");

      std::string temp_path = p.path();
      temp_path.erase(0, path.size());
      while (temp_path.back() == '/') {
        temp_path.pop_back();
      }
      while (temp_path.at(0) == '/') {
        temp_path.erase(0, 1);
      }

      dir_node.append_attribute("targ_name") = temp_path.c_str();


      recursive_directory(p.path(), dir_node);
    }
  }
}
pugi::xml_document make_volume(const std::string& content_id, const std::string& volume_type, std::string c_date, std::string c_time) {
    // Check input

    if (c_date.empty()) {
        log_fatal("Malformed c_date");
    }

    if (c_time.empty()) {
        log_fatal("Malformed c_time");
    }

    if (volume_type != "pkg_ps4_app" && volume_type != "pkg_ps4_patch" && volume_type != "pkg_ps4_remaster" && volume_type != "pkg_ps4_theme" && volume_type != "additional-content-data" && volume_type != "additional-content-no-data") {
        log_fatal("Unknown volume type");
    }

    // Generate XML
    pugi::xml_document doc;

    pugi::xml_node volume_node = doc.append_child("volume");
    pugi::xml_node volume_type_node = volume_node.append_child("volume_type");
    volume_type_node.append_child(pugi::node_pcdata).set_value(volume_type.c_str());
    pugi::xml_node volume_ts_node = volume_node.append_child("volume_ts");

    // Get current time for volume_ts... and possibly c_date
    std::time_t t = std::time(0);
    std::tm* timeinfo = std::localtime(&t);

    char buffer[20];
    std::memset(buffer, '\0', sizeof(buffer));
    std::strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", timeinfo);
    volume_ts_node.append_child(pugi::node_pcdata).set_value(buffer);

    pugi::xml_node package_node = volume_node.append_child("package");
    package_node.append_attribute("content_id") = content_id.c_str();
    package_node.append_attribute("passcode") = "00000000000000000000000000000000";
    if (volume_type == "additional-content-data" || volume_type == "additional-content-no-data") {
        // TODO: Attempt to get entitlement_key based on content_id
        package_node.append_attribute("entitlement_key") = "";
    }
    if(volume_type == "pkg_ps4_patch")
       package_node.append_attribute("storage_type") = "digital25";
    else
       package_node.append_attribute("storage_type") = "digital50";

    package_node.append_attribute("app_type") = "full";
log_info("---------");
    // Set c_date
    std::string new_time = std::string("actual_datetime");
    if (!c_date.empty()) {
        new_time = c_date;
        if (!c_time.empty()) {
            // TODO: Append time to new_time
            log_info("---------");
        }
    }
    package_node.append_attribute("c_date") = new_time.c_str();
log_info("---------");
    return doc;
}

pugi::xml_document make_playgo(const std::string& playgo_xml) {
    pugi::xml_document doc;

    // TODO: pugixml kills attributes in the <xml> node itself... get them back/add to default build

    // Return external XML if it exists
    if (std::filesystem::exists(playgo_xml) && std::filesystem::is_regular_file(playgo_xml) && doc.load_file(playgo_xml.c_str())) {
        return doc;
    }

    // Build default
    pugi::xml_node psproject_node = doc.append_child("psproject");
    psproject_node.append_attribute("fmt") = "playgo-manifest";
    psproject_node.append_attribute("version") = "1000"; // TODO: Will this be correct?

    pugi::xml_node volume_node = psproject_node.append_child("volume");

    pugi::xml_node chunk_info_node = volume_node.append_child("chunk_info");
    chunk_info_node.append_attribute("chunk_count") = "1";
    chunk_info_node.append_attribute("scenario_count") = "1";

    pugi::xml_node chunks_node = chunk_info_node.append_child("chunks");

    pugi::xml_node chunk_node = chunks_node.append_child("chunk");
    chunk_node.append_attribute("id") = "0";
    chunk_node.append_attribute("label") = "Chunk #0";

    pugi::xml_node scenarios_node = chunk_info_node.append_child("scenarios");
    scenarios_node.append_attribute("default_id") = "0";

    pugi::xml_node scenario_node = scenarios_node.append_child("scenario");
    scenario_node.append_attribute("id") = "0";
    scenario_node.append_attribute("type") = "sp";
    scenario_node.append_attribute("initial_chunk_count") = "1";
    scenario_node.append_attribute("label") = "Scenario #0";

    return doc;
}

pugi::xml_document make_files(const std::string &path, bool validation) {
  // Check for empty or pure whitespace path
  if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
    log_error("Empty path argument!");
    return pugi::xml_document();
  }

  // Generate XML
  pugi::xml_document doc;
  pugi::xml_node files_node = doc.append_child("files");
  files_node.append_attribute("img_no") = "0"; // TODO: PlayGo
  // Files to skip when making GP4, but not the GP4FV
  std::vector<std::string> skip_files = {};
  if (!validation) {
    skip_files = {
        "sce_sys/about/right.sprx", // Cannot be included for official PKG tools
        "sce_sys/about/",
        "sce_sys/.digests",
        "sce_sys/.entry_keys",
        "sce_sys/.image_key",
        "sce_sys/.unknown_0x21",
        "sce_sys/.general_digests",
        "sce_sys/.unknown_0xC0",
        "sce_sys/.metas",
        "sce_sys/.entry_names",
        "sce_sys/license.dat",
        "sce_sys/license.info",
        "sce_sys/selfinfo.dat",         // ?
        "sce_sys/imageinfo.dat",        // ?
        "sce_sys/target-deltainfo.dat", // ?
        "sce_sys/origin-deltainfo.dat",
        "sce_sys/psreserved.dat",
        "sce_sys/playgo-chunk.dat",
        "sce_sys/playgo-chunk.sha",
        "sce_sys/playgo-manifest.xml",
        "sce_sys/pubtoolinfo.dat", // ?
        "sce_sys/app/playgo-chunk.dat",
        "sce_sys/app/playgo-chunk.sha",
        "sce_sys/app/playgo-manifest.xml",
        "sce_sys/icon0.dds",
        "sce_sys/pic0.dds",
        "sce_sys/pic1.dds"
    };
    for (auto &&f : skip_files) {
      std::stringstream ss_encrypted;
      ss_encrypted << f << ".encrypted";
      skip_files.push_back(ss_encrypted.str());
    }
    for (uint8_t i = 0; i < 31; i++) {
      std::stringstream ss_image;
      ss_image << "sce_sys/icon0_" << std::dec << std::setfill('0') << std::setw(2) << i << ".dds";
      skip_files.push_back(ss_image.str());
      ss_image.str(std::string());
      ss_image << "sce_sys/pic1_" << std::dec << std::setfill('0') << std::setw(2) << i << ".dds";
      skip_files.push_back(ss_image.str());
    }
  }
  
  for (auto &&p : std::filesystem::recursive_directory_iterator(path)) {
    // Skip files/directories contained in the `skip_files` vector
    if (std::count(skip_files.begin(), skip_files.end(), customRelative(p.path(), path))) {
        continue;
    }
    else if(customRelative(p.path(), path).string().rfind(".fself") != std::string::npos){
            continue;
    }
    else if (std::filesystem::is_regular_file(p.path())) {
      std::filesystem::path targ_path = customRelative(p.path(), path);
      std::filesystem::path orig_path = customRelative(p.path(), path);
      // TODO:
      //   - Add proper PlayGo options
      //   - Add PFS Compression option
      pugi::xml_node file_node = files_node.append_child("file");
      file_node.append_attribute("targ_path") = targ_path.c_str();

     // std::string orig_path_mod = orig_path.c_str();
      //std::replace(orig_path_mod.begin(), orig_path_mod.end(), '/', '\\');
      file_node.append_attribute("orig_path") = orig_path.c_str();
      if (validation) {
        // Get filesize, crc32, md5, and sha1
        file_node.append_attribute("size") = customFileSize(p.path()).c_str();
        CRC32 crc32;
        MD5 md5;
        SHA1 sha1;


        std::ifstream hash_data(p.path(), std::ios::in | std::ios::binary);
        if (!hash_data || !hash_data.good()) {
          hash_data.close();
          log_error("Cannot open file: %s", std::string(p.path()).c_str());
          return doc;
        }

        while (hash_data.good()) {
          std::vector<unsigned char> buffer(PAGE_SIZE);
          hash_data.read(reinterpret_cast<char *>(&buffer[0]), buffer.size()); // Flawfinder: ignore
          crc32.add(&buffer[0], hash_data.gcount());
          md5.add(&buffer[0], hash_data.gcount());
          sha1.add(&buffer[0], hash_data.gcount());
        }
        hash_data.close();
        file_node.append_attribute("crc") = &crc32.getHash()[0];
        file_node.append_attribute("md5") = &md5.getHash()[0];
        file_node.append_attribute("sha1") = &sha1.getHash()[0];

      }
    }
  }

  return doc;
}

pugi::xml_document make_directories(const std::string& path) {
    // Generate XML
    pugi::xml_document doc;

    pugi::xml_node rootdir_node = doc.append_child("rootdir");

    recursive_directory(path, rootdir_node);

    return doc;
}

pugi::xml_document assemble(const pugi::xml_document& volume, const pugi::xml_document& playgo, const pugi::xml_document& files, const pugi::xml_document& directories, const std::string& custom_version = "") {
    // Generate XML
    pugi::xml_document doc;

    // TODO: Set XML node attributes

    pugi::xml_node psproject_node = doc.append_child("psproject");
    psproject_node.append_attribute("fmt") = "gp4";
    if (!custom_version.empty()) {
        psproject_node.append_attribute("version") = custom_version.c_str();
    }
    else {
        if (playgo.child("psproject").attribute("version").empty()) {
            psproject_node.append_attribute("version") = "1000"; // TODO: Will this be correct?
        }
        else {
            psproject_node.append_attribute("version") = playgo.child("psproject").attribute("version").value(); // TODO: Will this be correct?
        }
    }

    psproject_node.append_copy(volume.child("volume"));
    psproject_node.child("volume").append_copy(playgo.child("psproject").child("volume").child("chunk_info"));

    psproject_node.child("volume").child("chunk_info").prepend_child("chunks");
    uint64_t chunk_count = 1;
    if (!psproject_node.child("volume").child("chunk_info").attribute("chunk_count").empty()) {
        chunk_count = std::strtoull(psproject_node.child("volume").child("chunk_info").attribute("chunk_count").value(), NULL, 10);
    }

    for (uint64_t i = 0; i < chunk_count; i++) {
        pugi::xml_node chunk_node = psproject_node.child("volume").child("chunk_info").child("chunks").append_child("chunk");
        std::stringstream ss_id;
        std::stringstream ss_label;

        ss_id << "" << std::dec << i;
        ss_label << "Chunk #" << std::dec << i; // TODO: The labels are not what they actually are, need to see if they are stored in other playgo files

        chunk_node.append_attribute("id") = ss_id.str().c_str();
        chunk_node.append_attribute("label") = ss_label.str().c_str();
    }

    // Remove `initial_chunk_count_disc` from each `scenario` if it exists... we aren't a disc anymore
    for (pugi::xml_node scenarios : psproject_node.child("volume").child("chunk_info")) {
        for (pugi::xml_node scenario : scenarios.children("scenario")) {
            scenario.remove_attribute("initial_chunk_count_disc");
        }
    }

    psproject_node.append_copy(files.child("files"));
    psproject_node.append_copy(directories.child("rootdir"));
    return doc;
}

void write(const pugi::xml_document& xml, const std::string& path) {
    // Check for empty or pure whitespace path
    if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
        log_fatal("Empty output path argument!");
    }

    std::filesystem::path output_path(path);

    // Exists, but is not a file
    if (std::filesystem::exists(output_path) && !std::filesystem::is_regular_file(output_path)) {
        log_fatal("Output object exists but is not a file!");
    }

    // Open path
    std::ofstream output_file(output_path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!output_file || !output_file.good()) {
        output_file.close();
        log_fatal("Cannot open file: %s" , std::string(output_path).c_str());
    }

    xml.save(output_file);

    output_file.close();
}

void generate(const std::string& sfo_path, const std::string& output_path, const std::string& gp4_path, std::vector<std::string>& self_files, Dump_Options opt) {
    char buff[100];

    std::vector<sfo::SfoData> sfo_data = sfo::read(sfo_path); // Flawfinder: ignore
    std::vector<std::string> sfo_keys = sfo::get_keys(sfo_data);

    if (!std::count(sfo_keys.begin(), sfo_keys.end(), std::string("CONTENT_ID"))) {
        log_fatal("param.sfo does not contain `CONTENT_ID`!");
    }

    std::vector<sfo::SfoPubtoolinfoIndex> pubtool_data = sfo::read_pubtool_data(sfo_data);
    std::vector<std::string> pubtool_keys;
    if (std::count(sfo_keys.begin(), sfo_keys.end(), std::string("PUBTOOLINFO"))) {
        pubtool_keys = sfo::get_pubtool_keys(pubtool_data);
    }

    std::vector<unsigned char> temp_content_id = sfo::get_value("CONTENT_ID", sfo_data);
    std::string content_id(temp_content_id.begin(), temp_content_id.end());

    std::string c_date; // "YYYY-MM-DD"
    if (std::count(pubtool_keys.begin(), pubtool_keys.end(), std::string("c_date"))) {
        try {
              c_date = sfo::get_pubtool_value("c_date", pubtool_data);
              sprintf(&buff[0], "%.4s-%.2s-%.2s", c_date.c_str(), c_date.c_str() + 4, c_date.c_str() + 6);
              c_date = std::string(buff);
        }
        catch (...) {
        } // get_pubtool_value throws if key is not found but we don't care and c_date will remain empty
    }

    std::string c_time; // "XXXXXX"
    if (std::count(pubtool_keys.begin(), pubtool_keys.end(), std::string("c_time"))) {
        try {
           c_time = sfo::get_pubtool_value("c_time", pubtool_data);
        }
        catch (...) {
        } // get_pubtool_value throws if key is not found but we don't care and c_time will remain empty
    }

    // Get content type string for GP4
    std::string content_type;
    if (opt == BASE_GAME) {
        content_type = "pkg_ps4_app";
    }
    else if (opt == GAME_PATCH) {
        content_type = "pkg_ps4_patch";
    }
    else if (opt == REMASTER) {
        content_type = "pkg_ps4_remaster";
    }
    else if (opt == THEME) {
        content_type = "pkg_ps4_theme";
    }
    else if (opt == THEME_UNLOCK) {
        content_type = "pkg_ps4_ac_nodata"; // Use "pkg_ps4_ac_nodata" as the GP4 is only used for the "unlock" PKG in this case
    }
    else if (opt == ADDITIONAL_CONTENT_DATA) {
        content_type = "pkg_ps4_ac_data";
    }
    else if (opt == ADDITIONAL_CONTENT_NO_DATA) {
        content_type = "pkg_ps4_ac_nodata";
    }
    // Generate actual GP4 file
    pugi::xml_document volume_xml = make_volume(content_id, content_type, c_date, c_time);
    pugi::xml_document files_xml = make_files(output_path, false);
    pugi::xml_document directories_xml = make_directories(output_path);
    pugi::xml_document playgo_xml;
    pugi::xml_document assembled_xml;

    std::filesystem::path playgo_xml_path(output_path);
    playgo_xml_path /= "sce_sys";
    playgo_xml_path /= "playgo-manifest.xml";
    if (std::filesystem::is_regular_file(playgo_xml_path)) {
        playgo_xml = make_playgo(playgo_xml_path);
        assembled_xml = assemble(volume_xml, playgo_xml, files_xml, directories_xml);
    }
    else {
        assembled_xml = assemble(volume_xml, playgo_xml, files_xml, directories_xml, "1000"); // TODO: Version number?
    }

    write(assembled_xml, gp4_path);
}
} // namespace gp4