
#ifndef VOXLIFE_PRIMITIVES_H
#define VOXLIFE_PRIMITIVES_H

#include <cstdint>

namespace voxlife::bsp {


  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // BSP primitives

  struct vec3f32 {
      float x, y, z;
  };

  enum lump_type {
      LUMP_ENTITIES       = 0,
      LUMP_PLANES         = 1,
      LUMP_TEXTURES       = 2,
      LUMP_VERTICES       = 3,
      LUMP_VISIBILITY     = 4,
      LUMP_NODES          = 5,
      LUMP_TEXINFO        = 6,
      LUMP_FACES          = 7,
      LUMP_LIGHTING       = 8,
      LUMP_CLIPNODES      = 9,
      LUMP_LEAFS          = 10,
      LUMP_MARKSURFACES   = 11,
      LUMP_EDGES          = 12,
      LUMP_SURFEDGES      = 13,
      LUMP_MODELS         = 14,
      LUMP_MAX            = 15
  };

  // I swear in 2056 we will get array designators in C++
  constexpr int32_t max_lump_size[] = {
      /* [LUMP_ENTITIES]      = */ 1024,
      /* [LUMP_PLANES]        = */ 32767,
      /* [LUMP_TEXTURES]      = */ 512,
      /* [LUMP_VERTICES]      = */ 65535,
      /* [LUMP_VISIBILITY]    = */ 2097152,
      /* [LUMP_NODES]         = */ 32767,
      /* [LUMP_TEXINFO]       = */ 8192,
      /* [LUMP_FACES]         = */ 65535,
      /* [LUMP_LIGHTING]      = */ 2097152,
      /* [LUMP_CLIPNODES]     = */ 32767,
      /* [LUMP_LEAFS]         = */ 8192,
      /* [LUMP_MARKSURFACES]  = */ 65535,
      /* [LUMP_EDGES]         = */ 256000,
      /* [LUMP_SURFEDGES]     = */ 512000,
      /* [LUMP_MODELS]        = */ 400
  };

  constexpr const char* lump_names[] = {
      /* [LUMP_ENTITIES]      = */ "LUMP_ENTITIES",
      /* [LUMP_PLANES]        = */ "LUMP_PLANES",
      /* [LUMP_TEXTURES]      = */ "LUMP_TEXTURES",
      /* [LUMP_VERTICES]      = */ "LUMP_VERTICES",
      /* [LUMP_VISIBILITY]    = */ "LUMP_VISIBILITY",
      /* [LUMP_NODES]         = */ "LUMP_NODES",
      /* [LUMP_TEXINFO]       = */ "LUMP_TEXINFO",
      /* [LUMP_FACES]         = */ "LUMP_FACES",
      /* [LUMP_LIGHTING]      = */ "LUMP_LIGHTING",
      /* [LUMP_CLIPNODES]     = */ "LUMP_CLIPNODES",
      /* [LUMP_LEAFS]         = */ "LUMP_LEAFS",
      /* [LUMP_MARKSURFACES]  = */ "LUMP_MARKSURFACES",
      /* [LUMP_EDGES]         = */ "LUMP_EDGES",
      /* [LUMP_SURFEDGES]     = */ "LUMP_SURFEDGES",
      /* [LUMP_MODELS]        = */ "LUMP_MODELS"
  };

  struct lump {
      int32_t offset;
      int32_t length;
  };

  struct header {
      constexpr static int32_t bsp_version_halflife = 30;
      int32_t version;
      lump lumps[lump_type::LUMP_MAX];
  };


  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // BSP lumps

  // Entities
  struct entity {
      constexpr static uint32_t max_key_value_pairs = 32;
      constexpr static uint32_t max_value = 1024;
  };

  // Planes
  struct plane {
      vec3f32 normal;
      float dist;

      enum type : int32_t {
          PLANE_X     = 0,
          PLANE_Y     = 1,
          PLANE_Z     = 2,
          PLANE_ANYX  = 3,
          PLANE_ANYY  = 4,
          PLANE_ANYZ  = 5
      } type;
  };

  // Textures
  struct texture_header {
      uint32_t mip_texture_count;
      // Followed by int32_t * mip_texture_count offsets which point to structures of texture
  };

  struct mip_texture {
      constexpr static uint32_t max_texture_name = 16;
      constexpr static uint32_t mip_levels = 4;

      char name[max_texture_name];
      uint32_t width, height;
      uint32_t offsets[mip_levels];
  };

  // Vertices
  struct vertex : vec3f32 { };

  // Visibility
  // Purposefully omitted

  // Nodes
  struct node {
      uint32_t plane;
      int16_t children[2];
      int16_t min[3], max[3];
      uint16_t first_face, face_count;
  };

  // Texinfo
  struct texture_info {
      vec3f32 s;
      float shift_s;
      vec3f32 t;
      float shift_t;
      uint32_t mip_texture;   // index into textures lump

      enum flags : uint32_t {
          FLAGS_FULLBRIGHT    = 0x00000001,
      } flags;
  };

  // Faces
  struct face {
      uint16_t plane;
      uint16_t side;
      uint32_t first_edge;    // index into edge lumps
      uint16_t edge_count;
      uint16_t texture_info;  // index into texinfo lumps
      uint8_t styles[4];
      int32_t light_offset;   // index into lightmap data
  };

  // Lighting
  struct light_texel {
      uint8_t red;
      uint8_t green;
      uint8_t blue;
  };

  // Clipnodes
  struct clip_node {
      int32_t plane;          // index into plane lumps
      int16_t children[2];
  };

  // Leafs
  struct leaf {
      enum contents : int32_t {
          CONTENTS_EMPTY          = -1,
          CONTENTS_SOLID          = -2,
          CONTENTS_WATER          = -3,
          CONTENTS_SLIME          = -4,
          CONTENTS_LAVA           = -5,
          CONTENTS_SKY            = -6,
          CONTENTS_ORIGIN         = -7,
          CONTENTS_CLIP           = -8,
          CONTENTS_CURRENT_0      = -9,
          CONTENTS_CURRENT_90     = -10,
          CONTENTS_CURRENT_180    = -11,
          CONTENTS_CURRENT_270    = -12,
          CONTENTS_CURRENT_UP     = -13,
          CONTENTS_CURRENT_DOWN   = -14,
          CONTENTS_TRANSLUCENT    = -15,
      } contents;

      int32_t visibility_offset;                          // index into visibility lump
      int16_t min[3], max[3];                             // bounding box
      uint32_t first_mark_surface, mark_surface_count;    // index into marksurfaces lump
      uint16_t ambient_sound_levels[4];
  };

  // Marksurfaces
  struct mark_surface {
      uint16_t face;      // index into face lumps
  };

  // Edges
  struct edge {
      uint16_t vertex[2]; // index into vertices lump
  };

  // Surfedges
  struct surf_edge {
      int32_t edge;       // index into edge lumps
  };

  // Models
  struct model {
      constexpr static uint32_t max_map_hulls = 32;
      vec3f32 min, max;                  // bounding box
      vec3f32 origin;                    // coordinates of model origin
      int32_t head_nodes[max_map_hulls]; // index into node lumps
      int32_t vis_leafs;
      int32_t first_face, face_count;    // index into face lumps
  };

}

#endif //VOXLIFE_PRIMITIVES_H
