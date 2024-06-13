// Copyright (c) 2021 Al Azif
// License: GPLv3

#include "common.hpp"
#include "dump.hpp"
#include "dumper.h"
#include <iostream>
#include <string>
#include <unistd.h>
#include "lang.h"
#include "pfs.hpp"

#define DUMPER_LOG "/user/app/ITEM00001/logs/dumper.log"
#define USB_DUMPER_LOG "/itemzflow/dumper.log"

unsigned int usbpath();

bool Dumper(const Dumper_Options& options)
{
    /*-- INIT LOGGING FUNCS --*/
    unlink(DUMPER_LOG);
    log_set_quiet(false);
    log_set_level(LOG_DEBUG);
    FILE* fp = fopen(DUMPER_LOG, "w");
    if(fp != NULL)
      log_add_fp(fp, LOG_DEBUG); 
    
    std::string usb_path = "/mnt/usb"+std::to_string(usbpath())+USB_DUMPER_LOG;
    unlink(usb_path.c_str());
    fp = fopen(usb_path.c_str(), "w");
    if(fp != NULL)
      log_add_fp(fp, LOG_DEBUG); 


    log_info("LibDumper Started with path: %s tid: %s title: %s opt: %i", options.dump_path, options.title_id, options.title, options.opt);

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

    pfs::dump_op = options.opt;
    return (options.opt == ADDITIONAL_CONTENT_DATA) ? dump::dump_dlc(options.dump_path, options.title_id) :  dump::__dump(options);
}
