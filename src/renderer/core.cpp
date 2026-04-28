#include "renderer/internal.h"
#include "renderer/shaders.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <vector>

namespace renderer {

const char* modeName(Mode mode) {
    switch (mode) {
    case Mode::SurfaceBiomes:
        return "Surface biomes";
    case Mode::Landforms:
        return "Landforms";
    case Mode::Ecology:
        return "Ecology";
    case Mode::Temperature:
        return "Temperature";
    case Mode::Precipitation:
        return "Precipitation";
    case Mode::Moisture:
        return "Moisture";
    case Mode::Slope:
        return "Slope";
    }

    return "Uncreachable modeName";
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
      mode_(Mode::SurfaceBiomes),
      terrainBuffersValid_(false),
      terrainColorsValid_(false),
      cachedTerrainVertexCount_(0u),
      cachedTerrainIndexCount_(0u),
      terrainVao_(0u),
      terrainVbo_(0u),
      terrainIbo_(0u),
      terrainProgram_(0u),
      grassTexture_(0u),
      rockTexture_(0u),
      snowTexture_(0u),
      sandTexture_(0u),
      profileNextFrame_(true),
      pendingProfileReason_("startup") {}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::init() {
    using Clock = std::chrono::steady_clock;
    const auto initStart = Clock::now();
    const auto stageMs = [](const Clock::time_point& start, const Clock::time_point& end) {
        return std::chrono::duration<double, std::milli>(end - start).count();
    };

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
    const auto glReady = Clock::now();

    SDL_GL_SetSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    glCullFace(GL_BACK);
    glEnable(GL_MULTISAMPLE);
    glClearColor(kFogHorizonColor.x, kFogHorizonColor.y, kFogHorizonColor.z, 1.0f);

    terrainProgram_ = createProgram(kTerrainVertexShader, kTerrainFragmentShader);
    if (terrainProgram_ == 0u) {
        return false;
    }

    const auto cacheUniform = [](GLuint program, const char* name) {
        return glfn::GetUniformLocation(program, name);
    };
    terrainUniforms_.viewProj = cacheUniform(terrainProgram_, "uViewProj");
    terrainUniforms_.sunLightDir = cacheUniform(terrainProgram_, "uSunLightDir");
    terrainUniforms_.sunColor = cacheUniform(terrainProgram_, "uSunColor");
    terrainUniforms_.ambientColor = cacheUniform(terrainProgram_, "uAmbientColor");
    terrainUniforms_.enableMaterials = cacheUniform(terrainProgram_, "uEnableMaterials");
    terrainUniforms_.grassTex = cacheUniform(terrainProgram_, "uGrassTex");
    terrainUniforms_.rockTex = cacheUniform(terrainProgram_, "uRockTex");
    terrainUniforms_.sandTex = cacheUniform(terrainProgram_, "uSandTex");
    terrainUniforms_.snowTex = cacheUniform(terrainProgram_, "uSnowTex");

    glfn::UseProgram(terrainProgram_);
    setUniform(terrainUniforms_.grassTex, 0);
    setUniform(terrainUniforms_.rockTex, 1);
    setUniform(terrainUniforms_.sandTex, 2);
    setUniform(terrainUniforms_.snowTex, 3);
    glfn::UseProgram(0);
    const auto shadersReady = Clock::now();
    const auto shadowReady = shadersReady;

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
    const auto texturesReady = Clock::now();

    std::cout << "GL Vendor: " << glGetString(GL_VENDOR) << '\n';
    std::cout << "GL Renderer: " << glGetString(GL_RENDERER) << '\n';
    std::cout << "GL Version: " << glGetString(GL_VERSION) << '\n';
    std::cout << "[profile] renderer.init gl=" << stageMs(initStart, glReady) << "ms"
              << " shaders=" << stageMs(glReady, shadersReady) << "ms"
              << " textures=" << stageMs(shadersReady, texturesReady) << "ms"
              << " total=" << stageMs(initStart, texturesReady) << "ms\n";
    return true;
}

void Renderer::shutdown() {
    destroyTexture(grassTexture_);
    destroyTexture(rockTexture_);
    destroyTexture(snowTexture_);
    destroyTexture(sandTexture_);

    destroyProgram(terrainProgram_);

    destroyBuffer(terrainVbo_);
    destroyBuffer(terrainIbo_);
    destroyVertexArray(terrainVao_);

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

void Renderer::setMode(Mode mode) {
    if (mode_ != mode) {
        terrainColorsValid_ = false;
        profileNextFrame_ = true;
        pendingProfileReason_ = "mode change";
    }
    mode_ = mode;
}

Mode Renderer::mode() const {
    return mode_;
}

void Renderer::invalidateMeshCache() {
    terrainBuffersValid_ = false;
    terrainColorsValid_ = false;
    cachedTerrainVertexCount_ = 0u;
    cachedTerrainIndexCount_ = 0u;
    terrainBaseColors_.clear();
    profileNextFrame_ = true;
    pendingProfileReason_ = "mesh rebuild";
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
    using Clock = std::chrono::steady_clock;
    const auto frameStart = Clock::now();
    const auto elapsedMs = [](const Clock::time_point& start, const Clock::time_point& end) {
        return std::chrono::duration<double, std::milli>(end - start).count();
    };
    double terrainUploadMs = 0.0;
    double sceneSubmitMs = 0.0;

    if (!window_) {
        return;
    }

    if (!terrainColorsValid_ || !terrainBuffersValid_ || cachedTerrainVertexCount_ != mesh.vertices.size() ||
        cachedTerrainIndexCount_ != mesh.indices.size()) {
        const auto uploadStart = Clock::now();
        rebuildTerrainColorBuffer(mesh, mode_, terrainBaseColors_);

        std::vector<TerrainGpuVertex> terrainVertices(mesh.vertices.size());
        const float invHeightRange = 1.0f / std::max(0.001f, mesh.maxHeight - mesh.minHeight);
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            const terrain::TerrainVertex& v = mesh.vertices[i];
            const size_t colorBase = i * 3u;
            terrainVertices[i] = TerrainGpuVertex{
                {v.x, v.y, v.z},
                {v.nx, v.ny, v.nz},
                {terrainBaseColors_[colorBase], terrainBaseColors_[colorBase + 1u], terrainBaseColors_[colorBase + 2u]},
                {std::clamp(v.slope, 0.0f, 1.0f), std::clamp(v.mountainWeight, 0.0f, 1.0f), std::clamp(v.riverWeight, 0.0f, 1.0f), std::clamp(v.moisture, 0.0f, 1.0f)},
                {std::clamp((v.y - mesh.minHeight) * invHeightRange, 0.0f, 1.0f), std::clamp(v.temperature, 0.0f, 1.0f), std::clamp(v.precipitation, 0.0f, 1.0f), 0.0f}
            };
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
        terrainUploadMs = elapsedMs(uploadStart, Clock::now());
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

    const Vec3 sunLightDir = normalize(Vec3{-kSunDirection.x, -kSunDirection.y, -kSunDirection.z});
    const bool enableMaterials = usesMaterials(mode_);

    glEnable(GL_DEPTH_TEST);

    glViewport(0, 0, drawableWidth, drawableHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const auto sceneStart = Clock::now();

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);

    glfn::UseProgram(terrainProgram_);
    setUniform(terrainUniforms_.viewProj, viewProjection);
    setUniform(terrainUniforms_.sunLightDir, sunLightDir);
    setUniform(terrainUniforms_.sunColor, kSunColor);
    setUniform(terrainUniforms_.ambientColor, kAmbientColor);
    setUniform(terrainUniforms_.enableMaterials, enableMaterials ? 1 : 0);

    glfn::ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, grassTexture_);
    glfn::ActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, rockTexture_);
    glfn::ActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, sandTexture_);
    glfn::ActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, snowTexture_);

    glfn::BindVertexArray(terrainVao_);
    glDrawElements(
        GL_TRIANGLES,
        static_cast<GLsizei>(cachedTerrainIndexCount_),
        GL_UNSIGNED_INT,
        nullptr);
    glfn::BindVertexArray(0);

    glfn::ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glfn::UseProgram(0);
    sceneSubmitMs = elapsedMs(sceneStart, Clock::now());

    if (profileNextFrame_) {
        const double cpuTotalMs = elapsedMs(frameStart, Clock::now());
        glFinish();
        const double gpuTotalMs = elapsedMs(frameStart, Clock::now());
        const double terrainVertexMiB =
            static_cast<double>(mesh.vertices.size() * sizeof(TerrainGpuVertex)) / (1024.0 * 1024.0);
        const double terrainIndexMiB =
            static_cast<double>(mesh.indices.size() * sizeof(uint32_t)) / (1024.0 * 1024.0);
        std::cout << "[profile] renderer.frame reason=" << pendingProfileReason_
                  << " terrainUpload=" << terrainUploadMs << "ms"
                  << " sceneSubmit=" << sceneSubmitMs << "ms"
                  << " cpuTotal=" << cpuTotalMs << "ms"
                  << " gpuTotal=" << gpuTotalMs << "ms"
                  << " terrainVerts=" << mesh.vertices.size()
                  << " terrainTris=" << (mesh.indices.size() / 3u)
                  << " terrainVB=" << terrainVertexMiB << "MiB"
                  << " terrainIB=" << terrainIndexMiB << "MiB" << '\n';
        profileNextFrame_ = false;
        pendingProfileReason_.clear();
    }
}

} // namespace renderer
