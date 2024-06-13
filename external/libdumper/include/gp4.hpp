// Copyright (c) 2021 Al Azif
// License: GPLv3

#ifndef GP4_HPP_
#define GP4_HPP_

#include "pugixml.hpp"
#include "dumper.h"
#include <iostream>
#include <vector>
#include <time.h>


namespace gp4 {
void recursive_directory(const std::string &path, pugi::xml_node &node);
pugi::xml_document make_volume(const std::string &content_id, const std::string &volume_type, std::string c_date = "", std::string c_time = "");
pugi::xml_document make_playgo(const std::string &playgo_xml);
pugi::xml_document make_files(const std::string& path, std::vector<std::string>& elf_files, Dump_Options opt);
pugi::xml_document make_directories(const std::string& path);
pugi::xml_document assemble(const pugi::xml_document& volume, const pugi::xml_document& playgo, const pugi::xml_document& files, const pugi::xml_document& directories, const std::string& custom_version = "");
bool write(const pugi::xml_document& xml, const std::string& path);
void generate(const std::string& sfo_path, const std::string& output_path, const std::string& gp4_path, const std::string& title, const std::string& title_id, Dump_Options opt);
}

#endif // GP4_HPP_
