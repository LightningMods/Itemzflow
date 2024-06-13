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

#define MB(x)   ((size_t) (x) << 20)

typedef struct {
    const char* dump_path;
    const char* title_id;
    Dump_Options opt;
    const char* title;
} Dumper_Options;

#define YES 1
#define NO  2

bool Dumper(const Dumper_Options& options);
std::vector<std::tuple<std::string, std::string, std::string>> query_dlc_database(const std::string& title_id);
bool copy_dir(const std::string& source_dir, const std::string& dest_dir);

#endif  // RAYLIB_TILESON_H_