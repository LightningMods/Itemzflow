// Copyright (c) 2021 Al Azif
// License: GPLv3

#include "npbind.hpp"
#include "common.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include "log.h"
#include "libshahash.h"

namespace npbind {
std::vector<npbind::NpBindEntry> read(const std::string &path) { // Flawfinder: ignore
  // Check for empty or pure whitespace path
  if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
    log_error("Empty path argument!");
  }

  // Check if file exists and is file
  if (!std::filesystem::is_regular_file(path)) {
    log_error("Input path does not exist or is not a file!");
  }

  // Open path
  std::ifstream npbind_input(path, std::ios::in | std::ios::binary);
  if (!npbind_input || !npbind_input.good()) {
    npbind_input.close();
    log_error("Cannot open file: %s", path.c_str());
  }

  // Check file magic (Read in whole header)
  NpBindHeader header;
  npbind_input.read((char *)&header, sizeof(header)); // Flawfinder: ignore
  if (!npbind_input.good()) {
    npbind_input.close();
    log_error("Error reading header!");
  }
  if (__builtin_bswap32(header.magic) != NPBIND_MAGIC) {
    npbind_input.close();
    log_error("Input path is not a npbind.dat!");
  }

  // Read in body(s)
  std::vector<NpBindEntry> entries;
  for (uint64_t i = 0; i < __builtin_bswap64(header.num_entries); i++) {
    NpBindEntry temp_entry;
    npbind_input.read((char *)&temp_entry, __builtin_bswap64(header.entry_size)); // Flawfinder: ignore
    if (!npbind_input.good()) {
      npbind_input.close();
      log_error("Error reading entries!");
    }
    entries.push_back(temp_entry);
  }

  // Read digest
  unsigned char digest[20];
  npbind_input.seekg(-sizeof(digest), npbind_input.end); // Make sure we are in the right place
  npbind_input.read((char *)&digest, sizeof(digest));    // Flawfinder: ignore
  if (!npbind_input.good()) {
    // Should never reach here... will affect coverage %
    npbind_input.close();
    log_error("Error reading digest!");
  }
  npbind_input.close();

  // Check digest
  unsigned char calculated_digest[sizeof(digest)];

  std::stringstream ss;
  ss.write(reinterpret_cast<const char *>(&header), sizeof(header));

  for (uint64_t i = 0; i < __builtin_bswap64(header.num_entries); i++) {
    ss.write(reinterpret_cast<const char *>(&entries[i]), sizeof(NpBindEntry));
  }

  unsigned char data_to_hash[ss.str().size()];
  for (size_t i = 0; i < ss.str().size(); i++) {
    data_to_hash[i] = ss.str().c_str()[i];
  }

  Sha1Context context;
  Sha1BlockInit(&context);
  Sha1BlockUpdate(&context, data_to_hash, ss.str().size());
  Sha1BlockResult(&context, calculated_digest);

#if 0
  printf("\[SHA1] Digest: ");

  for (int i = 0; i < 20; i++)
      printf("%x", digest[i]);

      printf("\n");


      printf("\[SHA1] calculated_digest: ");

      for (int i = 0; i < 20; i++)
          printf("%x", calculated_digest[i]);

      printf("\n");
#endif

  if (std::memcmp(calculated_digest, digest, sizeof(digest)) != 0) {
      log_info("[SHA1] File %s has failed validation", path.c_str());
  }
  else
      log_info("[SHA1] File %s Successfully validated", path.c_str());

  return entries;
}
} // namespace npbind
