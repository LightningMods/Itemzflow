// Copyright (c) 2021 Al Azif
// License: GPLv3

#include "common.hpp"
#include "dump.hpp"
#include "dumper.h"
#include <iostream>
#include <unistd.h>

#define DUMPER_LOG "/user/app/ITEM00001/logs/if_dumper.log"


bool Dumper(std::string &dump_path, std::string &title_id, Dump_Options opt, std::string &title)
{
    /*-- INIT LOGGING FUNCS --*/
    unlink(DUMPER_LOG);
    log_set_quiet(false);
    log_set_level(LOG_DEBUG);
    FILE* fp = fopen(DUMPER_LOG, "w");
    if(fp != NULL)
      log_add_fp(fp, LOG_DEBUG);

    log_info("LibDumper Started with path: %s tid: %s title: %s opt: %i", dump_path.c_str(), title_id.c_str(), title.c_str(), opt);
    return dump::__dump(dump_path, title_id, opt, title);
}

