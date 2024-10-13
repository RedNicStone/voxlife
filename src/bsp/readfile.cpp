
#include <bsp/readfile.h>
#include <bsp/primitives.h>

#include <stdexcept>
#include <format>
#include <iostream>
#include <fstream>

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>


namespace voxelife::bsp {

  struct bsp_info {
      int bsp_file = -1;
      size_t file_size = 0;
      uint8_t *file_data = nullptr;
  };

  void parse_header(bsp_info &info) {
      auto* header = reinterpret_cast<const struct header*>(info.file_data);
      if (header->version != header::bsp_version_halflife)
          throw std::runtime_error(std::format("Unsupported BSP version {}", header->version));

      auto vertex_offset = header->lumps[lump_type::LUMP_VERTICES].offset;
      auto vertex_length = header->lumps[lump_type::LUMP_VERTICES].length;

      if (vertex_offset < 0 || vertex_length < 0)
          throw std::runtime_error("Vertex lump is not valid");

      if (vertex_offset + vertex_length > info.file_size)
          throw std::runtime_error("Vertex lump extends beyond end of file");

      auto* vertex_begin = reinterpret_cast<const struct vertex*>(info.file_data + vertex_offset);
      auto* vertex_end = reinterpret_cast<const struct vertex*>(info.file_data + vertex_offset + vertex_length);

      auto file = std::fstream("map.ply", std::ios::out);

      file << "ply\n";
      file << "format ascii 1.0\n";
      file << "element vertex " << vertex_length / sizeof(struct vertex) << "\n";
      file << "property float x\n";
      file << "property float y\n";
      file << "property float z\n";
      file << "end_header\n";

      for (auto* vertex = vertex_begin; vertex < vertex_end; ++vertex)
          file << vertex->x << " " << vertex->y << " " << vertex->z << "\n";
  }

  void open_file(std::string_view filename, bsp_handle* handle) {
      handle = new bsp_handle;
      auto& info = reinterpret_cast<bsp_info&>(*handle);

      info.bsp_file = open(filename.data(), O_RDONLY);
      if (info.bsp_file < 0)
          throw std::runtime_error(std::format("Could not open file '{}'", filename));

      struct stat st{};
      if (fstat(info.bsp_file, &st) < 0)
          throw std::runtime_error(std::format("Could not stat file '{}'", filename));

      info.file_size = st.st_size;

      info.file_data = reinterpret_cast<uint8_t*>(
          mmap(nullptr, info.file_size, PROT_READ, MAP_PRIVATE | MAP_FILE, info.bsp_file, 0));
      if (info.file_data == MAP_FAILED)
          throw std::runtime_error(std::format("Could not mmap file '{}'", filename));

      if (madvise(info.file_data, info.file_size, MADV_RANDOM | MADV_WILLNEED | MADV_HUGEPAGE) < 0)
          throw std::runtime_error(std::format("Could not madvise file '{}'", filename));

      parse_header(info);
  }

}