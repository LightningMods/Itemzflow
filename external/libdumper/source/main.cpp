// Copyright (c) 2021 Al Azif
// License: GPLv3

#include "common.hpp"
#include "dump.hpp"
#include "dumper.h"
#include <iostream>
#include <unistd.h>
#include "lang.h"
#include "pfs.hpp"

#define DUMPER_LOG "/user/app/ITEM00001/logs/if_dumper.log"


bool Dumper(const std::string &dump_path, const std::string &title_id, Dump_Options opt, const std::string &title)
{
    /*-- INIT LOGGING FUNCS --*/
    unlink(DUMPER_LOG);
    log_set_quiet(false);
    log_set_level(LOG_DEBUG);
    FILE* fp = fopen(DUMPER_LOG, "w");
    if(fp != NULL)
      log_add_fp(fp, LOG_DEBUG);

    log_info("LibDumper Started with path: %s tid: %s title: %s opt: %i", dump_path.c_str(), title_id.c_str(), title.c_str(), opt);

    if (!LoadDumperLangs(DumperGetLang())) {
        log_debug("Failed to load the language, loading the backup");
       if (!LoadDumperLangs(0x01)) {
          log_debug("Failed to load the backup, loading the embedded");
          if(!load_dumper_embdded_eng()){
              log_debug("Failed to load the embedded, exiting");
              return false;
          }
        }
        else
            log_debug("Loaded the backup, lang %i failed to load", DumperGetLang());
    }

    pfs::dump_op = opt;
    return (opt == ADDITIONAL_CONTENT_DATA) ? dump::dump_dlc(dump_path, title_id) :  dump::__dump(dump_path, title_id, opt, title);
}
