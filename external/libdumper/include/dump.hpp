// Copyright (c) 2021 Al Azif
// License: GPLv3

#ifndef DUMP_HPP_
#define DUMP_HPP_

#include <iostream>
#include "dumper.h"

namespace dump {
bool __dump(const std::string& usb_device, const std::string& title_id, Dump_Options opt, const std::string& title);
bool dump_dlc(const std::string & usb_device, const std::string & title_id);
} // namespace dump

#endif // DUMP_HPP_
