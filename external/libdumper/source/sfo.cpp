// Copyright (c) 2021 Al Azif
// License: GPLv3

#include "sfo.hpp"
#include "common.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include "log.h"
#include <user_mem.h>

namespace sfo {
bool is_sfo(const std::string &path) {
  // Check for empty or pure whitespace path
  if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
    log_error("Empty path argument!");
    return false;
  }

  // Check if file exists and is file
  if (!std::filesystem::is_regular_file(path)) {
    log_error("Input path does not exist or is not a file!");
    return false;
  }

  // Open path
  std::ifstream sfo_input(path, std::ios::in | std::ios::binary);
  if (!sfo_input || !sfo_input.good()) {
    sfo_input.close();
    log_error("Cannot open file: %s" ,path.c_str());
    return false;
  }

  // Read SFO header
  SfoHeader header;
  sfo_input.read((char *)&header, sizeof(header)); // Flawfinder: ignore
  if (!sfo_input.good()) {
    sfo_input.close();
    return false;
  }
  sfo_input.close();

  // Compare magic
  if (__builtin_bswap32(header.magic) == SFO_MAGIC) {
    return true;
  }

  return false;
}

std::vector<SfoData> read(const std::string &path) { // Flawfinder: ignore
  // Check for empty or pure whitespace path
  if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
    log_error("Empty path argument!");
    return std::vector<SfoData>();
  }

  // Check if file exists and is file
  if (!std::filesystem::is_regular_file(path)) {
    log_error("Input path does not exist or is not a file!");
    return std::vector<SfoData>();
  }

  // Open path
  std::ifstream sfo_input(path, std::ios::in | std::ios::binary);
  if (!sfo_input || !sfo_input.good()) {
    sfo_input.close();
    log_error("Cannot open file: %s",path.c_str());
    return std::vector<SfoData>();
  }

  // Check to make sure file is a SFO
  if (!is_sfo(path)) {
    sfo_input.close();
    log_error("Input path is not a SFO!");
    return std::vector<SfoData>();
  }

  // Read SFO header
  SfoHeader header;
  sfo_input.read((char *)&header, sizeof(header)); // Flawfinder: ignore
  if (!sfo_input.good()) {
    // Should never reach here... will affect coverage %
    sfo_input.close();
    log_error("Error reading SFO header!");
    return std::vector<SfoData>();
  }

  std::vector<SfoData> data;

  // Read data
  for (size_t i = 0; i < header.num_entries; i++) {
    // Cannot just read sizeof(SfoData) as it includes a std::string value for key_name which is added in the next loop along with the actual data
    SfoData temp_data;
    sfo_input.read((char *)&temp_data.key_offset, sizeof(temp_data.key_offset)); // Flawfinder: ignore
    if (!sfo_input.good()) {
      sfo_input.close();
      log_error("Error reading entry key offset!");
      return std::vector<SfoData>();
    }
    sfo_input.read((char *)&temp_data.format, sizeof(temp_data.format)); // Flawfinder: ignore
    if (!sfo_input.good()) {
      sfo_input.close();
      log_error("Error reading entry format!");
      return std::vector<SfoData>();
    }
    sfo_input.read((char *)&temp_data.length, sizeof(temp_data.length)); // Flawfinder: ignore
    if (!sfo_input.good()) {
      sfo_input.close();
      log_error("Error reading entry length!");
      return std::vector<SfoData>();
    }
    sfo_input.read((char *)&temp_data.max_length, sizeof(temp_data.max_length)); // Flawfinder: ignore
    if (!sfo_input.good()) {
      sfo_input.close();
      log_error("Error reading entry max length!");
      return std::vector<SfoData>();
    }
    sfo_input.read((char *)&temp_data.data_offset, sizeof(temp_data.data_offset)); // Flawfinder: ignore
    if (!sfo_input.good()) {
      sfo_input.close();
      log_error("Error reading entry data offset!");
      return std::vector<SfoData>();
    }
    data.push_back(temp_data);
  }

  // Read key names and data
  for (auto &&entry : data) {
    sfo_input.seekg(header.key_table_offset + entry.key_offset, sfo_input.beg);
    std::getline(sfo_input, entry.key_name, '\0');

    unsigned char buffer[entry.length];
    sfo_input.seekg(header.data_table_offset + entry.data_offset, sfo_input.beg);
    sfo_input.read((char *)&buffer, sizeof(buffer)); // Flawfinder: ignore
    if (!sfo_input.good()) {
      sfo_input.close();
      log_error("Error reading data table!");
    }

    for (size_t i = 0; i < sizeof(buffer); i++) {
      entry.data.push_back(buffer[i]);
    }
  }
  sfo_input.close();

  return data;
}

std::vector<std::string> get_keys(const std::vector<SfoData> &data) {
  std::vector<std::string> temp_key_list;
  for (auto &&entry : data) {
    temp_key_list.push_back(entry.key_name);
  }
  return temp_key_list;
}

uint16_t get_format(const std::string &key, const std::vector<SfoData> &data) {
  for (auto &&entry : data) {
    if (entry.key_name == key) {
      return entry.format;
    }
  }
  log_error("Could not find key");
  return (uint16_t)NULL;
}

uint32_t get_length(const std::string &key, const std::vector<SfoData> &data) {
  for (auto &&entry : data) {
    if (entry.key_name == key) {
      return entry.length;
    }
  }
  log_error("Could not find key");
  return (uint32_t)NULL;
}


uint32_t get_max_length(const std::string &key, const std::vector<SfoData> &data) {
  for (auto &&entry : data) {
    if (entry.key_name == key) {
      return entry.max_length;
    }
  }
  log_error("Could not find key");
  return (uint32_t)NULL;
}

std::vector<unsigned char> get_value(const std::string &key, const std::vector<SfoData> &data) {
  std::vector<unsigned char> buffer;
  for (auto &&entry : data) {
    if (entry.key_name == key) {
      for (uint32_t i = 0; i < entry.length; i++) {
        buffer.push_back(entry.data[i]);
      }
    }
  }
  log_error("Could not find key");
  return buffer;
}

std::vector<SfoPubtoolinfoIndex> read_pubtool_data(const std::vector<SfoData> &data) {
  std::vector<std::string> sfo_keys = get_keys(data);
  std::vector<SfoPubtoolinfoIndex> pubtool_data;

  if (std::count(sfo_keys.begin(), sfo_keys.end(), std::string("PUBTOOLINFO"))) {
    std::vector<unsigned char> pubtoolinfo_buffer = get_value("PUBTOOLINFO", data);
    std::stringstream ss(std::string(pubtoolinfo_buffer.begin(), pubtoolinfo_buffer.end()));

    std::vector<std::string> csv;
    while (ss.good()) {
      std::string substr;
      getline(ss, substr, ',');
      csv.push_back(substr);
    }

    for (auto &&value : csv) {
      SfoPubtoolinfoIndex temp_index;
      temp_index.key_name = value.substr(0, value.find('='));
      temp_index.value = value.substr(value.find('=') + 1, value.size());
      pubtool_data.push_back(temp_index);
    }
  }

  return pubtool_data;
}

std::vector<std::string> get_pubtool_keys(const std::vector<SfoPubtoolinfoIndex> &data) {
  std::vector<std::string> temp_key_list;
  for (auto &&entry : data) {
    temp_key_list.push_back(entry.key_name);
  }
  return temp_key_list;
}

std::string get_pubtool_value(const std::string &key, const std::vector<SfoPubtoolinfoIndex> &data) {
  for (auto &&entry : data) {
    if (entry.key_name == key) {
      return entry.value;
    }
  }
  log_error("Could not find key");
  return "";
}

SfoData build_data(const std::string &key_name, const std::string &format, uint32_t length, uint32_t max_length, const std::vector<unsigned char> &data) {
  SfoData new_data;

  new_data.key_name = key_name;
  new_data.key_offset = 0;
  new_data.data_offset = 0;

  // Calculate format value
  if (format == "special") {
    // Used in contents generated by the system (e.g.: save data)
    new_data.format = 0x0004;
  } else if (format == "utf-8") {
    // Character string, NULL finished (0x00)
    new_data.format = 0x0204;
  } else if (format == "integer") {
    // 32 bits unsigned
    new_data.format = 0x0404;
  } else {
    log_error("Unknown SFO format type!");
  }

  if (length > max_length) {
    log_error("Input `length` of SFO entry must be <= input `max_length`!");
  }
  new_data.length = length;
  new_data.max_length = max_length;

  if (data.size() > length) {
    log_error("Input SFO data is larger than the `length`");
  }
  new_data.data = data;

  return new_data;
}

SfoPubtoolinfoIndex build_pubtool_data(const std::string &key, const std::string &value) {
  SfoPubtoolinfoIndex new_data;
  new_data.key_name = key;
  new_data.value = value;

  return new_data;
}

std::vector<SfoData> add_data(const SfoData &data_to_add, const std::vector<SfoData> &current_data) {
  // Validate data_to_add
  if (data_to_add.format != 0x0004 && data_to_add.format != 0x0204 && data_to_add.format != 0x0404) {
    log_error("Unknown SFO format type!");
  }
  if (data_to_add.length > data_to_add.max_length) {
    log_error("Input `length` of SFO entry must be <= input `max_length`!");
  }
  if (data_to_add.data.size() > data_to_add.length) {
    log_error("Input SFO data is larger than the `length`");
  }

  // Remove existing key from current data and add new data
  std::vector<SfoData> new_data = remove_key(data_to_add.key_name, current_data);
  new_data.push_back(data_to_add);

  return new_data;
}

std::vector<SfoData> add_pubtool_data(const SfoPubtoolinfoIndex &data_to_add, const std::vector<SfoData> &current_data) {
  std::vector<SfoData> new_data = remove_pubtool_key(data_to_add.key_name, current_data);
  std::vector<SfoPubtoolinfoIndex> pubtool_data = read_pubtool_data(new_data);

  std::vector<unsigned char> new_value;
  for (auto &&entry : pubtool_data) {
    for (auto &&character : entry.key_name) {
      new_value.push_back(character);
    }
    new_value.push_back('=');
    for (auto &&character : entry.value) {
      new_value.push_back(character);
    }
    new_value.push_back(',');
  }
  new_value.pop_back(); // Remove trailing comma
  if (new_value.size() > 0x200) {
      log_error("New PUBTOOLINFO key is too large (> 0x200)!");
      return new_data;
  }

  SfoData new_entry = build_data("PUBTOOLINFO", "utf-8", new_value.size(), 0x200, new_value);
  new_data = add_data(new_entry, new_data);

  return new_data;
}

std::vector<SfoData> remove_key(const std::string &remove_key, const std::vector<SfoData> &current_data) {
  std::vector<SfoData> new_data;
  for (auto &&entry : current_data) {
    if (entry.key_name != remove_key) {
      new_data.push_back(entry);
    }
  }

  return new_data;
}

std::vector<SfoData> remove_pubtool_key(const std::string &remove_key, const std::vector<SfoData> &current_data) {
  std::vector<std::string> sfo_keys = sfo::get_keys(current_data);
  if (!std::count(sfo_keys.begin(), sfo_keys.end(), std::string("PUBTOOLINFO"))) {
    return current_data;
  }

  std::vector<SfoPubtoolinfoIndex> pubtool_data = read_pubtool_data(current_data);
  std::vector<SfoPubtoolinfoIndex> new_pubtool_data;

  for (auto &&index : pubtool_data) {
    if (index.key_name != remove_key) {
      new_pubtool_data.push_back(index);
    }
  }

  std::vector<unsigned char> new_value;
  for (auto &&entry : new_pubtool_data) {
    for (auto &&character : entry.key_name) {
      new_value.push_back(character);
    }
    new_value.push_back('=');
    for (auto &&character : entry.value) {
      new_value.push_back(character);
    }
    new_value.push_back(',');
  }
  new_value.pop_back(); // Remove trailing comma
  if (new_value.size() > 0x200) {
      log_error("New PUBTOOLINFO key is too large (> 0x200)!");
  }

  SfoData new_entry = build_data("PUBTOOLINFO", "utf-8", new_value.size(), 0x200, new_value);
  std::vector<SfoData> new_data = add_data(new_entry, current_data);

  return new_data;
}

bool compare_sfo_date(SfoData data_1, SfoData data_2) {
  return (data_1.key_name.compare(data_2.key_name) < 0);
}

void write(const std::vector<SfoData> &data, const std::string &path) {
  // Check for empty or pure whitespace path
  if (path.empty() || std::all_of(path.begin(), path.end(), [](char c) { return std::isspace(c); })) {
    log_error("Empty path argument!");
    return;
  }

  std::filesystem::path output_path(path);

  // Exists, but is not a file
  if (std::filesystem::exists(output_path) && !std::filesystem::is_regular_file(output_path)) {
    log_error("Output path exists, but is not a file!");
    return;
  }

  // Open path
  std::ofstream output_file(output_path, std::ios::out | std::ios::trunc | std::ios::binary);
  if (!output_file || !output_file.good()) {
    output_file.close();
    log_error("Cannot open output file: %s",path.c_str());
    return;
  }

  // Check for duplicate key_name
  for (size_t i = 0; i < data.size(); i++) {
    for (size_t j = 0; j < data.size(); j++) {
      if (i != j && data[i].key_name == data[j].key_name) {
          log_error("Duplicate key name found in SFO!");
      }
    }
  }

  // Validate/Standardize data
  std::vector<SfoData> new_data;
  for (auto &&entry : data) {
    if (entry.format != 0x0004 && entry.format != 0x0204 && entry.format != 0x0404) {
      output_file.close();
      log_error("Unknown SFO format type!");
      return;
    }
    if (entry.length > entry.max_length) {
      output_file.close();
      log_error("Input `length` of SFO entry must be <= input `max_length`!");
      return;
    }
    if (entry.data.size() > entry.length) {
      output_file.close();
      log_error("Input SFO data is larger than the `length`");
      return;
    }

    SfoData temp_data = entry;
    temp_data.key_offset = 0;
    temp_data.data_offset = 0;

    new_data.push_back(temp_data);
  }

  // Alphabetize new_data by key_name
  std::sort(new_data.begin(), new_data.end(), compare_sfo_date);

  // Calculate file offsets
  uint16_t total_key_offset = 0;
  uint32_t total_data_offset = 0;

  // Calculate key and data table offsets
  for (auto &&entry : new_data) {
    entry.key_offset = total_key_offset;
    total_key_offset += entry.key_name.size();
    total_key_offset++; // Null terminator

    entry.data_offset = total_data_offset;
    total_data_offset += entry.max_length;
  }

  // Add alignment bytes
  while (total_key_offset % 0x4 != 0) {
    total_key_offset++;
  }

  // Build header
  SfoHeader header;
  header.magic = __builtin_bswap32(SFO_MAGIC);
  header.version = 0x00000101;
  header.key_table_offset = new_data.size() * 0x10 + sizeof(header);
  header.data_table_offset = header.key_table_offset + total_key_offset;
  header.num_entries = new_data.size();

  // Create stringstream
  std::stringstream ss;

  // Write header to stringstream
  ss.write(reinterpret_cast<const char *>(&header), sizeof(header));

  // Write index to stringstream
  for (auto &&entry : new_data) {
    ss.write(reinterpret_cast<const char *>(&entry.key_offset), sizeof(entry.key_offset));
    ss.write(reinterpret_cast<const char *>(&entry.format), sizeof(entry.format));
    ss.write(reinterpret_cast<const char *>(&entry.length), sizeof(entry.length));
    ss.write(reinterpret_cast<const char *>(&entry.max_length), sizeof(entry.max_length));
    ss.write(reinterpret_cast<const char *>(&entry.data_offset), sizeof(entry.data_offset));
  }

  // Build key table
  std::vector<unsigned char> key_table;
  for (auto &&entry : new_data) {
    for (size_t i = 0; i < entry.key_name.size(); i++) {
      key_table.insert(key_table.begin() + entry.key_offset + i, entry.key_name[i]);
      if (i == entry.key_name.size() - 1) {
        key_table.insert(key_table.begin() + entry.key_offset + i + 1, '\0');
      }
    }
  }

  // Add alignment bytes to key table
  while (key_table.size() % 0x4 != 0) {
    key_table.push_back('\0');
  }

  // Write key table to stringstream
  for (auto &&entry : key_table) {
    ss.write(reinterpret_cast<const char *>(&entry), sizeof(entry));
  }

  // Build data table
  std::vector<unsigned char> data_table;
  for (auto &&entry : new_data) {
    for (size_t i = 0; i < entry.max_length; i++) {
      if (i < entry.length) {
        data_table.insert(data_table.begin() + entry.data_offset + i, entry.data[i]);
      } else {
        data_table.insert(data_table.begin() + entry.data_offset + i, '\0');
      }
    }
  }

  // Write data table to stringstream
  for (auto &&entry : data_table) {
    ss.write(reinterpret_cast<const char *>(&entry), sizeof(entry));
  }

  // Write streamstream to output file
  output_file << ss.rdbuf();
  output_file.close();
}
} // namespace sfo
