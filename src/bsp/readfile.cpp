
#include <bsp/readfile.h>
#include <bsp/primitives.h>

#include <stdexcept>
#include <format>
#include <iostream>
#include <fstream>

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>


namespace voxlife::bsp {

  struct bsp_info {
      int bsp_file = -1;
      size_t file_size = 0;

      union {
          const uint8_t *file_data = nullptr;
          const header *header;
      };

      const void* lump_begins[lump_type::LUMP_MAX];
      const void* lump_ends[lump_type::LUMP_MAX];

      std::span<const entity>       entities;
      std::span<const plane>        planes;
      std::span<const mip_texture>  textures;
      std::span<const vertex>       vertices;
      std::span<const node>         nodes;
      std::span<const texture_info> texture_infos;
      std::span<const face>         faces;
      std::span<const clip_node>    clip_nodes;
      std::span<const leaf>         leafs;
      std::span<const mark_surface> mark_surfaces;
      std::span<const edge>         edges;
      std::span<const surf_edge>    surface_edges;
      std::span<const model>        models;
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

      info.entities      = std::span(static_cast<const entity*>      (info.lump_begins[lump_type::LUMP_ENTITIES]),
                                     static_cast<const entity*>      (  info.lump_ends[lump_type::LUMP_ENTITIES]));
      info.planes        = std::span(static_cast<const plane*>       (info.lump_begins[lump_type::LUMP_PLANES]),
                                     static_cast<const plane*>       (  info.lump_ends[lump_type::LUMP_PLANES]));
      info.textures      = std::span(static_cast<const mip_texture*> (info.lump_begins[lump_type::LUMP_TEXTURES]),
                                     static_cast<const mip_texture*> (  info.lump_ends[lump_type::LUMP_TEXTURES]));
      info.vertices      = std::span(static_cast<const vertex*>      (info.lump_begins[lump_type::LUMP_VERTICES]),
                                     static_cast<const vertex*>      (  info.lump_ends[lump_type::LUMP_VERTICES]));
      info.nodes         = std::span(static_cast<const node*>        (info.lump_begins[lump_type::LUMP_NODES]),
                                     static_cast<const node*>        (  info.lump_ends[lump_type::LUMP_NODES]));
      info.texture_infos = std::span(static_cast<const texture_info*>(info.lump_begins[lump_type::LUMP_TEXINFO]),
                                     static_cast<const texture_info*>(  info.lump_ends[lump_type::LUMP_TEXINFO]));
      info.faces         = std::span(static_cast<const face*>        (info.lump_begins[lump_type::LUMP_FACES]),
                                     static_cast<const face*>        (  info.lump_ends[lump_type::LUMP_FACES]));
      info.clip_nodes    = std::span(static_cast<const clip_node*>   (info.lump_begins[lump_type::LUMP_CLIPNODES]),
                                     static_cast<const clip_node*>   (  info.lump_ends[lump_type::LUMP_CLIPNODES]));
      info.leafs         = std::span(static_cast<const leaf*>        (info.lump_begins[lump_type::LUMP_LEAFS]),
                                     static_cast<const leaf*>        (  info.lump_ends[lump_type::LUMP_LEAFS]));
      info.mark_surfaces = std::span(static_cast<const mark_surface*>(info.lump_begins[lump_type::LUMP_MARKSURFACES]),
                                     static_cast<const mark_surface*>(  info.lump_ends[lump_type::LUMP_MARKSURFACES]));
      info.edges         = std::span(static_cast<const edge*>        (info.lump_begins[lump_type::LUMP_EDGES]),
                                     static_cast<const edge*>        (  info.lump_ends[lump_type::LUMP_EDGES]));
      info.surface_edges = std::span(static_cast<const surf_edge*>   (info.lump_begins[lump_type::LUMP_SURFEDGES]),
                                     static_cast<const surf_edge*>   (  info.lump_ends[lump_type::LUMP_SURFEDGES]));
      info.models        = std::span(static_cast<const model*>       (info.lump_begins[lump_type::LUMP_MODELS]),
                                     static_cast<const model*>       (  info.lump_ends[lump_type::LUMP_MODELS]));
  }

  template<typename T>
  constexpr T& span_at(std::span<T> span, size_t index) {
      if (index >= span.size())
          throw std::out_of_range("Span index out of bounds");

      return span[index];
  }

  template<typename T>
  constexpr std::span<T> safe_subspan(std::span<T> span, size_t offset, size_t length) {
      if (offset + length > span.size())
          throw std::out_of_range("Span index out of bounds");

      return span.subspan(offset, length);
  }

  void read_map(bsp_info &info) {
      auto file = std::ofstream("map.ply", std::ios::out);

      file << "ply\n";
      file << "format ascii 1.0\n";
      file << "element vertex " << info.vertices.size() << "\n";
      file << "property float x\n";
      file << "property float y\n";
      file << "property float z\n";
      file << "element face " << info.faces.size() << "\n";
      file << "property list uint int vertex_index\n";
      file << "end_header\n";

      for (auto& vertex : info.vertices)
          file << vertex.x << " " << vertex.y << " " << vertex.z << "\n";

      for (auto& face : info.faces) {
          auto surface_edges = safe_subspan(info.surface_edges, face.first_edge, face.edge_count);

          file << face.edge_count;
          for (auto& surface_edge : surface_edges) {
              auto edge = span_at(info.edges, std::abs(surface_edge.edge));

              uint16_t edge_index = surface_edge.edge < 0 ? edge.vertex[0] : edge.vertex[1];
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

      read_map(info);
  }

}