#ifndef INTERNAL_H
#define INTERNAL_H

#include "renderer/core.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "terrain.h"
#include "terrain/util.h"
#include "utils.h"

namespace renderer {

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr int kMaterialTextureSize = 256;
inline constexpr int kShadowMapSize = 1024;
inline constexpr float kCameraFovDeg = 60.0f;
inline constexpr float kCameraNear = 0.5f;
inline constexpr float kCameraFar = 5000.0f;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Mat4 {
    float m[16] = {
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    };
};

struct Color3 {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

inline Color3 clampColor(Color3 c, float min, float max) {
    c.r = std::clamp(c.r, min, max);
    c.g = std::clamp(c.g, min, max);
    c.b = std::clamp(c.b, min, max);
    return c;
}

inline Color3 scaleColor(Color3 c, float s) {
    return {c.r * s, c.g * s, c.b * s};
}

inline Color3 addColor(Color3 a, Color3 b) {
    return {a.r + b.r, a.g + b.g, a.b + b.b};
}

inline Color3 mulColor(Color3 a, Color3 b) {
    return {a.r * b.r, a.g * b.g, a.b * b.b};
}

inline Color3 mixColor(Color3 a, Color3 b, float t) {
    return {
        ::lerp(a.r, b.r, t),
        ::lerp(a.g, b.g, t),
        ::lerp(a.b, b.b, t),
    };
}

struct TerrainGpuVertex {
    float position[3];
    float normal[3];
    float baseColor[3];
    float params0[4];
    float params1[4];
};

namespace glfn {
extern PFNGLGENVERTEXARRAYSPROC GenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC BindVertexArray;
extern PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays;
extern PFNGLGENBUFFERSPROC GenBuffers;
extern PFNGLBINDBUFFERPROC BindBuffer;
extern PFNGLBUFFERDATAPROC BufferData;
extern PFNGLDELETEBUFFERSPROC DeleteBuffers;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer;
extern PFNGLCREATESHADERPROC CreateShader;
extern PFNGLSHADERSOURCEPROC ShaderSource;
extern PFNGLCOMPILESHADERPROC CompileShader;
extern PFNGLGETSHADERIVPROC GetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;
extern PFNGLDELETESHADERPROC DeleteShader;
extern PFNGLCREATEPROGRAMPROC CreateProgram;
extern PFNGLATTACHSHADERPROC AttachShader;
extern PFNGLLINKPROGRAMPROC LinkProgram;
extern PFNGLGETPROGRAMIVPROC GetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog;
extern PFNGLUSEPROGRAMPROC UseProgram;
extern PFNGLDELETEPROGRAMPROC DeleteProgram;
extern PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation;
extern PFNGLUNIFORMMATRIX4FVPROC UniformMatrix4fv;
extern PFNGLUNIFORM1IPROC Uniform1i;
extern PFNGLUNIFORM1FPROC Uniform1f;
extern PFNGLUNIFORM3FPROC Uniform3f;
extern PFNGLACTIVETEXTUREPROC ActiveTexture;
extern PFNGLGENERATEMIPMAPPROC GenerateMipmap;
extern PFNGLGENFRAMEBUFFERSPROC GenFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC BindFramebuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus;
extern PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers;

bool load();
} // namespace glfn

inline constexpr Vec3 kSunDirection = {-0.42f, -1.0f, -0.28f};
inline constexpr Vec3 kSunColor = {1.12f, 1.03f, 0.92f};
inline constexpr Vec3 kSkyAmbientColor = {0.34f, 0.43f, 0.53f};
inline constexpr Vec3 kGroundAmbientColor = {0.10f, 0.09f, 0.08f};
inline constexpr Vec3 kSkyZenithColor = {0.21f, 0.40f, 0.70f};
inline constexpr Vec3 kSkyHorizonColor = {0.22f, 0.30f, 0.42f};
inline constexpr Vec3 kFogHorizonColor = {0.58f, 0.68f, 0.78f};
inline constexpr Vec3 kFogZenithColor = {0.28f, 0.44f, 0.62f};
inline constexpr Vec3 kFogSunColor = {0.64f, 0.48f, 0.30f};

float degToRad(float deg);
bool usesMaterials(Mode mode);
void rebuildTerrainColorBuffer(const terrain::TerrainMesh& mesh, Mode mode, std::vector<float>& terrainColors);
GLuint createProceduralTexture(
    int size,
    float scale,
    uint32_t seed,
    const Color3& base,
    const Color3& tint,
    float contrast,
    float grainMix);
GLuint createProgram(const char* vertexSource, const char* fragmentSource);

Vec3 operator+(const Vec3& a, const Vec3& b);
Vec3 operator-(const Vec3& a, const Vec3& b);
Vec3 operator*(const Vec3& v, float scalar);
float dot(const Vec3& a, const Vec3& b);
Vec3 cross(const Vec3& a, const Vec3& b);
float length(const Vec3& v);
Vec3 normalize(const Vec3& v);
Mat4 multiply(const Mat4& a, const Mat4& b);
Vec3 transformPoint(const Mat4& m, const Vec3& v);
Mat4 perspective(float fovYRadians, float aspect, float nearPlane, float farPlane);
Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up);
Mat4 orthoBox(float left, float right, float bottom, float top, float nearPlane, float farPlane);
Mat4 buildLightViewProjection(const terrain::TerrainMesh& mesh);

void setUniform(GLuint program, const char* name, const Mat4& value);
void setUniform(GLuint program, const char* name, int value);
void setUniform(GLuint program, const char* name, float value);
void setUniform(GLuint program, const char* name, const Vec3& value);

void destroyTexture(GLuint& texture);
void destroyProgram(GLuint& program);
void destroyBuffer(GLuint& buffer);
void destroyVertexArray(GLuint& vao);

void yawDirections(float yawDeg, float& forwardX, float& forwardZ, float& rightX, float& rightZ);

} // namespace renderer

#endif // INTERNAL_H
