#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

struct MyVertex {
    daxa_f32vec3 pos;
    daxa_f32vec2 uv;
    daxa_u32 model_id;
    daxa_ImageViewIndex tex_id;
    daxa_u32 flags;
};
DAXA_DECL_BUFFER_PTR(MyVertex)

struct GpuVoxel {
    daxa_u32 color;
};
DAXA_DECL_BUFFER_PTR(GpuVoxel)
struct GpuModelManifest {
    daxa_BufferPtr(GpuVoxel) voxels;
    daxa_f32vec3 aabb_min;
    daxa_f32vec3 aabb_max;
};
DAXA_DECL_BUFFER_PTR(GpuModelManifest)

struct FrameConstants {
    daxa_f32mat4x4 world_to_view;
    daxa_f32mat4x4 view_to_world;
    daxa_f32mat4x4 view_to_clip;
    daxa_f32mat4x4 clip_to_view;
    daxa_SamplerId sampler_llr;
    daxa_SamplerId sampler_nnr;
    float time;
};
DAXA_DECL_BUFFER_PTR(FrameConstants)

DAXA_DECL_TASK_HEAD_BEGIN(DrawToSwapchainH)
DAXA_TH_BUFFER_PTR(VERTEX_SHADER_READ, daxa_BufferPtr(FrameConstants), frame_constants)
DAXA_TH_BUFFER_PTR(VERTEX_SHADER_READ, daxa_BufferPtr(MyVertex), vertices)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, color_target)
DAXA_TH_IMAGE(DEPTH_ATTACHMENT, REGULAR_2D, depth_target)
DAXA_TH_IMAGE(FRAGMENT_SHADER_SAMPLED, REGULAR_2D, textures)
DAXA_DECL_TASK_HEAD_END

DAXA_DECL_TASK_HEAD_BEGIN(BoxDraw)
DAXA_TH_BUFFER_PTR(VERTEX_SHADER_READ, daxa_BufferPtr(FrameConstants), frame_constants)
DAXA_TH_BUFFER_PTR(VERTEX_SHADER_READ, daxa_BufferPtr(GpuModelManifest), model_manifests)
DAXA_TH_BUFFER(FRAGMENT_SHADER_READ, model_voxels)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, color_target)
DAXA_TH_IMAGE(DEPTH_ATTACHMENT, REGULAR_2D, depth_target)
DAXA_DECL_TASK_HEAD_END

struct TriDrawPush {
    daxa_BufferPtr(FrameConstants) frame_constants;
    daxa_BufferPtr(MyVertex) vertices;
    daxa_f32vec4 overlay;
};

struct BoxDrawPush {
    daxa_BufferPtr(FrameConstants) frame_constants;
    daxa_BufferPtr(GpuModelManifest) model_manifests;
};

struct VoxelizeDrawPush {
    daxa_BufferPtr(FrameConstants) frame_constants;
    daxa_BufferPtr(GpuModelManifest) model_manifests;
    daxa_BufferPtr(MyVertex) vertices;
    daxa_BufferPtr(MyVertex) processed_vertices;
    daxa_u32 triangle_count;
};
