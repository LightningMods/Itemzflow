#pragma once
#ifndef DUMPER_H_
#define DUMPER_H_

#include <string>

typedef enum Dump_Options {
        BASE_GAME = 1,
        GAME_PATCH = 2,
        REMASTER = 3,
        THEME = 4,
        THEME_UNLOCK = 5,
        ADDITIONAL_CONTENT_DATA = 6,
        ADDITIONAL_CONTENT_NO_DATA = 7,
        TOTAL_OF_OPTS = 7
}  Dump_Options;

bool Dumper(std::string &dump_path, std::string &title_id, Dump_Options opt, std::string &title);

#endif  // RAYLIB_TILESON_H_