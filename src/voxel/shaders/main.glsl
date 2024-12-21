#include <daxa/daxa.inl>

#include <shared.inl>

DAXA_DECL_PUSH_CONSTANT(MyPushConstant, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

layout(location = 0) out daxa_f32vec2 v_uv;
layout(location = 1) out uint v_tex_id;
layout(location = 2) out uint v_model_id;
void main() {
    daxa_BufferPtr(MyVertex) vertices_ptr = push.attachments.vertices;
    MyVertex vert = deref_i(push.attachments.vertices, gl_VertexIndex);
    daxa_BufferPtr(FrameConstants) frame_constants = push.attachments.frame_constants;

    vec4 vs_pos = deref(frame_constants).world_to_view * daxa_f32vec4(vert.position, 1);
    vec4 cs_pos = deref(frame_constants).view_to_clip * vs_pos;
    if (push.overlay.a != 0)
        cs_pos.z *= 0.999999;
    gl_Position = cs_pos;
    v_uv = vert.uv;
    v_tex_id = vert.tex_id.value;
    v_model_id = vert.model_id;
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

#define select(cond, a, b) mix(b, a, cond)

float sRGB_OETF(float a) {
    return select(.0031308f >= a, 12.92f * a, 1.055f * pow(a, .4166666666666667f) - .055f);
}

vec3 sRGB_OETF(vec3 a) {
    return vec3(sRGB_OETF(a.r), sRGB_OETF(a.g), sRGB_OETF(a.b));
}

uint baseHash(uvec3 p)
{
    p = 1103515245U*((p.xyz >> 1U)^(p.yzx));
    uint h32 = 1103515245U*((p.x^p.z)^(p.y>>3U));
    return h32^(h32 >> 16);
}
uint baseHash(uint p)
{
    p = 1103515245U*((p >> 1U)^(p));
    uint h32 = 1103515245U*((p)^(p>>3U));
    return h32^(h32 >> 16);
}
vec3 hash31(uint x)
{
    uint n = baseHash(x);
    uvec3 rz = uvec3(n, n*16807U, n*48271U); //see: http://random.mat.sbg.ac.at/results/karl/server/node4.html
    return vec3((rz >> 1) & uvec3(0x7fffffffU))/float(0x7fffffff);
}

layout(location = 0) in daxa_f32vec2 v_uv;
layout(location = 1) in flat uint v_tex_id;
layout(location = 2) in flat uint v_model_id;
layout(location = 0) out daxa_f32vec4 color;
void main() {
    daxa_BufferPtr(FrameConstants) frame_constants = push.attachments.frame_constants;
    daxa_ImageViewId tex = daxa_ImageViewId(v_tex_id);
    vec4 tex_col = texture(daxa_sampler2D(tex, deref(frame_constants).sampler_llr), v_uv);

    // vec3 c = tex_col.rgb;
    // vec3 c = vec3(fract(v_uv), 0);
    // vec3 c = hash31(v_tex_id);
    vec3 c = hash31(v_model_id);
    c = c * 0.6 + tex_col.rgb * 0.4;
    c = mix(c, push.overlay.rgb, push.overlay.a * 2);
    color = daxa_f32vec4(sRGB_OETF(c), 1);
}

#endif
