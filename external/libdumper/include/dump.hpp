// Copyright (c) 2021 Al Azif
// License: GPLv3

#ifndef DUMP_HPP_
#define DUMP_HPP_

#include <iostream>
#include "dumper.h"

namespace dump {
bool __dump(const Dumper_Options& options);
bool dump_dlc(const std::string & usb_device, const std::string & title_id);
} // namespace dump

#endif // DUMP_HPP_
