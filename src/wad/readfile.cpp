
#include <wad/readfile.h>
#include <wad/primitives.h>

#include <stdexcept>
#include <format>
#include <iostream>
#include <fstream>
#include <cstring>
#include <unordered_map>

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>


namespace voxlife::wad {

  struct case_insensitive_hash {
      std::size_t operator()(std::string_view s) const noexcept {
#if SIZE_MAX == UINT64_MAX
          const std::size_t FNV_offset_basis = 14695981039346656037ULL;
          const std::size_t FNV_prime = 1099511628211ULL;
#elif SIZE_MAX == UINT32_MAX
          const std::size_t FNV_offset_basis = 2166136261U;
          const std::size_t FNV_prime = 16777619U;
#else
#error "Unsupported size_t size"
#endif
          std::size_t hash = FNV_offset_basis;
          for (unsigned char c : s) {
              c = to_lower_ascii(c);
              hash ^= c;
              hash *= FNV_prime;
          }
          return hash;
      }

    protected:
      static constexpr unsigned char to_lower_ascii(unsigned char c) noexcept {
          // Convert uppercase ASCII letters to lowercase
          if (c >= 'A' && c <= 'Z')
              return c + 32;
          return c;
      }
  };

  struct case_insensitive_equal : case_insensitive_hash {
      bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
          if (lhs.size() != rhs.size())
              return false;

          for (std::size_t i = 0; i < lhs.size(); ++i) {
              if (case_insensitive_hash::to_lower_ascii(lhs[i]) != case_insensitive_hash::to_lower_ascii(rhs[i]))
                  return false;
          }
          return true;
      }
  };

  struct wad_info {
      int wad_file = -1;
      size_t file_size = 0;

      union {
          const uint8_t *file_data = nullptr;
          const header *header;
      };

      std::unordered_map<std::string_view, const void*, case_insensitive_hash, case_insensitive_equal> entries;
  };

  void index_entries(wad_info &info) {
      if (std::memcmp(info.file_data, header::magic_value, 4) != 0)
          throw std::runtime_error("Invalid WAD magic value");

      std::span<const entry> entries(reinterpret_cast<const entry*>(info.file_data + info.header->entry_offset),
                                     info.header->entry_count);

      for (auto& entry : entries) {
          if (entry.compressed)
              throw std::runtime_error("Compressed entries are not supported");

          std::cout << entry.name << std::endl;

          info.entries[entry.name] = info.file_data + entry.offset;
      }
  }

  void open_file(std::string_view filename, wad_handle* handle) {
      *handle = reinterpret_cast<wad_handle>(new wad_info());
      auto& info = reinterpret_cast<wad_info&>(**handle);

      info.wad_file = open(filename.data(), O_RDONLY);
      if (info.wad_file < 0)
          throw std::runtime_error(std::format("Could not open file '{}'", filename));

      struct stat st{};
      if (fstat(info.wad_file, &st) < 0)
          throw std::runtime_error(std::format("Could not stat file '{}'", filename));

      info.file_size = st.st_size;

      void* data = mmap(nullptr, info.file_size, PROT_READ, MAP_PRIVATE | MAP_FILE, info.wad_file, 0);
      if (data == MAP_FAILED)
          throw std::runtime_error(std::format("Could not mmap file '{}'", filename));

      if (madvise(data, info.file_size, MADV_RANDOM | MADV_WILLNEED | MADV_HUGEPAGE) < 0)
          throw std::runtime_error(std::format("Could not madvise file '{}'", filename));

      info.file_data = reinterpret_cast<uint8_t*>(data);

      index_entries(info);
  }

  const void* get_entry(wad_handle handle, std::string_view name) {
      auto& info = reinterpret_cast<wad_info&>(*handle);

      auto it = info.entries.find(name);
      if (it == info.entries.end())
          return nullptr;

      return it->second;
  }

}
