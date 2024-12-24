#include <daxa/daxa.inl>

#include <shared.inl>

DAXA_DECL_PUSH_CONSTANT(TriDrawPush, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

layout(location = 0) out daxa_f32vec2 v_uv;
layout(location = 1) out uint v_tex_id;
layout(location = 2) out uint v_model_id;
void main() {
    MyVertex vert = deref_i(push.vertices, gl_VertexIndex);
    vec4 vs_pos = deref(push.frame_constants).world_to_view * daxa_f32vec4(vert.pos, 1);
    vec4 cs_pos = deref(push.frame_constants).view_to_clip * vs_pos;
    if (push.overlay.a != 0)
        cs_pos.z *= 0.999999;
    gl_Position = cs_pos;
    v_uv = vert.uv;
    v_tex_id = vert.tex_id.value;
    v_model_id = vert.model_id;
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

#include "common.glsl"

layout(location = 0) in daxa_f32vec2 v_uv;
layout(location = 1) in flat uint v_tex_id;
layout(location = 2) in flat uint v_model_id;
layout(location = 0) out vec4 color;
void main() {
    daxa_ImageViewId tex = daxa_ImageViewId(v_tex_id);
    vec4 tex_col = texture(daxa_sampler2D(tex, deref(push.frame_constants).sampler_llr), v_uv);
    vec3 c = tex_col.rgb;
    c = mix(c, push.overlay.rgb, push.overlay.a * 2);
    color = vec4(sRGB_OETF(c), 1);
}

#endif
