#include "renderer/internal.h"

#include "terrain/biomes.h"
#include "terrain/util.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace renderer {

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

bool load() {
    auto loadSymbol = [](const char* name) {
        return SDL_GL_GetProcAddress(name);
    };

#define LOAD_GL(symbol)                                                        \
    do {                                                                       \
        symbol = reinterpret_cast<decltype(symbol)>(loadSymbol("gl" #symbol)); \
        if (!(symbol)) {                                                       \
            std::cerr << "Failed to load OpenGL symbol gl" #symbol << '\n';    \
            return false;                                                      \
        }                                                                      \
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

#undef LOAD_GL
    return true;
}
} // namespace glfn

float degToRad(float deg) {
    return deg * kPi / 180.0f;
}

namespace {

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
    const float nx0 = lerp(n00, n10, sx);
    const float nx1 = lerp(n01, n11, sx);
    return lerp(nx0, nx1, sy);
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

    const float rockSignal = slope * 0.38f + v.mountainWeight * 0.82f + h * 0.08f;
    const float rockBoost = terrain::smoothstep(0.46f, 0.88f, rockSignal);
    const Color3 rockTint{0.44f, 0.42f, 0.40f};
    color = mixColor(color, rockTint, rockBoost * 0.10f);

    const float terraceNoise =
        0.5f + 0.5f * std::sin(v.x * 0.048f + v.z * 0.031f) * std::cos(v.x * 0.019f - v.z * 0.043f);
    const float cavity = terrain::smoothstep(0.16f, 0.78f, slope) * (0.62f + 0.38f * (1.0f - h));
    const float riverDarkening = terrain::smoothstep(0.08f, 0.85f, river) * 0.09f;
    const float slopeLightening = terrain::smoothstep(0.05f, 0.28f, 1.0f - slope) * 0.045f;
    const float shade =
        0.92f + h * 0.12f - cavity * 0.08f - riverDarkening + slopeLightening + (terraceNoise - 0.5f) * 0.03f;
    color = scaleColor(color, shade);

    const Color3 coolShadowTint{0.97f, 0.98f, 1.00f};
    const float coolMix = cavity * 0.02f;
    color = mixColor(color, mulColor(color, coolShadowTint), coolMix);

    return clampColor(color, 0.0f, 1.0f);
}

Color3 heatmapColor(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    if (t < 0.33f) {
        return mixColor({0.06f, 0.12f, 0.42f}, {0.17f, 0.62f, 0.86f}, t / 0.33f);
    }
    if (t < 0.66f) {
        return mixColor({0.17f, 0.62f, 0.86f}, {0.90f, 0.82f, 0.24f}, (t - 0.33f) / 0.33f);
    }
    return mixColor({0.90f, 0.82f, 0.24f}, {0.82f, 0.22f, 0.14f}, (t - 0.66f) / 0.34f);
}

Color3 precipitationColor(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    if (t < 0.5f) {
        return mixColor({0.74f, 0.61f, 0.38f}, {0.42f, 0.68f, 0.28f}, t / 0.5f);
    }
    return mixColor({0.42f, 0.68f, 0.28f}, {0.12f, 0.38f, 0.63f}, (t - 0.5f) / 0.5f);
}

Color3 moistureColor(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    if (t < 0.5f) {
        return mixColor({0.63f, 0.52f, 0.34f}, {0.31f, 0.55f, 0.20f}, t / 0.5f);
    }
    return mixColor({0.31f, 0.55f, 0.20f}, {0.06f, 0.44f, 0.36f}, (t - 0.5f) / 0.5f);
}

Color3 slopeColor(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    if (t < 0.4f) {
        return mixColor({0.15f, 0.38f, 0.12f}, {0.57f, 0.56f, 0.26f}, t / 0.4f);
    }
    if (t < 0.75f) {
        return mixColor({0.57f, 0.56f, 0.26f}, {0.55f, 0.49f, 0.44f}, (t - 0.4f) / 0.35f);
    }
    return mixColor({0.55f, 0.49f, 0.44f}, {0.95f, 0.95f, 0.95f}, (t - 0.75f) / 0.25f);
}

Color3 debugVertexColor(const terrain::TerrainVertex& v, float minH, float maxH, Mode mode) {
    switch (mode) {
    case Mode::SurfaceBiomes:
        return biomeVertexColor(v, minH, maxH);
    case Mode::Landforms: {
        const terrain::BiomeColor c = terrain::landformColor(static_cast<terrain::LandformId>(v.landform));
        return {c.r, c.g, c.b};
    }
    case Mode::Ecology: {
        const terrain::BiomeColor c = terrain::ecologyColor(static_cast<terrain::EcologyId>(v.ecology));
        return {c.r, c.g, c.b};
    }
    case Mode::Temperature:
        return heatmapColor(v.temperature);
    case Mode::Precipitation:
        return precipitationColor(v.precipitation);
    case Mode::Moisture:
        return moistureColor(v.moisture);
    case Mode::Slope:
        return slopeColor(v.slope);
    }

    return biomeVertexColor(v, minH, maxH);
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
    std::cerr << "Shader compilation failed:\n"
              << log << '\n';
    glfn::DeleteShader(shader);
    return 0u;
}

} // namespace

bool usesMaterials(Mode mode) {
    return mode == Mode::SurfaceBiomes;
}

void rebuildTerrainColorBuffer(
    const terrain::TerrainMesh& mesh,
    Mode mode,
    std::vector<float>& terrainColors) {
    terrainColors.resize(mesh.vertices.size() * 3u);

    float precipMin = 0.0f;
    float precipMax = 1.0f;
    if (mode == Mode::Precipitation && !mesh.vertices.empty()) {
        precipMin = mesh.vertices.front().precipitation;
        precipMax = mesh.vertices.front().precipitation;
        for (const terrain::TerrainVertex& v : mesh.vertices) {
            precipMin = std::min(precipMin, v.precipitation);
            precipMax = std::max(precipMax, v.precipitation);
        }
    }

    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        Color3 color = debugVertexColor(mesh.vertices[i], mesh.minHeight, mesh.maxHeight, mode);
        if (mode == Mode::Precipitation) {
            const float normalized = (mesh.vertices[i].precipitation - precipMin) / std::max(0.0001f, precipMax - precipMin);
            color = precipitationColor(normalized);
        }
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
            const float ridge = 1.0f - std::abs(valueNoise2D(fx * 3.4f, fy * 3.4f, seed + 131u) * 2.0f - 1.0f);
            const float strata =
                0.5f + 0.5f * std::sin((fx * 0.85f + fy * 0.55f) * kPi + (mid - 0.5f) * 3.6f);
            float n = low * 0.42f + mid * 0.24f + hi * 0.13f + ridge * 0.13f + strata * 0.08f;
            n = lerp(n, grain, grainMix);
            n = std::clamp((n - 0.5f) * contrast + 0.5f, 0.0f, 1.0f);
            const float highlight = 0.90f + (hi - 0.5f) * 0.22f + (grain - 0.5f) * 0.10f;

            Color3 c{
                lerp(base.r, tint.r, n),
                lerp(base.g, tint.g, n),
                lerp(base.b, tint.b, n),
            };
            c.r = std::clamp(c.r * highlight, 0.0f, 1.0f);
            c.g = std::clamp(c.g * highlight, 0.0f, 1.0f);
            c.b = std::clamp(c.b * highlight, 0.0f, 1.0f);
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
    std::cerr << "Program link failed:\n"
              << log << '\n';
    glfn::DeleteProgram(program);
    return 0u;
}

void setUniform(GLint location, const Mat4& value) {
    if (location >= 0) {
        glfn::UniformMatrix4fv(location, 1, GL_FALSE, value.m);
    }
}

void setUniform(GLint location, int value) {
    if (location >= 0) {
        glfn::Uniform1i(location, value);
    }
}

void setUniform(GLint location, float value) {
    if (location >= 0) {
        glfn::Uniform1f(location, value);
    }
}

void setUniform(GLint location, const Vec3& value) {
    if (location >= 0) {
        glfn::Uniform3f(location, value.x, value.y, value.z);
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

} // namespace renderer
