
#define select(cond, a, b) mix(b, a, cond)

float sRGB_OETF(float a) {
    return select(.0031308f >= a, 12.92f * a, 1.055f * pow(a, .4166666666666667f) - .055f);
}

vec3 sRGB_OETF(vec3 a) {
    return vec3(sRGB_OETF(a.r), sRGB_OETF(a.g), sRGB_OETF(a.b));
}

float sRGB_EOTF(float a) {
    return select(.04045f < a, pow((a + .055f) / 1.055f, 2.4f), a / 12.92f);
}

vec3 sRGB_EOTF(vec3 a) {
    return vec3(sRGB_EOTF(a.r), sRGB_EOTF(a.g), sRGB_EOTF(a.b));
}

uint baseHash(uvec3 p) {
    p = 1103515245U * ((p.xyz >> 1U) ^ (p.yzx));
    uint h32 = 1103515245U * ((p.x ^ p.z) ^ (p.y >> 3U));
    return h32 ^ (h32 >> 16);
}
uint baseHash(uint p) {
    p = 1103515245U * ((p >> 1U) ^ (p));
    uint h32 = 1103515245U * ((p) ^ (p >> 3U));
    return h32 ^ (h32 >> 16);
}
vec3 hash31(uint x) {
    uint n = baseHash(x);
    uvec3 rz = uvec3(n, n * 16807U, n * 48271U); // see: http://random.mat.sbg.ac.at/results/karl/server/node4.html
    return vec3((rz >> 1) & uvec3(0x7fffffffU)) / float(0x7fffffff);
}

uint pack_color(vec4 c) {
    uint r = uint(sRGB_OETF(c.r) * 255);
    uint g = uint(sRGB_OETF(c.g) * 255);
    uint b = uint(sRGB_OETF(c.b) * 255);
    uint a = uint(c.a * 255);

    uint result = 0;
    result = result | (r << 0x00);
    result = result | (g << 0x08);
    result = result | (b << 0x10);
    result = result | (a << 0x18);

    return result;
}

vec4 unpack_color(uint color) {
    return vec4(
        sRGB_EOTF(float((color >> 0x00) & 0xff) / 255.0),
        sRGB_EOTF(float((color >> 0x08) & 0xff) / 255.0),
        sRGB_EOTF(float((color >> 0x10) & 0xff) / 255.0),
        float((color >> 0x10) & 0xff) / 255.0);
}
