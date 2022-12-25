// Copyright (c) 2021 Al Azif
// License: GPLv3

#ifndef SFO_HPP_
#define SFO_HPP_

#include <iostream>
#include <vector>

#define SFO_MAGIC 0x00505346

namespace sfo {
typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t key_table_offset;
  uint32_t data_table_offset;
  uint32_t num_entries;
} SfoHeader;

typedef struct {
  std::string key_name;
  uint16_t key_offset;
  uint16_t format;
  uint32_t length;
  uint32_t max_length;
  uint32_t data_offset;
  std::vector<unsigned char> data;
} SfoData;

typedef struct {
  std::string key_name;
  std::string value;
} SfoPubtoolinfoIndex;

bool is_sfo(const std::string &path);
std::vector<SfoData> read(const std::string &path);
std::vector<std::string> get_keys(const std::vector<SfoData> &data);
uint16_t get_format(const std::string &key, const std::vector<SfoData> &data);
uint32_t get_length(const std::string &key, const std::vector<SfoData> &data);
uint32_t get_max_length(const std::string &key, const std::vector<SfoData> &data);
std::vector<unsigned char> get_value(const std::string &key, const std::vector<SfoData> &data);
std::vector<SfoPubtoolinfoIndex> read_pubtool_data(const std::vector<SfoData> &data);
std::vector<std::string> get_pubtool_keys(const std::vector<SfoPubtoolinfoIndex> &data);
std::string get_pubtool_value(const std::string &key, const std::vector<SfoPubtoolinfoIndex> &data);
SfoData build_data(const std::string &key_name, const std::string &format, uint32_t length, uint32_t max_length, const std::vector<unsigned char> &data);
SfoPubtoolinfoIndex build_pubtool_data(const std::string &key, const std::string &value);
std::vector<SfoData> add_data(const SfoData &add_data, const std::vector<SfoData> &current_data);
std::vector<SfoData> add_pubtool_data(const SfoPubtoolinfoIndex &add_data, const std::vector<SfoData> &current_data);
std::vector<SfoData> remove_key(const std::string &remove_key, const std::vector<SfoData> &current_data);
std::vector<SfoData> remove_pubtool_key(const std::string &remove_key, const std::vector<SfoData> &current_data);
bool compare_sfo_date(SfoData data_1, SfoData data_2);
void write(const std::vector<SfoData> &data, const std::string &path);
} // namespace sfo

#endif // SFO_HPP_
