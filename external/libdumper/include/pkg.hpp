#ifndef PKG_HPP_
#define PKG_HPP_

#include <iostream>

#define PKG_MAGIC 0x7F434E54

namespace pkg {
// struct from LibOrbisPKG
typedef struct {
  uint32_t magic;
  uint32_t flags;
  uint32_t unk_0x08;
  uint32_t unk_0x0C; /* 0xF */
  uint32_t entry_count;
  uint16_t sc_entry_count;
  uint16_t entry_count_2; /* same as entry_count */
  uint32_t entry_table_offset;
  uint32_t main_ent_data_size;
  uint64_t body_offset;
  uint64_t body_size;
  unsigned char padding1[0x10];
  char content_id[0x30]; /* PKG_CONTENT_ID_SIZE */
  uint32_t drm_type;
  uint32_t content_type;
  uint32_t content_flags;
  uint32_t promote_size;
  uint32_t version_date;
  uint32_t version_hash;
  uint32_t unk_0x88; /* for delta patches only? */
  uint32_t unk_0x8C; /* for delta patches only? */
  uint32_t unk_0x90; /* for delta patches only? */
  uint32_t unk_0x94; /* for delta patches only? */
  uint32_t iro_tag;
  uint32_t ekc_version; /* drm type version */
  unsigned char padding2[0x60];
  unsigned char sc_entries1_hash[0x20];  //PKG_HASH_SIZE
  unsigned char sc_entries2_hash[0x20];  // PKG_HASH_SIZE
  unsigned char digest_table_hash[0x20]; // PKG_HASH_SIZE
  unsigned char body_digest[0x20];       // PKG_HASH_SIZE
  unsigned char padding3[0x280];
  // TODO: I think these fields are actually members of element of container array
  uint32_t unk_0x400;
  uint32_t pfs_image_count;
  uint64_t pfs_flags;
  uint64_t pfs_image_offset;
  uint64_t pfs_image_size;
  uint64_t mount_image_offset;
  uint64_t mount_image_size;
  uint64_t package_size;
  uint32_t pfs_signed_size;
  uint32_t pfs_cache_size;
  unsigned char pfs_image_digest[0x20];  // PKG_HASH_SIZE
  unsigned char pfs_signed_digest[0x20]; // PKG_HASH_SIZE
  uint64_t pfs_split_size_nth_0;
  uint64_t pfs_split_size_nth_1;
  unsigned char padding4[0xB50];
  unsigned char package_digest[32];
  unsigned char package_signature[256];
} PkgHeader;

// struct from LibOrbisPKG
typedef struct {
  uint32_t id;
  uint32_t name_table_offset;
  uint32_t flags1;
  uint32_t flags2;
  uint32_t offset;
  uint32_t size;
  uint64_t padding;
} PkgTableEntry;

std::string get_entry_name_by_type(uint32_t type);
bool extract_sc0(const std::string& pkg_path, const std::string& output_path, const std::string& tid, const std::string& title);
} // namespace pkg

#endif // PKG_HPP_
