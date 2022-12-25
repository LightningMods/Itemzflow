// Copyright (c) 2021 Al Azif
// License: GPLv3

#ifndef NPBIND_HPP_
#define NPBIND_HPP_

#include <iostream>
#include <vector>

#define NPBIND_MAGIC 0xD294A018

namespace npbind {
typedef struct {
  uint32_t magic;
  uint32_t version;
  uint64_t file_size;
  uint64_t entry_size;
  uint64_t num_entries;
  char padding[0x60];
} NpBindHeader;

typedef struct {
  uint16_t type;
  uint16_t size;
  char data[0xC];
} NpBindNpCommIdEntry;

typedef struct {
  uint16_t type;
  uint16_t size;
  char data[0xC];
} NpBindTrophyNumberEntry;

typedef struct {
  uint16_t type;
  uint16_t size;
  unsigned char data[0xB0];
} NpBindUnk1Entry;

typedef struct {
  uint16_t type;
  uint16_t size;
  unsigned char data[0x10];
} NpBindUnk2Entry;

typedef struct {
  NpBindNpCommIdEntry npcommid;
  NpBindTrophyNumberEntry trophy_number;
  NpBindUnk1Entry unk1;
  NpBindUnk2Entry unk2;
  char padding[0x98];
} NpBindEntry;

std::vector<NpBindEntry> read(const std::string &path); // Flawfinder: ignore
} // namespace npbind

#endif // NPBIND_HPP_
