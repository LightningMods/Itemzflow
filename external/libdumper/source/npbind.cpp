// Copyright (c) 2021-2022 Al Azif
// License: GPLv3

#include "npbind.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "common.hpp"

#include <sha1.h>


namespace npbind {
std::vector<npbind::NpBindEntry> read(const std::string &path) { // Flawfinder: ignore
    std::error_code ec;
  // Check for empty or pure whitespace path
  if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
    log_error("Empty path argument!");
    return std::vector<npbind::NpBindEntry>();
  }

  // Check if file exists and is file
  if (!std::filesystem::is_regular_file(path, ec)) {
    log_error("Input path does not exist or is not a file!");
    return std::vector<npbind::NpBindEntry>();
  }

  // Open path
  std::ifstream npbind_input(path, std::ios::in | std::ios::binary);
  if (!npbind_input || !npbind_input.good()) {
    npbind_input.close();
    log_error("Cannot open file: %s", path.c_str());
    return std::vector<npbind::NpBindEntry>();
  }

  // Check file magic (Read in whole header)
  NpBindHeader header;
  npbind_input.read((char *)&header, sizeof(header)); // Flawfinder: ignore
  if (!npbind_input.good()) {
    npbind_input.close();
    log_error("Error reading header!");
    return std::vector<npbind::NpBindEntry>();

  }
  if (__builtin_bswap32(header.magic) != NPBIND_MAGIC) {
    npbind_input.close();
    log_error("Input path is not a npbind.dat!");
    return std::vector<npbind::NpBindEntry>();
  }

  // Read in body(s)
  std::vector<NpBindEntry> entries;
  for (uint64_t i = 0; i < __builtin_bswap64(header.num_entries); i++) {
    NpBindEntry temp_entry;
    npbind_input.read((char *)&temp_entry, __builtin_bswap64(header.entry_size)); // Flawfinder: ignore
    if (!npbind_input.good()) {
      npbind_input.close();
      log_error("Error reading entries!");
      return std::vector<npbind::NpBindEntry>();
    }
    entries.push_back(temp_entry);
  }

  // Read digest
  std::vector<unsigned char> digest(SHA1::HashBytes);
  npbind_input.seekg(-digest.size(), npbind_input.end);                   // Make sure we are in the right place
  npbind_input.read(reinterpret_cast<char *>(&digest[0]), digest.size()); // Flawfinder: ignore
  if (!npbind_input.good()) {
    // Should never reach here... will affect coverage %
    npbind_input.close();
    log_error("Error reading digest!");
    return std::vector<npbind::NpBindEntry>();
  }
  npbind_input.close();

// Check digest
  std::stringstream ss;
  ss.write(reinterpret_cast<const char *>(&header), sizeof(header));

  for (uint64_t i = 0; i < __builtin_bswap64(header.num_entries); i++) {
    ss.write(reinterpret_cast<const char *>(&entries[i]), sizeof(NpBindEntry));
  }

  std::vector<unsigned char> data_to_hash;
  for (size_t i = 0; i < ss.str().size(); i++) {
    data_to_hash.push_back(ss.str().c_str()[i]);
  }

  std::vector<unsigned char> calculated_digest(digest.size());

  SHA1 sha1;
  sha1(&data_to_hash[0], ss.str().size());
  sha1.getHash(&calculated_digest[0]);

  if (std::memcmp(&calculated_digest[0], &digest[0], digest.size()) != 0) {
      log_error("Digests do not match! Aborting...");
  }
   else
      log_info("Digests match!");

  return entries;
}
} // namespace npbind
