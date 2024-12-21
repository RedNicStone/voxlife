#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

struct MyVertex {
    daxa_f32vec3 position;
    daxa_f32vec2 uv;
    daxa_u32 model_id;
    daxa_ImageViewIndex tex_id;
};
DAXA_DECL_BUFFER_PTR(MyVertex)

struct PackedVoxel {
    daxa_u32 color;
};

struct Voxel {
    daxa_f32vec3 color;
};

struct FrameConstants {
    daxa_f32mat4x4 world_to_view;
    daxa_f32mat4x4 view_to_clip;
    daxa_SamplerId sampler_llr;
};
DAXA_DECL_BUFFER_PTR(FrameConstants)

DAXA_DECL_TASK_HEAD_BEGIN(DrawToSwapchainH)
DAXA_TH_BUFFER_PTR(VERTEX_SHADER_READ, daxa_BufferPtr(MyVertex), vertices)
DAXA_TH_BUFFER_PTR(VERTEX_SHADER_READ, daxa_BufferPtr(FrameConstants), frame_constants)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, color_target)
DAXA_TH_IMAGE(DEPTH_ATTACHMENT, REGULAR_2D, depth_target)
DAXA_TH_IMAGE(FRAGMENT_SHADER_SAMPLED, REGULAR_2D, textures)
DAXA_DECL_TASK_HEAD_END

struct MyPushConstant {
    DAXA_TH_BLOB(DrawToSwapchainH, attachments)
    daxa_f32vec4 overlay;
};
