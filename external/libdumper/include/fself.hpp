// Copyright (c) 2021 Al Azif
// License: GPLv3

#ifndef FSELF_HPP_
#define FSELF_HPP_

#include <iostream>
#include <vector>

namespace fself {
bool is_fself(const std::string &path);
void make_fself(const std::string &input, const std::string &output, uint64_t paid, const std::string &ptype, uint64_t app_version, uint64_t fw_version, std::vector<unsigned char> auth_info);
}

#endif // FSELF_HPP_
