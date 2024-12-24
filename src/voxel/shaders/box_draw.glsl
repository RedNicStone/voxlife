#include <daxa/daxa.inl>

#include <shared.inl>

DAXA_DECL_PUSH_CONSTANT(BoxDrawPush, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

layout(location = 0) out uint v_model_id;
layout(location = 1) out vec3 v_pos;
layout(location = 2) out vec3 v_center;
void main() {
    v_model_id = gl_InstanceIndex;

    daxa_BufferPtr(GpuModelManifest) manifest_ptr = advance(push.model_manifests, gl_InstanceIndex);

    vec3 center_ws = (deref(manifest_ptr).aabb_max + deref(manifest_ptr).aabb_min) / 2;
    vec3 diff = (deref(manifest_ptr).aabb_max - deref(manifest_ptr).aabb_min);
    vec3 camera_pos_ws = (deref(push.frame_constants).view_to_world * vec4(0, 0, 0, 1)).xyz;

    bool inside_aabb = all(lessThan(vec3(camera_pos_ws), deref(manifest_ptr).aabb_max)) &&
                       all(greaterThanEqual(vec3(camera_pos_ws), deref(manifest_ptr).aabb_min));

    if (inside_aabb) {
        vec2 sign_ = vec2((ivec2(0xFC, 0x06) >> gl_VertexIndex) & ivec2(1)) * 2 - 1;

        vec4 cs_pos = vec4(sign_, 0, 1);
        gl_Position = cs_pos;

        vec4 vs_pos_h = deref(push.frame_constants).clip_to_view * cs_pos;
        vec4 ws_pos_h = deref(push.frame_constants).view_to_world * vs_pos_h;

        vec3 hit_pos = ws_pos_h.xyz / ws_pos_h.w;
        v_pos = hit_pos; // camera_pos_ws + normalize(hit_pos - camera_pos_ws);
    } else {
        // extracting the vertex offset relative to the center.
        // Thanks to @cantaslaus on Discord.
        //   NOTE(grundlett) Description updated to show I tried using TRIANGLE_STRIP and degenerate triangles to
        // emulate primitive restart instead of instancing. Throughput numbers for both appear to be the same,
        // however, the STRIP mode has more and duplicate work to do. I assume this is why it did not improve perf
        // and instead was about a sizeable regression (0.92ms vs 1.32ms)
        //  axis |          x     |          y     |          z     |
        //  bits |     '0001'1100 |     '0100'0110 |     '0111'0000 | (instancing) ivec3(0x01C, 0x046, 0x070)
        //       | 1110'0000'1011 | 0000'0101'1000 | 0010'1100'0000 | (degenerate) ivec3(0xE0B, 0x058, 0x2C0)
        vec3 sign_ = vec3(ivec3(greaterThan(camera_pos_ws, center_ws)) ^ ((ivec3(0x1C, 0x46, 0x70) >> gl_VertexIndex) & ivec3(1))) - 0.5;
        vec3 vert_pos = sign_ * diff + center_ws;
        // ---------------------

        vec4 vs_pos = deref(push.frame_constants).world_to_view * vec4(vert_pos, 1);
        vec4 cs_pos = deref(push.frame_constants).view_to_clip * vs_pos;
        gl_Position = cs_pos;

        v_pos = vert_pos;
    }

    v_center = center_ws;
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT
#include "common.glsl"

layout(location = 0) in flat uint v_model_id;
layout(location = 1) in vec3 v_pos;
layout(location = 2) in vec3 v_center;
layout(location = 0) out vec4 color;

out float gl_FragDepth;

float sdSphere(vec3 p, float d) { return length(p) - d; }

float sdBox(vec3 p, vec3 b) {
    vec3 d = abs(p) - b;
    return min(max(d.x, max(d.y, d.z)), 0.0) +
           length(max(d, 0.0));
}

vec3 voxel_offset;
daxa_BufferPtr(GpuModelManifest) manifest_ptr;

uint get_voxel_index(ivec3 c) {
    uvec3 volume_extent = uvec3(deref(manifest_ptr).aabb_max - deref(manifest_ptr).aabb_min);
    ivec3 aabb_min = ivec3(floor(deref(manifest_ptr).aabb_min));
    uvec3 vp = uvec3(clamp(ivec3(c - aabb_min), ivec3(0), ivec3(volume_extent - 1)));
    return vp.x + vp.y * volume_extent.x + vp.z * volume_extent.x * volume_extent.y;
}

bool getVoxel(ivec3 c, in out Voxel voxel) {
    uint index = get_voxel_index(c);
    Voxel v = deref_i(deref(manifest_ptr).voxels, index);
    voxel = v;
    return (v.color >> 0x18) == 255;
}

const int MAX_RAY_STEPS = 256 * 3;
float raytrace(vec3 rayPos, vec3 rayDir, vec3 aabb_min, vec3 aabb_max, in out vec3 nrm, in out Voxel voxel) {
    ivec3 mapPos = ivec3(floor(rayPos));
    vec3 deltaDist = abs(vec3(length(rayDir)) / rayDir);
    ivec3 rayStep = ivec3(sign(rayDir));
    vec3 sideDist = (sign(rayDir) * (vec3(mapPos) - rayPos) + (sign(rayDir) * 0.5) + 0.5) * deltaDist;
    bvec3 mask = bvec3(nrm);
    ivec3 deltaStep = ivec3(0);
    for (int i = 0; i < MAX_RAY_STEPS; i++) {
        if (any(greaterThanEqual(vec3(mapPos), aabb_max + 1)) || any(lessThan(vec3(mapPos), aabb_min - 1)))
            break;
        vec3 testSideDist = vec3(deltaStep) * deltaDist + sideDist;
        if (getVoxel(mapPos, voxel)) {
            nrm = vec3(mask) * -sign(rayDir);
            return dot(testSideDist, vec3(mask));
        }
        mask = lessThanEqual(testSideDist.xyz, min(testSideDist.yzx, testSideDist.zxy));
        deltaStep += ivec3(mask);
        mapPos += ivec3(mask) * rayStep;
    }

    return -1;
}

struct Box {
    vec3 center;
    vec3 radius;
    vec3 invRadius;
};
struct Ray {
    vec3 origin;
    vec3 direction;
};

vec3 boxNormal(Box box, Ray ray, in vec3 _invRayDir) {
    ray.origin = ray.origin - box.center;
    float winding = 1;
    vec3 sgn = -sign(ray.direction);
    // Distance to plane
    vec3 d = box.radius * winding * sgn - ray.origin;
    d *= _invRayDir;
#define TEST(U, VW) (d.U >= 0.0) && all(lessThan(abs(ray.origin.VW + ray.direction.VW * d.U), box.radius.VW))
    bvec3 test = bvec3(TEST(x, yz), TEST(y, zx), TEST(z, xy));
#undef TEST
    sgn = test.x ? vec3(sgn.x, 0, 0) : (test.y ? vec3(0, sgn.y, 0) : vec3(0, 0, test.z ? sgn.z : 0));
    return sgn;
}

void main() {
    // vec3 c = vec3(1);
    // vec3 c = tex_col.rgb;
    // vec3 c = hash31(v_model_id);
    vec3 c = fract(v_pos / 10);

    voxel_offset = hash31(v_model_id) * 10;

    manifest_ptr = advance(push.model_manifests, v_model_id);
    vec3 box_radius = (deref(manifest_ptr).aabb_max - deref(manifest_ptr).aabb_min) / 2;
    vec3 center_ws = (deref(manifest_ptr).aabb_max + deref(manifest_ptr).aabb_min) / 2;

    vec3 camera_pos_ws = (deref(push.frame_constants).view_to_world * vec4(0, 0, 0, 1)).xyz;
    vec3 to_hit_pos = normalize(v_pos - camera_pos_ws);
    // vec3 c = vec3(0);

    vec3 ray_origin = v_pos;
    vec3 ray_direction = to_hit_pos;
    vec3 nrm = boxNormal(Box(center_ws, box_radius, vec3(1) / box_radius), Ray(ray_origin - ray_direction, ray_direction), vec3(1) / ray_direction);

    Voxel voxel;
    float dist = raytrace(ray_origin, ray_direction, deref(manifest_ptr).aabb_min, deref(manifest_ptr).aabb_max, nrm, voxel);
    if (dist == -1)
        discard;

    vec3 light = vec3(1); // vec3(dot(nrm, normalize(vec3(1, 2, 3))) * 0.5 + 0.5);
    // vec3 albedo = hash31(v_model_id);
    vec3 albedo = unpack_color(voxel.color).rgb;
    c = light * albedo;

    vec3 ws_pos = ray_origin + ray_direction * dist;
    vec4 vs_pos = deref(push.frame_constants).world_to_view * vec4(ws_pos, 1);
    vec4 cs_pos = deref(push.frame_constants).view_to_clip * vs_pos;
    float ndc_depth = cs_pos.z / cs_pos.w;
    gl_FragDepth = ndc_depth;

    color = daxa_f32vec4(sRGB_OETF(c), 1);
}

#endif
