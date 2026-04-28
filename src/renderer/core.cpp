#include "renderer/internal.h"
#include "renderer/shaders.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace renderer {

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
      terrainBuffersValid_(false),
      cachedTerrainVertexCount_(0u),
      cachedTerrainIndexCount_(0u),
      terrainVao_(0u),
      terrainVbo_(0u),
      terrainIbo_(0u),
      terrainProgram_(0u),
      grassTexture_(0u),
      rockTexture_(0u),
      snowTexture_(0u),
      sandTexture_(0u) {}

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
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    glCullFace(GL_BACK);
    glClearColor(kFogHorizonColor.x, kFogHorizonColor.y, kFogHorizonColor.z, 1.0f);

    terrainProgram_ = createProgram(kTerrainVertexShader, kTerrainFragmentShader);
    if (terrainProgram_ == 0u) {
        return false;
    }

    const auto cacheUniform = [](GLuint program, const char* name) {
        return glfn::GetUniformLocation(program, name);
    };
    terrainUniforms_.viewProj = cacheUniform(terrainProgram_, "uViewProj");
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

void Renderer::invalidateMeshCache() {
    terrainBuffersValid_ = false;
    cachedTerrainVertexCount_ = 0u;
    cachedTerrainIndexCount_ = 0u;
    terrainBaseColors_.clear();
}

void Renderer::render(const terrain::TerrainMesh& mesh) {
    if (!window_) {
        return;
    }

    if (!terrainBuffersValid_ || cachedTerrainVertexCount_ != mesh.vertices.size() ||
        cachedTerrainIndexCount_ != mesh.indices.size()) {
        buildTerrainColorBuffer(mesh, terrainBaseColors_);

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
        terrainBuffersValid_ = true;
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

    glViewport(0, 0, drawableWidth, drawableHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glfn::UseProgram(terrainProgram_);
    setUniform(terrainUniforms_.viewProj, viewProjection);

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
}

} // namespace renderer
