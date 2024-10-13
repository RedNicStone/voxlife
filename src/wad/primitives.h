
#ifndef VOXLIFE_WAD_PRIMITIVES_H
#define VOXLIFE_WAD_PRIMITIVES_H

#include <cstdint>

namespace voxlife::wad {

  struct header {
      constexpr static const char* magic_value = "WAD3";

      char magic[4];
      uint32_t entry_count;
      uint32_t entry_offset;
  };

  struct entry {
      constexpr static uint32_t max_entry_name = 16;

      uint32_t offset;
      uint32_t disk_size;
      uint32_t size;
      uint8_t type;
      bool compressed;
      uint16_t _pad;
      char name[max_entry_name];
  };

}

#endif //VOXLIFE_WAD_PRIMITIVES_H
