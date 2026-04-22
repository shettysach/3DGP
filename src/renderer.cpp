#include "renderer.h"
#include "terrain/biomes.h"
#include "terrain/util.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace renderer {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr int kMaterialTextureSize = 256;
constexpr int kWaterTextureSize = 256;
constexpr int kShadowMapSize = 2048;
constexpr float kCameraFovDeg = 60.0f;
constexpr float kCameraNear = 0.5f;
constexpr float kCameraFar = 5000.0f;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Mat4 {
    float m[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
};

struct Color3 {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

struct TerrainGpuVertex {
    float position[3];
    float normal[3];
    float baseColor[3];
    float params0[4];
    float params1[4];
};

struct WaterGpuVertex {
    float position[3];
    float params[4];
};

namespace glfn {
PFNGLGENVERTEXARRAYSPROC GenVertexArrays = nullptr;
PFNGLBINDVERTEXARRAYPROC BindVertexArray = nullptr;
PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays = nullptr;
PFNGLGENBUFFERSPROC GenBuffers = nullptr;
PFNGLBINDBUFFERPROC BindBuffer = nullptr;
PFNGLBUFFERDATAPROC BufferData = nullptr;
PFNGLDELETEBUFFERSPROC DeleteBuffers = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer = nullptr;
PFNGLCREATESHADERPROC CreateShader = nullptr;
PFNGLSHADERSOURCEPROC ShaderSource = nullptr;
PFNGLCOMPILESHADERPROC CompileShader = nullptr;
PFNGLGETSHADERIVPROC GetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog = nullptr;
PFNGLDELETESHADERPROC DeleteShader = nullptr;
PFNGLCREATEPROGRAMPROC CreateProgram = nullptr;
PFNGLATTACHSHADERPROC AttachShader = nullptr;
PFNGLLINKPROGRAMPROC LinkProgram = nullptr;
PFNGLGETPROGRAMIVPROC GetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog = nullptr;
PFNGLUSEPROGRAMPROC UseProgram = nullptr;
PFNGLDELETEPROGRAMPROC DeleteProgram = nullptr;
PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation = nullptr;
PFNGLUNIFORMMATRIX4FVPROC UniformMatrix4fv = nullptr;
PFNGLUNIFORM1IPROC Uniform1i = nullptr;
PFNGLUNIFORM1FPROC Uniform1f = nullptr;
PFNGLUNIFORM3FPROC Uniform3f = nullptr;
PFNGLACTIVETEXTUREPROC ActiveTexture = nullptr;
PFNGLGENERATEMIPMAPPROC GenerateMipmap = nullptr;
PFNGLGENFRAMEBUFFERSPROC GenFramebuffers = nullptr;
PFNGLBINDFRAMEBUFFERPROC BindFramebuffer = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers = nullptr;

bool load() {
    auto loadSymbol = [](const char* name) {
        return SDL_GL_GetProcAddress(name);
    };

#define LOAD_GL(symbol)                                                                                              \
    do {                                                                                                             \
        symbol = reinterpret_cast<decltype(symbol)>(loadSymbol("gl" #symbol));                                      \
        if (!(symbol)) {                                                                                             \
            std::cerr << "Failed to load OpenGL symbol gl" #symbol << '\n';                                         \
            return false;                                                                                            \
        }                                                                                                            \
    } while (false)

    LOAD_GL(GenVertexArrays);
    LOAD_GL(BindVertexArray);
    LOAD_GL(DeleteVertexArrays);
    LOAD_GL(GenBuffers);
    LOAD_GL(BindBuffer);
    LOAD_GL(BufferData);
    LOAD_GL(DeleteBuffers);
    LOAD_GL(EnableVertexAttribArray);
    LOAD_GL(VertexAttribPointer);
    LOAD_GL(CreateShader);
    LOAD_GL(ShaderSource);
    LOAD_GL(CompileShader);
    LOAD_GL(GetShaderiv);
    LOAD_GL(GetShaderInfoLog);
    LOAD_GL(DeleteShader);
    LOAD_GL(CreateProgram);
    LOAD_GL(AttachShader);
    LOAD_GL(LinkProgram);
    LOAD_GL(GetProgramiv);
    LOAD_GL(GetProgramInfoLog);
    LOAD_GL(UseProgram);
    LOAD_GL(DeleteProgram);
    LOAD_GL(GetUniformLocation);
    LOAD_GL(UniformMatrix4fv);
    LOAD_GL(Uniform1i);
    LOAD_GL(Uniform1f);
    LOAD_GL(Uniform3f);
    LOAD_GL(ActiveTexture);
    LOAD_GL(GenerateMipmap);
    LOAD_GL(GenFramebuffers);
    LOAD_GL(BindFramebuffer);
    LOAD_GL(FramebufferTexture2D);
    LOAD_GL(CheckFramebufferStatus);
    LOAD_GL(DeleteFramebuffers);

#undef LOAD_GL
    return true;
}
} // namespace glfn

constexpr Vec3 kSunDirection = {-0.42f, -1.0f, -0.28f};
constexpr Vec3 kSunColor = {1.12f, 1.03f, 0.92f};
constexpr Vec3 kSkyAmbientColor = {0.34f, 0.43f, 0.53f};
constexpr Vec3 kGroundAmbientColor = {0.10f, 0.09f, 0.08f};
constexpr Vec3 kSkyZenithColor = {0.21f, 0.40f, 0.70f};
constexpr Vec3 kSkyHorizonColor = {0.77f, 0.88f, 0.97f};
constexpr Vec3 kFogHorizonColor = {0.76f, 0.84f, 0.92f};
constexpr Vec3 kFogZenithColor = {0.48f, 0.63f, 0.82f};
constexpr Vec3 kFogSunColor = {0.90f, 0.72f, 0.50f};

constexpr char kTerrainVertexShader[] = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aBaseColor;
layout(location = 3) in vec4 aParams0;
layout(location = 4) in vec4 aParams1;

uniform mat4 uViewProj;
uniform mat4 uLightViewProj;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vBaseColor;
out vec4 vParams0;
out vec4 vParams1;
out vec4 vShadowCoord;

void main() {
    vec4 worldPos = vec4(aPosition, 1.0);
    vWorldPos = aPosition;
    vNormal = aNormal;
    vBaseColor = aBaseColor;
    vParams0 = aParams0;
    vParams1 = aParams1;
    vShadowCoord = uLightViewProj * worldPos;
    gl_Position = uViewProj * worldPos;
}
)glsl";

constexpr char kTerrainFragmentShader[] = R"glsl(#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vBaseColor;
in vec4 vParams0;
in vec4 vParams1;
in vec4 vShadowCoord;

uniform sampler2D uGrassTex;
uniform sampler2D uRockTex;
uniform sampler2D uSandTex;
uniform sampler2D uSnowTex;
uniform sampler2D uShadowMap;
uniform vec3 uCameraPos;
uniform vec3 uSunLightDir;
uniform vec3 uSunColor;
uniform vec3 uSkyAmbientColor;
uniform vec3 uGroundAmbientColor;
uniform vec3 uFogHorizonColor;
uniform vec3 uFogZenithColor;
uniform vec3 uFogSunColor;
uniform float uFogDensity;
uniform float uFogHeightFalloff;
uniform float uFogBaseHeight;
uniform float uShadowTexelSize;
uniform int uEnableMaterials;
uniform int uEnableShadows;

out vec4 fragColor;

float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

vec3 toneMap(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 toDisplay(vec3 color) {
    return pow(toneMap(color), vec3(1.0 / 2.2));
}

vec3 triplanarSample(sampler2D tex, vec3 pos, vec3 normal, float scale) {
    vec3 weights = abs(normal);
    weights = pow(weights, vec3(6.0));
    weights /= max(weights.x + weights.y + weights.z, 0.0001);

    vec3 sampleX = texture(tex, pos.yz * scale).rgb;
    vec3 sampleY = texture(tex, pos.xz * scale).rgb;
    vec3 sampleZ = texture(tex, pos.xy * scale).rgb;
    return sampleX * weights.x + sampleY * weights.y + sampleZ * weights.z;
}

float sampleShadow(vec4 shadowCoord, vec3 normal, vec3 lightDir) {
    if (uEnableShadows == 0) {
        return 1.0;
    }

    vec3 proj = shadowCoord.xyz / max(shadowCoord.w, 0.0001);
    proj = proj * 0.5 + 0.5;
    if (proj.z <= 0.0 || proj.z >= 1.0) {
        return 1.0;
    }
    if (proj.x <= 0.0 || proj.x >= 1.0 || proj.y <= 0.0 || proj.y >= 1.0) {
        return 1.0;
    }

    float ndotl = saturate(dot(normal, lightDir));
    float bias = max(0.0013 * (1.0 - ndotl), 0.00018);
    float lit = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(float(x), float(y)) * uShadowTexelSize;
            float depth = texture(uShadowMap, proj.xy + offset).r;
            lit += proj.z - bias <= depth ? 1.0 : 0.0;
        }
    }
    return lit / 9.0;
}

vec3 fogColor(vec3 viewDir) {
    float horizonMix = saturate(pow(viewDir.y * 0.5 + 0.5, 0.65));
    float sunScatter = pow(max(dot(-viewDir, uSunLightDir), 0.0), 8.0);
    vec3 fog = mix(uFogHorizonColor, uFogZenithColor, horizonMix);
    fog += uFogSunColor * sunScatter * 0.20;
    return fog;
}

void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(uSunLightDir);
    vec3 viewDir = normalize(vWorldPos - uCameraPos);
    vec3 viewToCamera = normalize(uCameraPos - vWorldPos);

    float slope = vParams0.x;
    float mountain = vParams0.y;
    float river = vParams0.z;
    float moisture = vParams0.w;
    float heightN = vParams1.x;
    float temperature = vParams1.y;
    float precipitation = vParams1.z;

    vec3 albedo = vBaseColor;
    if (uEnableMaterials == 1) {
        float rock = smoothstep(0.38, 0.92, slope + mountain * 0.34 + heightN * 0.10);
        float snow = smoothstep(0.76, 0.99, heightN + (1.0 - temperature) * 0.28 + slope * 0.04) *
                     smoothstep(0.0, 0.55, 1.0 - moisture * 0.86);
        float sand = smoothstep(0.08, 0.48, river) *
                     smoothstep(0.0, 0.62, 1.0 - heightN) *
                     smoothstep(0.0, 0.62, 1.0 - moisture);
        float grass = clamp(1.0 - max(rock, snow), 0.0, 1.0) * (1.0 - sand * 0.72);

        float weightSum = max(grass + rock + sand + snow, 0.0001);
        grass /= weightSum;
        rock /= weightSum;
        sand /= weightSum;
        snow /= weightSum;

        vec3 grassDetail = texture(uGrassTex, vWorldPos.xz * 0.052).rgb;
        vec3 sandDetail = texture(uSandTex, vWorldPos.xz * 0.067).rgb;
        vec3 rockDetail = triplanarSample(uRockTex, vWorldPos, normal, 0.085);
        vec3 snowDetail = triplanarSample(uSnowTex, vWorldPos, normal, 0.042);

        vec3 fertileTint = mix(vec3(0.66, 0.58, 0.40), vec3(0.82, 0.93, 0.70), saturate(moisture * 0.62 + precipitation * 0.38));
        vec3 grassMaterial = grassDetail * mix(fertileTint, vBaseColor * 1.10, 0.42);
        vec3 rockMaterial = rockDetail * mix(vec3(0.66, 0.67, 0.69), vBaseColor * 1.08, 0.20);
        vec3 sandMaterial = sandDetail * mix(vec3(0.92, 0.84, 0.62), vBaseColor * 1.03, 0.12);
        vec3 snowMaterial = snowDetail * vec3(0.93, 0.97, 1.02);

        albedo = grassMaterial * grass + rockMaterial * rock + sandMaterial * sand + snowMaterial * snow;
    }

    float diffuse = saturate(dot(normal, lightDir));
    float shadow = sampleShadow(vShadowCoord, normal, lightDir);
    vec3 hemiAmbient = mix(uGroundAmbientColor, uSkyAmbientColor, normal.y * 0.5 + 0.5);
    vec3 lit = albedo * (hemiAmbient + uSunColor * diffuse * shadow);

    if (uEnableMaterials == 1) {
        vec3 halfVector = normalize(viewToCamera + lightDir);
        float spec = pow(saturate(dot(normal, halfVector)), 30.0 + heightN * 34.0);
        float specStrength = mix(0.02, 0.07, saturate(vParams0.z * 0.35 + (1.0 - temperature) * 0.20 + heightN * 0.18));
        lit += uSunColor * spec * specStrength * shadow;
    }

    float distanceToCamera = length(uCameraPos - vWorldPos);
    float lowAltitudeFog = exp(-max(vWorldPos.y - uFogBaseHeight, 0.0) * uFogHeightFalloff);
    float fogAmount = 1.0 - exp(-distanceToCamera * uFogDensity) * lowAltitudeFog;
    fogAmount = saturate(fogAmount + river * 0.05);

    vec3 atmosphere = fogColor(viewDir);
    vec3 finalColor = mix(lit, atmosphere, fogAmount);
    fragColor = vec4(toDisplay(finalColor), 1.0);
}
)glsl";

constexpr char kWaterVertexShader[] = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aParams;

uniform mat4 uViewProj;
uniform mat4 uLightViewProj;

out vec3 vWorldPos;
out vec4 vParams;
out vec4 vShadowCoord;

void main() {
    vec4 worldPos = vec4(aPosition, 1.0);
    vWorldPos = aPosition;
    vParams = aParams;
    vShadowCoord = uLightViewProj * worldPos;
    gl_Position = uViewProj * worldPos;
}
)glsl";

constexpr char kWaterFragmentShader[] = R"glsl(#version 330 core
in vec3 vWorldPos;
in vec4 vParams;
in vec4 vShadowCoord;

uniform sampler2D uWaterTex;
uniform sampler2D uShadowMap;
uniform vec3 uCameraPos;
uniform vec3 uSunLightDir;
uniform vec3 uSunColor;
uniform vec3 uSkyZenithColor;
uniform vec3 uFogHorizonColor;
uniform vec3 uFogZenithColor;
uniform vec3 uFogSunColor;
uniform float uFogDensity;
uniform float uFogHeightFalloff;
uniform float uFogBaseHeight;
uniform float uShadowTexelSize;
uniform float uTime;

out vec4 fragColor;

float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

vec3 toneMap(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 toDisplay(vec3 color) {
    return pow(toneMap(color), vec3(1.0 / 2.2));
}

float sampleShadow(vec4 shadowCoord, vec3 normal, vec3 lightDir) {
    vec3 proj = shadowCoord.xyz / max(shadowCoord.w, 0.0001);
    proj = proj * 0.5 + 0.5;
    if (proj.z <= 0.0 || proj.z >= 1.0) {
        return 1.0;
    }
    if (proj.x <= 0.0 || proj.x >= 1.0 || proj.y <= 0.0 || proj.y >= 1.0) {
        return 1.0;
    }

    float ndotl = saturate(dot(normal, lightDir));
    float bias = max(0.0015 * (1.0 - ndotl), 0.0002);
    float lit = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(float(x), float(y)) * uShadowTexelSize;
            float depth = texture(uShadowMap, proj.xy + offset).r;
            lit += proj.z - bias <= depth ? 1.0 : 0.0;
        }
    }
    return lit / 9.0;
}

vec3 fogColor(vec3 viewDir) {
    float horizonMix = saturate(pow(viewDir.y * 0.5 + 0.5, 0.65));
    float sunScatter = pow(max(dot(-viewDir, uSunLightDir), 0.0), 8.0);
    vec3 fog = mix(uFogHorizonColor, uFogZenithColor, horizonMix);
    fog += uFogSunColor * sunScatter * 0.20;
    return fog;
}

vec3 waterNormal(vec2 uv) {
    vec3 n1 = texture(uWaterTex, uv + vec2(uTime * 0.038, uTime * 0.021)).rgb * 2.0 - 1.0;
    vec3 n2 = texture(uWaterTex, uv * 1.7 + vec2(-uTime * 0.029, uTime * 0.034)).rgb * 2.0 - 1.0;
    vec2 slope = vec2(n1.r + n2.g, n1.g + n2.b) - 1.0;
    return normalize(vec3(slope.x * 0.38, 1.0, slope.y * 0.38));
}

void main() {
    float river = vParams.x;
    float slope = vParams.y;
    float heightN = vParams.z;
    float foamFactor = vParams.w;

    vec2 uv = vWorldPos.xz * 0.12;
    vec3 normal = waterNormal(uv);
    vec3 lightDir = normalize(uSunLightDir);
    vec3 toCamera = normalize(uCameraPos - vWorldPos);
    vec3 viewDir = normalize(vWorldPos - uCameraPos);

    float fresnel = pow(1.0 - saturate(dot(normal, toCamera)), 5.0);
    float depthLike = saturate(river * 1.18 + heightN * 0.12);
    float shadow = sampleShadow(vShadowCoord, normal, lightDir);

    vec3 shallowColor = vec3(0.12, 0.38, 0.44);
    vec3 deepColor = vec3(0.03, 0.16, 0.25);
    vec3 transmission = mix(shallowColor, deepColor, depthLike);
    vec3 reflection = mix(uFogHorizonColor, uSkyZenithColor, saturate(normal.y * 0.5 + 0.5));
    vec3 waterColor = mix(transmission, reflection, fresnel * 0.78 + 0.16);

    float diffuse = saturate(dot(normal, lightDir));
    vec3 halfVector = normalize(toCamera + lightDir);
    float specular = pow(saturate(dot(normal, halfVector)), 80.0) * (0.18 + depthLike * 0.12);
    float foamNoise = texture(uWaterTex, uv * 1.85 + vec2(uTime * 0.08, -uTime * 0.05)).r;
    float foam = saturate(foamFactor * (0.55 + foamNoise * 0.65));

    vec3 lit = waterColor * (0.22 + diffuse * shadow * 0.36);
    lit += uSunColor * specular * shadow;
    lit += vec3(0.96, 0.98, 1.0) * foam * (0.38 + slope * 0.16);

    float distanceToCamera = length(uCameraPos - vWorldPos);
    float lowAltitudeFog = exp(-max(vWorldPos.y - uFogBaseHeight, 0.0) * uFogHeightFalloff);
    float fogAmount = 1.0 - exp(-distanceToCamera * (uFogDensity * 1.1)) * lowAltitudeFog;
    fogAmount = saturate(fogAmount + depthLike * 0.06);

    vec3 finalColor = mix(lit, fogColor(viewDir), fogAmount);
    float alpha = saturate(0.44 + depthLike * 0.28 + fresnel * 0.14);
    fragColor = vec4(toDisplay(finalColor), alpha);
}
)glsl";

constexpr char kShadowVertexShader[] = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;

uniform mat4 uLightViewProj;

void main() {
    gl_Position = uLightViewProj * vec4(aPosition, 1.0);
}
)glsl";

constexpr char kShadowFragmentShader[] = R"glsl(#version 330 core
void main() {}
)glsl";

constexpr char kSkyVertexShader[] = R"glsl(#version 330 core
out vec2 vNdc;

void main() {
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2(3.0, -1.0),
        vec2(-1.0, 3.0)
    );
    vec2 pos = positions[gl_VertexID];
    vNdc = pos;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)glsl";

constexpr char kSkyFragmentShader[] = R"glsl(#version 330 core
in vec2 vNdc;

uniform vec3 uCameraForward;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
uniform vec3 uSunLightDir;
uniform vec3 uSunColor;
uniform vec3 uSkyZenithColor;
uniform vec3 uSkyHorizonColor;
uniform vec3 uFogHorizonColor;
uniform float uAspect;
uniform float uTanHalfFov;

out vec4 fragColor;

vec3 toneMap(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 toDisplay(vec3 color) {
    return pow(toneMap(color), vec3(1.0 / 2.2));
}

void main() {
    vec3 rayDir = normalize(
        uCameraForward +
        vNdc.x * uCameraRight * uAspect * uTanHalfFov +
        vNdc.y * uCameraUp * uTanHalfFov);

    float horizonMix = clamp(pow(rayDir.y * 0.5 + 0.5, 0.60), 0.0, 1.0);
    vec3 sky = mix(uSkyHorizonColor, uSkyZenithColor, horizonMix);

    float horizonGlow = pow(1.0 - abs(rayDir.y), 5.5);
    sky += uFogHorizonColor * horizonGlow * 0.08;

    float sunHalo = pow(max(dot(rayDir, uSunLightDir), 0.0), 18.0);
    float sunDisk = pow(max(dot(rayDir, uSunLightDir), 0.0), 900.0);
    sky += uSunColor * (sunHalo * 0.30 + sunDisk * 8.0);

    fragColor = vec4(toDisplay(sky), 1.0);
}
)glsl";

float degToRad(float deg) {
    return deg * kPi / 180.0f;
}

float fract(float x) {
    return x - std::floor(x);
}

float hash2D(int x, int y, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u;
    h += static_cast<uint32_t>(y) * 668265263u;
    h ^= seed * 362437u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= (h >> 16u);
    return static_cast<float>(h & 0x00ffffffu) / 16777215.0f;
}

float valueNoise2D(float x, float y, uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;

    const float tx = fract(x);
    const float ty = fract(y);
    const float sx = tx * tx * (3.0f - 2.0f * tx);
    const float sy = ty * ty * (3.0f - 2.0f * ty);

    const float n00 = hash2D(x0, y0, seed);
    const float n10 = hash2D(x1, y0, seed);
    const float n01 = hash2D(x0, y1, seed);
    const float n11 = hash2D(x1, y1, seed);
    const float nx0 = terrain::lerp(n00, n10, sx);
    const float nx1 = terrain::lerp(n01, n11, sx);
    return terrain::lerp(nx0, nx1, sy);
}

Vec3 operator+(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 operator-(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 operator*(const Vec3& v, float scalar) {
    return {v.x * scalar, v.y * scalar, v.z * scalar};
}

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float length(const Vec3& v) {
    return std::sqrt(dot(v, v));
}

Vec3 normalize(const Vec3& v) {
    const float len = length(v);
    if (len <= 0.000001f) {
        return {0.0f, 1.0f, 0.0f};
    }
    return {v.x / len, v.y / len, v.z / len};
}

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 out{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            out.m[col * 4 + row] = a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                                   a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                                   a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                                   a.m[3 * 4 + row] * b.m[col * 4 + 3];
        }
    }
    return out;
}

Vec3 transformPoint(const Mat4& m, const Vec3& v) {
    return {
        m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12],
        m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13],
        m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14],
    };
}

Mat4 perspective(float fovYRadians, float aspect, float nearPlane, float farPlane) {
    const float f = 1.0f / std::tan(fovYRadians * 0.5f);
    Mat4 out{};
    std::fill(std::begin(out.m), std::end(out.m), 0.0f);
    out.m[0] = f / aspect;
    out.m[5] = f;
    out.m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    out.m[11] = -1.0f;
    out.m[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    return out;
}

Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    const Vec3 forward = normalize(target - eye);
    const Vec3 right = normalize(cross(forward, up));
    const Vec3 cameraUp = cross(right, forward);

    Mat4 out{};
    out.m[0] = right.x;
    out.m[4] = right.y;
    out.m[8] = right.z;
    out.m[12] = -dot(right, eye);

    out.m[1] = cameraUp.x;
    out.m[5] = cameraUp.y;
    out.m[9] = cameraUp.z;
    out.m[13] = -dot(cameraUp, eye);

    out.m[2] = -forward.x;
    out.m[6] = -forward.y;
    out.m[10] = -forward.z;
    out.m[14] = dot(forward, eye);

    out.m[3] = 0.0f;
    out.m[7] = 0.0f;
    out.m[11] = 0.0f;
    out.m[15] = 1.0f;
    return out;
}

Mat4 orthoBox(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
    Mat4 out{};
    std::fill(std::begin(out.m), std::end(out.m), 0.0f);
    out.m[0] = 2.0f / (right - left);
    out.m[5] = 2.0f / (top - bottom);
    out.m[10] = -2.0f / (farPlane - nearPlane);
    out.m[12] = -(right + left) / (right - left);
    out.m[13] = -(top + bottom) / (top - bottom);
    out.m[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    out.m[15] = 1.0f;
    return out;
}

bool usesMaterials(RenderMode mode) {
    return mode == RenderMode::SurfaceBiomes;
}

Color3 lerpColor(const Color3& a, const Color3& b, float t) {
    return {
        terrain::lerp(a.r, b.r, t),
        terrain::lerp(a.g, b.g, t),
        terrain::lerp(a.b, b.b, t),
    };
}

Color3 biomeVertexColor(const terrain::TerrainVertex& v, float minH, float maxH) {
    const float h = (v.y - minH) / std::max(0.001f, maxH - minH);
    const float slope = std::clamp(v.slope, 0.0f, 1.0f);
    const float river = std::clamp(v.riverWeight, 0.0f, 1.0f);
    const float primaryWeight = std::clamp(v.primaryBiomeWeight, 0.0f, 1.0f);
    const float secondaryWeight = std::clamp(v.secondaryBiomeWeight, 0.0f, 1.0f);
    const float blendDenom = std::max(0.0001f, primaryWeight + secondaryWeight);

    const terrain::BiomeColor primaryBiomeColor =
        terrain::biomeColor(static_cast<terrain::BiomeId>(v.primaryBiome));
    const terrain::BiomeColor secondaryBiomeColor =
        terrain::biomeColor(static_cast<terrain::BiomeId>(v.secondaryBiome));
    Color3 color{
        (primaryBiomeColor.r * primaryWeight + secondaryBiomeColor.r * secondaryWeight) / blendDenom,
        (primaryBiomeColor.g * primaryWeight + secondaryBiomeColor.g * secondaryWeight) / blendDenom,
        (primaryBiomeColor.b * primaryWeight + secondaryBiomeColor.b * secondaryWeight) / blendDenom,
    };

    const Color3 rockTint{0.50f, 0.48f, 0.46f};
    const float rockBoost = terrain::smoothstep(0.18f, 0.62f, slope + v.mountainWeight * 0.18f + h * 0.10f);
    color = lerpColor(color, rockTint, rockBoost * 0.24f);

    const float terraceNoise =
        0.5f + 0.5f * std::sin(v.x * 0.048f + v.z * 0.031f) * std::cos(v.x * 0.019f - v.z * 0.043f);
    const float cavity = terrain::smoothstep(0.16f, 0.78f, slope) * (0.62f + 0.38f * (1.0f - h));
    const float riverDarkening = terrain::smoothstep(0.08f, 0.85f, river) * 0.09f;
    const float slopeLightening = terrain::smoothstep(0.05f, 0.28f, 1.0f - slope) * 0.045f;
    const float shade =
        0.84f + h * 0.21f - cavity * 0.18f - riverDarkening + slopeLightening + (terraceNoise - 0.5f) * 0.05f;
    color.r = std::clamp(color.r * shade, 0.0f, 1.0f);
    color.g = std::clamp(color.g * shade, 0.0f, 1.0f);
    color.b = std::clamp(color.b * shade, 0.0f, 1.0f);

    const Color3 coolShadowTint{0.88f, 0.92f, 0.97f};
    const float coolMix = cavity * 0.10f;
    color = lerpColor(color, {color.r * coolShadowTint.r, color.g * coolShadowTint.g, color.b * coolShadowTint.b}, coolMix);
    return color;
}

Color3 heatmapColor(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    if (t < 0.33f) {
        return lerpColor({0.06f, 0.12f, 0.42f}, {0.17f, 0.62f, 0.86f}, t / 0.33f);
    }
    if (t < 0.66f) {
        return lerpColor({0.17f, 0.62f, 0.86f}, {0.90f, 0.82f, 0.24f}, (t - 0.33f) / 0.33f);
    }
    return lerpColor({0.90f, 0.82f, 0.24f}, {0.82f, 0.22f, 0.14f}, (t - 0.66f) / 0.34f);
}

Color3 precipitationColor(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    if (t < 0.5f) {
        return lerpColor({0.74f, 0.61f, 0.38f}, {0.42f, 0.68f, 0.28f}, t / 0.5f);
    }
    return lerpColor({0.42f, 0.68f, 0.28f}, {0.12f, 0.38f, 0.63f}, (t - 0.5f) / 0.5f);
}

Color3 moistureColor(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    if (t < 0.5f) {
        return lerpColor({0.63f, 0.52f, 0.34f}, {0.31f, 0.55f, 0.20f}, t / 0.5f);
    }
    return lerpColor({0.31f, 0.55f, 0.20f}, {0.06f, 0.44f, 0.36f}, (t - 0.5f) / 0.5f);
}

Color3 slopeColor(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    if (t < 0.4f) {
        return lerpColor({0.15f, 0.38f, 0.12f}, {0.57f, 0.56f, 0.26f}, t / 0.4f);
    }
    if (t < 0.75f) {
        return lerpColor({0.57f, 0.56f, 0.26f}, {0.55f, 0.49f, 0.44f}, (t - 0.4f) / 0.35f);
    }
    return lerpColor({0.55f, 0.49f, 0.44f}, {0.95f, 0.95f, 0.95f}, (t - 0.75f) / 0.25f);
}

Color3 debugVertexColor(const terrain::TerrainVertex& v, float minH, float maxH, RenderMode mode) {
    switch (mode) {
    case RenderMode::SurfaceBiomes:
        return biomeVertexColor(v, minH, maxH);
    case RenderMode::Landforms: {
        const terrain::BiomeColor c = terrain::landformColor(static_cast<terrain::LandformId>(v.landform));
        return {c.r, c.g, c.b};
    }
    case RenderMode::Ecology: {
        const terrain::BiomeColor c = terrain::ecologyColor(static_cast<terrain::EcologyId>(v.ecology));
        return {c.r, c.g, c.b};
    }
    case RenderMode::Temperature:
        return heatmapColor(v.temperature);
    case RenderMode::Precipitation:
        return precipitationColor(v.precipitation);
    case RenderMode::Moisture:
        return moistureColor(v.moisture);
    case RenderMode::Slope:
        return slopeColor(v.slope);
    }

    return biomeVertexColor(v, minH, maxH);
}

void rebuildTerrainColorBuffer(
    const terrain::TerrainMesh& mesh,
    RenderMode mode,
    std::vector<float>& terrainColors) {
    terrainColors.resize(mesh.vertices.size() * 3u);
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const Color3 color = debugVertexColor(mesh.vertices[i], mesh.minHeight, mesh.maxHeight, mode);
        const size_t base = i * 3u;
        terrainColors[base + 0u] = color.r;
        terrainColors[base + 1u] = color.g;
        terrainColors[base + 2u] = color.b;
    }
}

GLuint createProceduralTexture(
    int size,
    float scale,
    uint32_t seed,
    const Color3& base,
    const Color3& tint,
    float contrast,
    float grainMix) {
    std::vector<unsigned char> data(static_cast<size_t>(size) * static_cast<size_t>(size) * 3u, 0u);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float fx = static_cast<float>(x) * scale;
            const float fy = static_cast<float>(y) * scale;
            const float low = valueNoise2D(fx, fy, seed);
            const float mid = valueNoise2D(fx * 2.0f, fy * 2.0f, seed + 17u);
            const float hi = valueNoise2D(fx * 4.0f, fy * 4.0f, seed + 53u);
            const float grain = valueNoise2D(fx * 10.0f, fy * 10.0f, seed + 97u);
            float n = low * 0.55f + mid * 0.30f + hi * 0.15f;
            n = terrain::lerp(n, grain, grainMix);
            n = std::clamp((n - 0.5f) * contrast + 0.5f, 0.0f, 1.0f);

            Color3 c{
                terrain::lerp(base.r, tint.r, n),
                terrain::lerp(base.g, tint.g, n),
                terrain::lerp(base.b, tint.b, n),
            };
            const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x)) * 3u;
            data[idx + 0u] = static_cast<unsigned char>(std::round(std::clamp(c.r, 0.0f, 1.0f) * 255.0f));
            data[idx + 1u] = static_cast<unsigned char>(std::round(std::clamp(c.g, 0.0f, 1.0f) * 255.0f));
            data[idx + 2u] = static_cast<unsigned char>(std::round(std::clamp(c.b, 0.0f, 1.0f) * 255.0f));
        }
    }

    GLuint tex = 0u;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, data.data());
    glfn::GenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint createWaterTexture(int size) {
    std::vector<unsigned char> data(static_cast<size_t>(size) * static_cast<size_t>(size) * 3u, 0u);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float fx = static_cast<float>(x) / static_cast<float>(size);
            const float fy = static_cast<float>(y) / static_cast<float>(size);
            const float ring = 0.5f + 0.5f * std::sin((fx * 12.0f + fy * 9.0f) * kPi);
            const float crossDetail = 0.5f + 0.5f * std::cos((fx * 8.0f - fy * 10.0f) * kPi);
            const float micro = valueNoise2D(fx * 24.0f, fy * 24.0f, 919u);
            const float n = std::clamp(ring * 0.42f + crossDetail * 0.38f + micro * 0.20f, 0.0f, 1.0f);

            const float r = terrain::lerp(0.28f, 0.76f, n);
            const float g = terrain::lerp(0.48f, 0.89f, n);
            const float b = terrain::lerp(0.62f, 1.00f, n);

            const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x)) * 3u;
            data[idx + 0u] = static_cast<unsigned char>(std::round(r * 255.0f));
            data[idx + 1u] = static_cast<unsigned char>(std::round(g * 255.0f));
            data[idx + 2u] = static_cast<unsigned char>(std::round(b * 255.0f));
        }
    }

    GLuint tex = 0u;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, data.data());
    glfn::GenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint compileShader(GLenum type, const char* source) {
    const GLuint shader = glfn::CreateShader(type);
    glfn::ShaderSource(shader, 1, &source, nullptr);
    glfn::CompileShader(shader);

    GLint compiled = GL_FALSE;
    glfn::GetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    GLint infoLen = 0;
    glfn::GetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
    std::string log(static_cast<size_t>(std::max(1, infoLen)), '\0');
    glfn::GetShaderInfoLog(shader, infoLen, nullptr, log.data());
    std::cerr << "Shader compilation failed:\n" << log << '\n';
    glfn::DeleteShader(shader);
    return 0u;
}

GLuint createProgram(const char* vertexSource, const char* fragmentSource) {
    const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    if (vertexShader == 0u) {
        return 0u;
    }
    const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (fragmentShader == 0u) {
        glfn::DeleteShader(vertexShader);
        return 0u;
    }

    const GLuint program = glfn::CreateProgram();
    glfn::AttachShader(program, vertexShader);
    glfn::AttachShader(program, fragmentShader);
    glfn::LinkProgram(program);

    GLint linked = GL_FALSE;
    glfn::GetProgramiv(program, GL_LINK_STATUS, &linked);
    glfn::DeleteShader(vertexShader);
    glfn::DeleteShader(fragmentShader);
    if (linked == GL_TRUE) {
        return program;
    }

    GLint infoLen = 0;
    glfn::GetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
    std::string log(static_cast<size_t>(std::max(1, infoLen)), '\0');
    glfn::GetProgramInfoLog(program, infoLen, nullptr, log.data());
    std::cerr << "Program link failed:\n" << log << '\n';
    glfn::DeleteProgram(program);
    return 0u;
}

void setUniform(GLuint program, const char* name, const Mat4& value) {
    const GLint loc = glfn::GetUniformLocation(program, name);
    if (loc >= 0) {
        glfn::UniformMatrix4fv(loc, 1, GL_FALSE, value.m);
    }
}

void setUniform(GLuint program, const char* name, int value) {
    const GLint loc = glfn::GetUniformLocation(program, name);
    if (loc >= 0) {
        glfn::Uniform1i(loc, value);
    }
}

void setUniform(GLuint program, const char* name, float value) {
    const GLint loc = glfn::GetUniformLocation(program, name);
    if (loc >= 0) {
        glfn::Uniform1f(loc, value);
    }
}

void setUniform(GLuint program, const char* name, const Vec3& value) {
    const GLint loc = glfn::GetUniformLocation(program, name);
    if (loc >= 0) {
        glfn::Uniform3f(loc, value.x, value.y, value.z);
    }
}

void destroyTexture(GLuint& texture) {
    if (texture != 0u) {
        glDeleteTextures(1, &texture);
        texture = 0u;
    }
}

void destroyProgram(GLuint& program) {
    if (program != 0u) {
        glfn::DeleteProgram(program);
        program = 0u;
    }
}

void destroyBuffer(GLuint& buffer) {
    if (buffer != 0u) {
        glfn::DeleteBuffers(1, &buffer);
        buffer = 0u;
    }
}

void destroyVertexArray(GLuint& vao) {
    if (vao != 0u) {
        glfn::DeleteVertexArrays(1, &vao);
        vao = 0u;
    }
}

void yawDirections(float yawDeg, float& forwardX, float& forwardZ, float& rightX, float& rightZ) {
    const float yawRad = degToRad(yawDeg);
    forwardX = std::cos(yawRad);
    forwardZ = std::sin(yawRad);
    rightX = std::cos(yawRad + kPi * 0.5f);
    rightZ = std::sin(yawRad + kPi * 0.5f);
}

Mat4 buildLightViewProjection(const terrain::TerrainMesh& mesh) {
    const float width = static_cast<float>(std::max(1, mesh.width - 1)) * mesh.horizontalScale;
    const float depth = static_cast<float>(std::max(1, mesh.depth - 1)) * mesh.horizontalScale;
    const Vec3 minCorner{0.0f, mesh.minHeight - 8.0f, 0.0f};
    const Vec3 maxCorner{width, mesh.maxHeight + 24.0f, depth};
    const Vec3 center{width * 0.5f, (mesh.minHeight + mesh.maxHeight) * 0.5f, depth * 0.5f};

    Vec3 lightForward = normalize(kSunDirection);
    Vec3 lightUp = {0.0f, 1.0f, 0.0f};
    if (std::abs(dot(lightForward, lightUp)) > 0.98f) {
        lightUp = {0.0f, 0.0f, 1.0f};
    }

    const float sceneRadius = std::max({width, depth, mesh.maxHeight - mesh.minHeight + 60.0f}) * 0.85f;
    const Vec3 lightEye = center - lightForward * (sceneRadius * 1.8f);
    const Mat4 lightView = lookAt(lightEye, center, lightUp);

    const std::array<Vec3, 8> corners = {
        Vec3{minCorner.x, minCorner.y, minCorner.z},
        Vec3{maxCorner.x, minCorner.y, minCorner.z},
        Vec3{minCorner.x, maxCorner.y, minCorner.z},
        Vec3{maxCorner.x, maxCorner.y, minCorner.z},
        Vec3{minCorner.x, minCorner.y, maxCorner.z},
        Vec3{maxCorner.x, minCorner.y, maxCorner.z},
        Vec3{minCorner.x, maxCorner.y, maxCorner.z},
        Vec3{maxCorner.x, maxCorner.y, maxCorner.z},
    };

    Vec3 mins{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3 maxs{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};
    for (const Vec3& corner : corners) {
        const Vec3 ls = transformPoint(lightView, corner);
        mins.x = std::min(mins.x, ls.x);
        mins.y = std::min(mins.y, ls.y);
        mins.z = std::min(mins.z, ls.z);
        maxs.x = std::max(maxs.x, ls.x);
        maxs.y = std::max(maxs.y, ls.y);
        maxs.z = std::max(maxs.z, ls.z);
    }

    const float margin = 24.0f;
    const Mat4 lightProjection = orthoBox(
        mins.x - margin,
        maxs.x + margin,
        mins.y - margin,
        maxs.y + margin,
        mins.z - margin,
        maxs.z + margin);
    return multiply(lightProjection, lightView);
}

} // namespace

const char* renderModeName(RenderMode mode) {
    switch (mode) {
    case RenderMode::SurfaceBiomes:
        return "Surface biomes";
    case RenderMode::Landforms:
        return "Landforms";
    case RenderMode::Ecology:
        return "Ecology";
    case RenderMode::Temperature:
        return "Temperature";
    case RenderMode::Precipitation:
        return "Precipitation";
    case RenderMode::Moisture:
        return "Moisture";
    case RenderMode::Slope:
        return "Slope";
    }

    return "Surface biomes";
}

Renderer::Renderer(int width, int height)
    : width_(width),
      height_(height),
      window_(nullptr),
      glContext_(nullptr),
      shouldClose_(false),
      yawDeg_(38.0f),
      pitchDeg_(32.0f),
      distance_(240.0f),
      targetX_(0.0f),
      targetY_(0.0f),
      targetZ_(0.0f),
      renderMode_(RenderMode::SurfaceBiomes),
      terrainBuffersValid_(false),
      waterBuffersValid_(false),
      terrainColorsValid_(false),
      cachedTerrainVertexCount_(0u),
      cachedTerrainIndexCount_(0u),
      cachedWaterVertexCount_(0u),
      cachedWaterIndexCount_(0u),
      terrainVao_(0u),
      terrainVbo_(0u),
      terrainIbo_(0u),
      waterVao_(0u),
      waterVbo_(0u),
      waterIbo_(0u),
      skyVao_(0u),
      terrainProgram_(0u),
      skyProgram_(0u),
      waterProgram_(0u),
      shadowProgram_(0u),
      shadowFramebuffer_(0u),
      shadowDepthTexture_(0u),
      grassTexture_(0u),
      rockTexture_(0u),
      snowTexture_(0u),
      sandTexture_(0u),
      waterTexture_(0u),
      shadowMapSize_(kShadowMapSize) {}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_DisplayMode displayMode;
    SDL_GetCurrentDisplayMode(0, &displayMode);
    width_ = displayMode.w;
    height_ = displayMode.h;

    window_ = SDL_CreateWindow(
        "Terrain Generator Demo",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width_,
        height_,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);
    if (!window_) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        return false;
    }

    glContext_ = SDL_GL_CreateContext(window_);
    if (!glContext_) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << '\n';
        return false;
    }

    if (!glfn::load()) {
        return false;
    }

    SDL_GL_SetSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    glCullFace(GL_BACK);
    glEnable(GL_MULTISAMPLE);
    glClearColor(kFogHorizonColor.x, kFogHorizonColor.y, kFogHorizonColor.z, 1.0f);

    terrainProgram_ = createProgram(kTerrainVertexShader, kTerrainFragmentShader);
    skyProgram_ = createProgram(kSkyVertexShader, kSkyFragmentShader);
    waterProgram_ = createProgram(kWaterVertexShader, kWaterFragmentShader);
    shadowProgram_ = createProgram(kShadowVertexShader, kShadowFragmentShader);
    if (terrainProgram_ == 0u || skyProgram_ == 0u || waterProgram_ == 0u || shadowProgram_ == 0u) {
        return false;
    }

    glfn::GenVertexArrays(1, &skyVao_);

    glfn::GenFramebuffers(1, &shadowFramebuffer_);
    glGenTextures(1, &shadowDepthTexture_);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const GLfloat borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_DEPTH_COMPONENT24,
        shadowMapSize_,
        shadowMapSize_,
        0,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        nullptr);

    glfn::BindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer_);
    glfn::FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepthTexture_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glfn::CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Shadow framebuffer is incomplete\n";
        glfn::BindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }
    glfn::BindFramebuffer(GL_FRAMEBUFFER, 0);

    grassTexture_ = createProceduralTexture(
        kMaterialTextureSize,
        0.085f,
        211u,
        {0.28f, 0.34f, 0.18f},
        {0.55f, 0.66f, 0.32f},
        1.18f,
        0.28f);
    rockTexture_ = createProceduralTexture(
        kMaterialTextureSize,
        0.120f,
        307u,
        {0.36f, 0.37f, 0.38f},
        {0.62f, 0.62f, 0.63f},
        1.45f,
        0.30f);
    snowTexture_ = createProceduralTexture(
        kMaterialTextureSize,
        0.090f,
        401u,
        {0.88f, 0.91f, 0.94f},
        {0.98f, 0.99f, 1.00f},
        1.15f,
        0.14f);
    sandTexture_ = createProceduralTexture(
        kMaterialTextureSize,
        0.110f,
        503u,
        {0.66f, 0.59f, 0.42f},
        {0.82f, 0.74f, 0.53f},
        1.20f,
        0.18f);
    waterTexture_ = createWaterTexture(kWaterTextureSize);

    std::cout << "GL Vendor: " << glGetString(GL_VENDOR) << '\n';
    std::cout << "GL Renderer: " << glGetString(GL_RENDERER) << '\n';
    std::cout << "GL Version: " << glGetString(GL_VERSION) << '\n';
    return true;
}

void Renderer::shutdown() {
    destroyTexture(grassTexture_);
    destroyTexture(rockTexture_);
    destroyTexture(snowTexture_);
    destroyTexture(sandTexture_);
    destroyTexture(waterTexture_);
    destroyTexture(shadowDepthTexture_);

    if (shadowFramebuffer_ != 0u) {
        glfn::DeleteFramebuffers(1, &shadowFramebuffer_);
        shadowFramebuffer_ = 0u;
    }

    destroyProgram(terrainProgram_);
    destroyProgram(skyProgram_);
    destroyProgram(waterProgram_);
    destroyProgram(shadowProgram_);

    destroyBuffer(terrainVbo_);
    destroyBuffer(terrainIbo_);
    destroyBuffer(waterVbo_);
    destroyBuffer(waterIbo_);
    destroyVertexArray(terrainVao_);
    destroyVertexArray(waterVao_);
    destroyVertexArray(skyVao_);

    if (glContext_) {
        SDL_GL_DeleteContext(glContext_);
        glContext_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

bool Renderer::shouldClose() const {
    return shouldClose_;
}

void Renderer::swapBuffers() {
    if (window_) {
        SDL_GL_SwapWindow(window_);
    }
}

void Renderer::orbit(float deltaYaw, float deltaPitch) {
    yawDeg_ += deltaYaw;
    pitchDeg_ += deltaPitch;
    pitchDeg_ = std::clamp(pitchDeg_, -30.0f, 88.0f);
}

void Renderer::zoom(float deltaDistance) {
    distance_ += deltaDistance;
    distance_ = std::clamp(distance_, 18.0f, 1600.0f);
}

void Renderer::pan(float deltaX, float deltaY) {
    float forwardX, forwardZ, rightX, rightZ;
    yawDirections(yawDeg_, forwardX, forwardZ, rightX, rightZ);
    targetX_ += rightX * deltaX;
    targetZ_ += rightZ * deltaX;
    targetY_ += deltaY;
}

void Renderer::moveForward(float amount) {
    float forwardX, forwardZ, rightX, rightZ;
    yawDirections(yawDeg_, forwardX, forwardZ, rightX, rightZ);
    targetX_ += forwardX * amount;
    targetZ_ += forwardZ * amount;
}

void Renderer::moveRight(float amount) {
    float forwardX, forwardZ, rightX, rightZ;
    yawDirections(yawDeg_, forwardX, forwardZ, rightX, rightZ);
    targetX_ += rightX * amount;
    targetZ_ += rightZ * amount;
}

void Renderer::setTarget(float x, float y, float z) {
    targetX_ = x;
    targetY_ = y;
    targetZ_ = z;
}

void Renderer::setRenderMode(RenderMode mode) {
    if (renderMode_ != mode) {
        terrainColorsValid_ = false;
    }
    renderMode_ = mode;
}

RenderMode Renderer::renderMode() const {
    return renderMode_;
}

void Renderer::invalidateMeshCache() {
    terrainBuffersValid_ = false;
    waterBuffersValid_ = false;
    terrainColorsValid_ = false;
    cachedTerrainVertexCount_ = 0u;
    cachedTerrainIndexCount_ = 0u;
    cachedWaterVertexCount_ = 0u;
    cachedWaterIndexCount_ = 0u;
    terrainBaseColors_.clear();
}

bool Renderer::captureScreenshot(const std::string& filepath) const {
    if (!window_) {
        return false;
    }

    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GL_GetDrawableSize(window_, &drawableWidth, &drawableHeight);
    if (drawableWidth <= 0 || drawableHeight <= 0) {
        return false;
    }

    std::vector<unsigned char> pixels(
        static_cast<size_t>(drawableWidth) * static_cast<size_t>(drawableHeight) * 3u,
        0u);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, drawableWidth, drawableHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR) {
        return false;
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0,
        drawableWidth,
        drawableHeight,
        24,
        SDL_PIXELFORMAT_RGB24);
    if (!surface) {
        return false;
    }

    const size_t rowBytes = static_cast<size_t>(drawableWidth) * 3u;
    for (int y = 0; y < drawableHeight; ++y) {
        unsigned char* dst = static_cast<unsigned char*>(surface->pixels) +
                             static_cast<size_t>(y) * static_cast<size_t>(surface->pitch);
        const unsigned char* src = pixels.data() +
                                   static_cast<size_t>(drawableHeight - 1 - y) * rowBytes;
        std::memcpy(dst, src, rowBytes);
    }

    const int saveResult = SDL_SaveBMP(surface, filepath.c_str());
    SDL_FreeSurface(surface);
    return saveResult == 0;
}

void Renderer::render(const terrain::TerrainMesh& mesh) {
    if (!window_) {
        return;
    }

    if (!terrainColorsValid_ || !terrainBuffersValid_ || cachedTerrainVertexCount_ != mesh.vertices.size() ||
        cachedTerrainIndexCount_ != mesh.indices.size()) {
        rebuildTerrainColorBuffer(mesh, renderMode_, terrainBaseColors_);

        std::vector<TerrainGpuVertex> terrainVertices(mesh.vertices.size());
        const float invHeightRange = 1.0f / std::max(0.001f, mesh.maxHeight - mesh.minHeight);
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            const terrain::TerrainVertex& v = mesh.vertices[i];
            TerrainGpuVertex gpu{};
            gpu.position[0] = v.x;
            gpu.position[1] = v.y;
            gpu.position[2] = v.z;
            gpu.normal[0] = v.nx;
            gpu.normal[1] = v.ny;
            gpu.normal[2] = v.nz;

            const size_t colorBase = i * 3u;
            gpu.baseColor[0] = terrainBaseColors_[colorBase + 0u];
            gpu.baseColor[1] = terrainBaseColors_[colorBase + 1u];
            gpu.baseColor[2] = terrainBaseColors_[colorBase + 2u];

            gpu.params0[0] = std::clamp(v.slope, 0.0f, 1.0f);
            gpu.params0[1] = std::clamp(v.mountainWeight, 0.0f, 1.0f);
            gpu.params0[2] = std::clamp(v.riverWeight, 0.0f, 1.0f);
            gpu.params0[3] = std::clamp(v.moisture, 0.0f, 1.0f);

            gpu.params1[0] = std::clamp((v.y - mesh.minHeight) * invHeightRange, 0.0f, 1.0f);
            gpu.params1[1] = std::clamp(v.temperature, 0.0f, 1.0f);
            gpu.params1[2] = std::clamp(v.precipitation, 0.0f, 1.0f);
            gpu.params1[3] = 0.0f;
            terrainVertices[i] = gpu;
        }

        if (terrainVao_ == 0u) {
            glfn::GenVertexArrays(1, &terrainVao_);
            glfn::GenBuffers(1, &terrainVbo_);
            glfn::GenBuffers(1, &terrainIbo_);
        }

        glfn::BindVertexArray(terrainVao_);
        glfn::BindBuffer(GL_ARRAY_BUFFER, terrainVbo_);
        glfn::BufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(terrainVertices.size() * sizeof(TerrainGpuVertex)),
            terrainVertices.data(),
            GL_STATIC_DRAW);
        glfn::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainIbo_);
        glfn::BufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(uint32_t)),
            mesh.indices.data(),
            GL_STATIC_DRAW);

        glfn::EnableVertexAttribArray(0);
        glfn::VertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(TerrainGpuVertex)),
            reinterpret_cast<const void*>(offsetof(TerrainGpuVertex, position)));
        glfn::EnableVertexAttribArray(1);
        glfn::VertexAttribPointer(
            1,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(TerrainGpuVertex)),
            reinterpret_cast<const void*>(offsetof(TerrainGpuVertex, normal)));
        glfn::EnableVertexAttribArray(2);
        glfn::VertexAttribPointer(
            2,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(TerrainGpuVertex)),
            reinterpret_cast<const void*>(offsetof(TerrainGpuVertex, baseColor)));
        glfn::EnableVertexAttribArray(3);
        glfn::VertexAttribPointer(
            3,
            4,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(TerrainGpuVertex)),
            reinterpret_cast<const void*>(offsetof(TerrainGpuVertex, params0)));
        glfn::EnableVertexAttribArray(4);
        glfn::VertexAttribPointer(
            4,
            4,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(TerrainGpuVertex)),
            reinterpret_cast<const void*>(offsetof(TerrainGpuVertex, params1)));
        glfn::BindVertexArray(0);

        cachedTerrainVertexCount_ = mesh.vertices.size();
        cachedTerrainIndexCount_ = mesh.indices.size();
        terrainColorsValid_ = true;
        terrainBuffersValid_ = true;
    }

    if (usesMaterials(renderMode_) &&
        (!waterBuffersValid_ || cachedWaterVertexCount_ != mesh.waterVertices.size() ||
         cachedWaterIndexCount_ != mesh.waterIndices.size())) {
        std::vector<WaterGpuVertex> waterVertices(mesh.waterVertices.size());
        const float invHeightRange = 1.0f / std::max(0.001f, mesh.maxHeight - mesh.minHeight);
        for (size_t i = 0; i < mesh.waterVertices.size(); ++i) {
            const terrain::TerrainVertex& water = mesh.waterVertices[i];
            const terrain::TerrainVertex& terrainVertex = mesh.vertices[i];
            WaterGpuVertex gpu{};
            gpu.position[0] = water.x;
            gpu.position[1] = water.y;
            gpu.position[2] = water.z;
            gpu.params[0] = std::clamp(water.riverWeight, 0.0f, 1.0f);
            gpu.params[1] = std::clamp(terrainVertex.slope, 0.0f, 1.0f);
            gpu.params[2] = std::clamp((terrainVertex.y - mesh.minHeight) * invHeightRange, 0.0f, 1.0f);
            const float edge = terrain::smoothstep(0.02f, 0.18f, gpu.params[0]) *
                               terrain::smoothstep(0.0f, 0.42f, 1.0f - gpu.params[0]);
            const float turbulent = terrain::smoothstep(0.06f, 0.42f, gpu.params[1]) *
                                    terrain::smoothstep(0.10f, 0.85f, gpu.params[0]);
            gpu.params[3] = std::clamp(edge * 0.58f + turbulent * 0.46f, 0.0f, 1.0f);
            waterVertices[i] = gpu;
        }

        if (waterVao_ == 0u) {
            glfn::GenVertexArrays(1, &waterVao_);
            glfn::GenBuffers(1, &waterVbo_);
            glfn::GenBuffers(1, &waterIbo_);
        }

        glfn::BindVertexArray(waterVao_);
        glfn::BindBuffer(GL_ARRAY_BUFFER, waterVbo_);
        glfn::BufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(waterVertices.size() * sizeof(WaterGpuVertex)),
            waterVertices.data(),
            GL_STATIC_DRAW);
        glfn::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, waterIbo_);
        glfn::BufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(mesh.waterIndices.size() * sizeof(uint32_t)),
            mesh.waterIndices.data(),
            GL_STATIC_DRAW);

        glfn::EnableVertexAttribArray(0);
        glfn::VertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(WaterGpuVertex)),
            reinterpret_cast<const void*>(offsetof(WaterGpuVertex, position)));
        glfn::EnableVertexAttribArray(1);
        glfn::VertexAttribPointer(
            1,
            4,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(WaterGpuVertex)),
            reinterpret_cast<const void*>(offsetof(WaterGpuVertex, params)));
        glfn::BindVertexArray(0);

        cachedWaterVertexCount_ = mesh.waterVertices.size();
        cachedWaterIndexCount_ = mesh.waterIndices.size();
        waterBuffersValid_ = true;
    }

    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GL_GetDrawableSize(window_, &drawableWidth, &drawableHeight);
    if (drawableWidth <= 0 || drawableHeight <= 0) {
        return;
    }

    width_ = drawableWidth;
    height_ = drawableHeight;

    const float yawRad = degToRad(yawDeg_);
    const float pitchRad = degToRad(pitchDeg_);
    const Vec3 target{targetX_, targetY_, targetZ_};
    const Vec3 eye{
        targetX_ + distance_ * std::cos(pitchRad) * std::cos(yawRad),
        targetY_ + distance_ * std::sin(pitchRad),
        targetZ_ + distance_ * std::cos(pitchRad) * std::sin(yawRad),
    };

    const Mat4 view = lookAt(eye, target, {0.0f, 1.0f, 0.0f});
    const Mat4 projection = perspective(
        degToRad(kCameraFovDeg),
        static_cast<float>(drawableWidth) / static_cast<float>(drawableHeight),
        kCameraNear,
        kCameraFar);
    const Mat4 viewProjection = multiply(projection, view);
    const Mat4 lightViewProjection = buildLightViewProjection(mesh);

    const Vec3 cameraForward = normalize(target - eye);
    const Vec3 cameraRight = normalize(cross(cameraForward, {0.0f, 1.0f, 0.0f}));
    const Vec3 cameraUp = cross(cameraRight, cameraForward);
    const Vec3 sunLightDir = normalize(Vec3{-kSunDirection.x, -kSunDirection.y, -kSunDirection.z});
    const float tanHalfFov = std::tan(degToRad(kCameraFovDeg) * 0.5f);

    glEnable(GL_DEPTH_TEST);

    glViewport(0, 0, drawableWidth, drawableHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (terrainBuffersValid_ && cachedTerrainIndexCount_ > 0u) {
        glfn::BindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer_);
        glViewport(0, 0, shadowMapSize_, shadowMapSize_);
        glClear(GL_DEPTH_BUFFER_BIT);
        glCullFace(GL_FRONT);
        glfn::UseProgram(shadowProgram_);
        setUniform(shadowProgram_, "uLightViewProj", lightViewProjection);
        glfn::BindVertexArray(terrainVao_);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(cachedTerrainIndexCount_),
            GL_UNSIGNED_INT,
            nullptr);
        glfn::BindVertexArray(0);
        glCullFace(GL_BACK);
        glfn::BindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glViewport(0, 0, drawableWidth, drawableHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glfn::UseProgram(skyProgram_);
    setUniform(skyProgram_, "uCameraForward", cameraForward);
    setUniform(skyProgram_, "uCameraRight", cameraRight);
    setUniform(skyProgram_, "uCameraUp", cameraUp);
    setUniform(skyProgram_, "uSunLightDir", sunLightDir);
    setUniform(skyProgram_, "uSunColor", kSunColor);
    setUniform(skyProgram_, "uSkyZenithColor", kSkyZenithColor);
    setUniform(skyProgram_, "uSkyHorizonColor", kSkyHorizonColor);
    setUniform(skyProgram_, "uFogHorizonColor", kFogHorizonColor);
    setUniform(skyProgram_, "uAspect", static_cast<float>(drawableWidth) / static_cast<float>(drawableHeight));
    setUniform(skyProgram_, "uTanHalfFov", tanHalfFov);
    glfn::BindVertexArray(skyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glfn::BindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);

    glfn::UseProgram(terrainProgram_);
    setUniform(terrainProgram_, "uViewProj", viewProjection);
    setUniform(terrainProgram_, "uLightViewProj", lightViewProjection);
    setUniform(terrainProgram_, "uCameraPos", eye);
    setUniform(terrainProgram_, "uSunLightDir", sunLightDir);
    setUniform(terrainProgram_, "uSunColor", kSunColor);
    setUniform(terrainProgram_, "uSkyAmbientColor", kSkyAmbientColor);
    setUniform(terrainProgram_, "uGroundAmbientColor", kGroundAmbientColor);
    setUniform(terrainProgram_, "uFogHorizonColor", kFogHorizonColor);
    setUniform(terrainProgram_, "uFogZenithColor", kFogZenithColor);
    setUniform(terrainProgram_, "uFogSunColor", kFogSunColor);
    setUniform(terrainProgram_, "uFogDensity", 0.0028f + std::clamp(distance_ / 3000.0f, 0.0f, 0.0013f));
    setUniform(terrainProgram_, "uFogHeightFalloff", 0.020f);
    setUniform(terrainProgram_, "uFogBaseHeight", mesh.minHeight + 8.0f);
    setUniform(terrainProgram_, "uShadowTexelSize", 1.0f / static_cast<float>(shadowMapSize_));
    setUniform(terrainProgram_, "uEnableMaterials", usesMaterials(renderMode_) ? 1 : 0);
    setUniform(terrainProgram_, "uEnableShadows", 1);
    setUniform(terrainProgram_, "uGrassTex", 0);
    setUniform(terrainProgram_, "uRockTex", 1);
    setUniform(terrainProgram_, "uSandTex", 2);
    setUniform(terrainProgram_, "uSnowTex", 3);
    setUniform(terrainProgram_, "uShadowMap", 4);

    glfn::ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, grassTexture_);
    glfn::ActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, rockTexture_);
    glfn::ActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, sandTexture_);
    glfn::ActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, snowTexture_);
    glfn::ActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTexture_);

    glfn::BindVertexArray(terrainVao_);
    glDrawElements(
        GL_TRIANGLES,
        static_cast<GLsizei>(cachedTerrainIndexCount_),
        GL_UNSIGNED_INT,
        nullptr);
    glfn::BindVertexArray(0);

    if (usesMaterials(renderMode_) && waterBuffersValid_ && cachedWaterIndexCount_ > 0u) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);

        glfn::UseProgram(waterProgram_);
        setUniform(waterProgram_, "uViewProj", viewProjection);
        setUniform(waterProgram_, "uLightViewProj", lightViewProjection);
        setUniform(waterProgram_, "uCameraPos", eye);
        setUniform(waterProgram_, "uSunLightDir", sunLightDir);
        setUniform(waterProgram_, "uSunColor", kSunColor);
        setUniform(waterProgram_, "uSkyZenithColor", kSkyZenithColor);
        setUniform(waterProgram_, "uFogHorizonColor", kFogHorizonColor);
        setUniform(waterProgram_, "uFogZenithColor", kFogZenithColor);
        setUniform(waterProgram_, "uFogSunColor", kFogSunColor);
        setUniform(waterProgram_, "uFogDensity", 0.0028f + std::clamp(distance_ / 3000.0f, 0.0f, 0.0013f));
        setUniform(waterProgram_, "uFogHeightFalloff", 0.020f);
        setUniform(waterProgram_, "uFogBaseHeight", mesh.minHeight + 8.0f);
        setUniform(waterProgram_, "uShadowTexelSize", 1.0f / static_cast<float>(shadowMapSize_));
        setUniform(waterProgram_, "uTime", static_cast<float>(SDL_GetTicks()) * 0.001f);
        setUniform(waterProgram_, "uWaterTex", 0);
        setUniform(waterProgram_, "uShadowMap", 1);

        glfn::ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, waterTexture_);
        glfn::ActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowDepthTexture_);

        glfn::BindVertexArray(waterVao_);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(cachedWaterIndexCount_),
            GL_UNSIGNED_INT,
            nullptr);
        glfn::BindVertexArray(0);

        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
    }

    glfn::ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glfn::UseProgram(0);
}

void runDemo() {
    using namespace std::chrono;

    terrain::TerrainSettings settings;
    settings.width = 512;
    settings.depth = 512;
    settings.horizontalScale = 2.0f;
    settings.verticalScale = 96.0f;
    settings.islandFalloff = false;
    settings.seed = 2026u;
    settings.noise.frequency = 0.0052f;
    settings.noise.octaves = 6;
    settings.noise.lacunarity = 2.0f;
    settings.noise.gain = 0.5f;
    settings.noise.ridgeSharpness = 2.5f;
    settings.noise.warpFrequency = 0.0038f;
    settings.noise.warpAmplitude = 20.0f;
    settings.rivers.sourceDensity = 0.0004f;
    settings.rivers.sourceAccumulation = 40.0f;
    settings.rivers.mainAccumulation = 100.0f;
    settings.rivers.maxHalfWidth = 1;
    settings.rivers.baseCarveFraction = 0.02f;
    settings.rivers.maxCarveFraction = 0.05f;

    terrain::TerrainGenerator generator(settings);
    terrain::TerrainMesh mesh = generator.generateMesh();

    auto printBiomeStats = [&mesh]() {
        std::array<size_t, static_cast<size_t>(terrain::BiomeId::Count)> counts{};
        for (const terrain::TerrainVertex& v : mesh.vertices) {
            ++counts[static_cast<size_t>(v.primaryBiome)];
        }

        std::cout << "Surface coverage:";
        const float invTotal = mesh.vertices.empty() ? 0.0f : 100.0f / static_cast<float>(mesh.vertices.size());
        for (size_t idx = 0; idx < counts.size(); ++idx) {
            if (counts[idx] == 0) {
                continue;
            }
            std::cout << ' ' << terrain::biomeName(static_cast<terrain::BiomeId>(idx)) << ' '
                      << counts[idx] * invTotal << '%';
        }
        std::cout << '\n';
    };

    Renderer renderer(1280, 800);
    if (!renderer.init()) {
        std::cerr << "Renderer init failed\n";
        return;
    }

    auto setMode = [&renderer](RenderMode mode) {
        renderer.setRenderMode(mode);
        std::cout << "Render mode: " << renderModeName(mode) << '\n';
    };

    RenderMode currentMode = renderer.renderMode();

    const float centerX = (static_cast<float>(settings.width - 1) * settings.horizontalScale) * 0.5f;
    const float centerZ = (static_cast<float>(settings.depth - 1) * settings.horizontalScale) * 0.5f;
    renderer.setTarget(centerX, (mesh.minHeight + mesh.maxHeight) * 0.30f, centerZ);
    renderer.zoom(220.0f);

    std::cout << "Controls:\n";
    std::cout << "  Left mouse drag: orbit\n";
    std::cout << "  Right mouse drag: pan\n";
    std::cout << "  Wheel: zoom\n";
    std::cout << "  WASD: move\n";
    std::cout << "  Q/E: move down/up\n";
    std::cout << "  R: regenerate terrain\n";
    std::cout << "  V: cycle forward through modes\n";
    std::cout << "  Shift+V: cycle backward through modes\n";
    std::cout << "  P: save screenshot\n";
    std::cout << "  ESC: quit\n";
    printBiomeStats();
    setMode(currentMode);

    SDL_Event event;
    bool orbiting = false;
    bool panning = false;
    int prevMouseX = 0;
    int prevMouseY = 0;
    using Clock = std::chrono::steady_clock;
    auto lastFrameTime = Clock::now();

    while (!renderer.shouldClose()) {
        const auto frameTime = Clock::now();
        const float deltaSeconds = std::clamp(
            std::chrono::duration<float>(frameTime - lastFrameTime).count(),
            0.0f,
            0.1f);
        lastFrameTime = frameTime;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                return;
            }

            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    return;
                case SDLK_r:
                    settings.seed += 1u;
                    generator.setSettings(settings);
                    mesh = generator.generateMesh();
                    renderer.invalidateMeshCache();
                    std::cout << "Regenerated terrain with seed " << settings.seed << '\n';
                    printBiomeStats();
                    break;
                case SDLK_v:
                    if (event.key.keysym.mod & KMOD_SHIFT) {
                        if (static_cast<int>(currentMode) == 0) {
                            currentMode = RenderMode::Slope;
                        } else {
                            currentMode = static_cast<RenderMode>(static_cast<int>(currentMode) - 1);
                        }
                    } else {
                        currentMode = static_cast<RenderMode>(
                            (static_cast<int>(currentMode) + 1) % (static_cast<int>(RenderMode::Slope) + 1));
                    }
                    renderer.setRenderMode(currentMode);
                    std::cout << "Render mode: " << renderModeName(currentMode) << '\n';
                    break;
                case SDLK_p: {
                    const std::time_t now = std::time(nullptr);
                    const std::string screenshotPath =
                        "screenshot_" + std::to_string(static_cast<long long>(now)) + ".bmp";
                    if (renderer.captureScreenshot(screenshotPath)) {
                        std::cout << "Saved screenshot to " << screenshotPath << '\n';
                    } else {
                        std::cout << "Failed to save screenshot\n";
                    }
                    break;
                }
                default:
                    break;
                }
            }

            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                orbiting = true;
                prevMouseX = event.button.x;
                prevMouseY = event.button.y;
            }
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                orbiting = false;
            }

            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
                panning = true;
                prevMouseX = event.button.x;
                prevMouseY = event.button.y;
            }
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) {
                panning = false;
            }

            if (event.type == SDL_MOUSEMOTION) {
                const int dx = event.motion.x - prevMouseX;
                const int dy = event.motion.y - prevMouseY;

                if (orbiting) {
                    renderer.orbit(static_cast<float>(dx) * 0.28f, static_cast<float>(-dy) * 0.28f);
                }
                if (panning) {
                    const float panScale = std::max(0.3f, 0.005f * mesh.horizontalScale * 300.0f);
                    renderer.pan(static_cast<float>(-dx) * panScale, static_cast<float>(dy) * panScale);
                }

                prevMouseX = event.motion.x;
                prevMouseY = event.motion.y;
            }

            if (event.type == SDL_MOUSEWHEEL) {
                renderer.zoom(static_cast<float>(-event.wheel.y) * 14.0f);
            }
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        const float moveSpeed = 170.0f * deltaSeconds;
        if (keys[SDL_SCANCODE_W]) {
            renderer.moveForward(-moveSpeed);
        }
        if (keys[SDL_SCANCODE_S]) {
            renderer.moveForward(moveSpeed);
        }
        if (keys[SDL_SCANCODE_A]) {
            renderer.moveRight(moveSpeed);
        }
        if (keys[SDL_SCANCODE_D]) {
            renderer.moveRight(-moveSpeed);
        }
        if (keys[SDL_SCANCODE_Q]) {
            renderer.pan(0.0f, -moveSpeed);
        }
        if (keys[SDL_SCANCODE_E]) {
            renderer.pan(0.0f, moveSpeed);
        }

        renderer.render(mesh);
        renderer.swapBuffers();
    }
}

} // namespace renderer
