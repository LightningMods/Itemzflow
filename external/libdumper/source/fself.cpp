// Copyright (c) 2021 Al Azif
// License: GPLv3

#include "fself.hpp"
#include "common.hpp"
#include "elf.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "log.h"
#include <user_mem.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace fself {

bool is_fself(const std::string& self_path){
    char buf[200];
    int ret = syscall(607, self_path.c_str(), buf);
    log_info("res: %i (%s)", ret, (ret == -1) ? strerror(errno) : "No Error");
    return (ret == 0);
}

void make_fself(const std::string &input, const std::string &output, uint64_t paid, const std::string &ptype, uint64_t app_version, uint64_t fw_version, std::vector<unsigned char> auth_info) {
  // Check for empty or pure whitespace path
  if (input.empty() || std::all_of(input.begin(), input.end(), [](char c) { return std::isspace(c); })) {
    log_error("Empty input path argument!");
    return;
  }

  // Check if file exists and is file
  if (!std::filesystem::is_regular_file(input)) {
    log_error("Input path does not exist or is not a file!");
    return;
  }

  // Open path
  std::ifstream self_input(input, std::ios::in | std::ios::binary);
  if (!self_input || !self_input.good()) {
    self_input.close();
    log_error("Cannot open file: %s",input.c_str());
    return;
  }


  // Check for empty or pure whitespace path
  if (output.empty() || std::all_of(output.begin(), output.end(), [](char c) { return std::isspace(c); })) {
    self_input.close();
    log_error("Empty output path argument!");
     return;
  }

  std::filesystem::path output_path(output);

  // Exists, but is not a file
  if (std::filesystem::exists(output_path) && !std::filesystem::is_regular_file(output_path)) {
    self_input.close();
    log_error("Oputput object exists but is not a file!");
     return;
  }

  // paid, app_version and fw_version are unsigned and any value between 0x0 and 0xFFFFFFFFFFFFFFFF is valid so we do not have to check range

  if (ptype != "fake" && ptype != "npdrm_exec" && ptype != "npdrm_dynlib" && ptype != "system_exec" && ptype != "system_dynlib" && ptype != "host_kernel" && ptype != "secure_module" && ptype != "secure_kernel") {
    self_input.close();
    log_error("Invalid ptype!");
     return;
  }

  // Should be 0x110 in size and 0-9a-fA-F
  if (auth_info.size() != 0x110) {
    self_input.close();
    log_error("Auth info is invalid length!");
    return;
  }
  if (std::all_of(output.begin(), output.end(), [](char c) { return std::isxdigit(c); })) {
    self_input.close();
    log_error("Auth info is not hex!");
    return;
  }

  // Input may not be "correct" but it's valid at this point

  // TODO
  self_input.close();
  UNUSED(paid);
  UNUSED(app_version);
  UNUSED(fw_version);
}
} // namespace fself
