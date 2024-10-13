
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

      union {
          const uint8_t *file_data = nullptr;
          const header *header;
      };

      const void* lump_begins[lump_type::LUMP_MAX];
      const void* lump_ends[lump_type::LUMP_MAX];
  };

  void parse_header(bsp_info &info) {
      auto* header = reinterpret_cast<const struct header*>(info.file_data);
      if (header->version != header::bsp_version_halflife)
          throw std::runtime_error(std::format("Unsupported BSP version {}", header->version));

      for (int i = 0; i < lump_type::LUMP_MAX; ++i) {
          auto& lump = header->lumps[i];
          if (lump.offset < 0 || lump.length < 0)
              throw std::runtime_error(std::format("Lump {} is not valid", lump_names[i]));

          if (lump.offset + lump.length > info.file_size)
              throw std::runtime_error(std::format("Lump {} extends beyond end of file", lump_names[i]));

          info.lump_begins[i] = info.file_data + lump.offset;
          info.lump_ends[i]   = info.file_data + lump.offset + lump.length;
      }

      auto file = std::ofstream("map.ply", std::ios::out);

      file << "ply\n";
      file << "format ascii 1.0\n";
      file << "element vertex " << info.header->lumps[lump_type::LUMP_VERTICES].length / sizeof(bsp::vertex) << "\n";
      file << "property float x\n";
      file << "property float y\n";
      file << "property float z\n";
      file << "element face " << info.header->lumps[lump_type::LUMP_FACES].length / sizeof(bsp::face) << "\n";
      file << "property list uint int vertex_index\n";
      file << "end_header\n";

      auto* vertex_begin    = static_cast<const bsp::vertex *>(info.lump_begins[lump_type::LUMP_VERTICES]);
      for (auto* vertex = vertex_begin; vertex < info.lump_ends[lump_type::LUMP_VERTICES]; ++vertex)
          file << vertex->x << " " << vertex->y << " " << vertex->z << "\n";

      auto* face_begin      = static_cast<const bsp::face *>(info.lump_begins[lump_type::LUMP_FACES]);
      auto* face_end        = static_cast<const bsp::face *>(info.lump_ends[lump_type::LUMP_FACES]);
      auto* edge_begin      = static_cast<const bsp::edge *>(info.lump_begins[lump_type::LUMP_EDGES]);
      auto* edge_end        = static_cast<const bsp::edge *>(info.lump_ends[lump_type::LUMP_EDGES]);
      auto* surfedge_begin  = static_cast<const surf_edge *>(info.lump_begins[lump_type::LUMP_SURFEDGES]);
      auto* surfedge_end    = static_cast<const surf_edge *>(info.lump_ends[lump_type::LUMP_SURFEDGES]);
      for (auto* face = face_begin; face < face_end; ++face) {
          auto* face_edge_begin = surfedge_begin + face->first_edge;
          auto* face_edge_end   = surfedge_begin + face->first_edge + face->edge_count;

          if (face_edge_end > surfedge_end)
              throw std::runtime_error("Face has invalid surface edges");

          file << face->edge_count;

          for (auto* surf_edge = face_edge_begin; surf_edge < face_edge_end; ++surf_edge) {
              auto* edge = edge_begin + std::abs(surf_edge->edge);
              if (edge < edge_begin || edge >= edge_end)
                  throw std::runtime_error("Surface edge has invalid edges");

              uint16_t edge_index = surf_edge->edge < 0 ? edge->vertex[0] : edge->vertex[1];
              file << " " << edge_index;
          }

          file << "\n";
      }
  }

  void open_file(std::string_view filename, bsp_handle* handle) {
      handle = reinterpret_cast<bsp_handle *>(new bsp_info{});
      auto& info = reinterpret_cast<bsp_info&>(*handle);

      info.bsp_file = open(filename.data(), O_RDONLY);
      if (info.bsp_file < 0)
          throw std::runtime_error(std::format("Could not open file '{}'", filename));

      struct stat st{};
      if (fstat(info.bsp_file, &st) < 0)
          throw std::runtime_error(std::format("Could not stat file '{}'", filename));

      info.file_size = st.st_size;

      void* data = mmap(nullptr, info.file_size, PROT_READ, MAP_PRIVATE | MAP_FILE, info.bsp_file, 0);
      if (data == MAP_FAILED)
          throw std::runtime_error(std::format("Could not mmap file '{}'", filename));

      if (madvise(data, info.file_size, MADV_RANDOM | MADV_WILLNEED | MADV_HUGEPAGE) < 0)
          throw std::runtime_error(std::format("Could not madvise file '{}'", filename));

      info.file_data = reinterpret_cast<uint8_t*>(data);

      parse_header(info);
  }

}