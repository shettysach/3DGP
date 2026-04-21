#include "renderer.h"
#include "terrain/biomes.h"
#include "terrain/util.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

namespace renderer
{

namespace
{

constexpr float kPi = 3.14159265358979323846f;

constexpr GLfloat kLightDir[] = {0.35f, 1.0f, 0.25f, 0.0f};

float degToRad(float deg)
{
    return deg * kPi / 180.0f;
}

bool usesLighting(RenderMode mode)
{
    return mode == RenderMode::SurfaceBiomes;
}

struct Color3
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

struct Color4
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
};

void yawDirections(float yawDeg, float& forwardX, float& forwardZ, float& rightX, float& rightZ)
{
    const float yawRad = degToRad(yawDeg);
    forwardX = std::cos(yawRad);
    forwardZ = std::sin(yawRad);
    rightX = std::cos(yawRad + kPi * 0.5f);
    rightZ = std::sin(yawRad + kPi * 0.5f);
}

Color3 lerpColor(const Color3& a, const Color3& b, float t)
{
    return {
        terrain::lerp(a.r, b.r, t),
        terrain::lerp(a.g, b.g, t),
        terrain::lerp(a.b, b.b, t),
    };
}

Color3 biomeVertexColor(const terrain::TerrainVertex& v, float minH, float maxH)
{
    const float h = (v.y - minH) / std::max(0.001f, maxH - minH);
    const float slope = std::clamp(v.slope, 0.0f, 1.0f);
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

    const float shade = 0.90f + h * 0.20f;
    color.r = std::clamp(color.r * shade, 0.0f, 1.0f);
    color.g = std::clamp(color.g * shade, 0.0f, 1.0f);
    color.b = std::clamp(color.b * shade, 0.0f, 1.0f);
    return color;
}

Color3 heatmapColor(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    if (t < 0.33f)
    {
        return lerpColor({0.06f, 0.12f, 0.42f}, {0.17f, 0.62f, 0.86f}, t / 0.33f);
    }
    if (t < 0.66f)
    {
        return lerpColor({0.17f, 0.62f, 0.86f}, {0.90f, 0.82f, 0.24f}, (t - 0.33f) / 0.33f);
    }
    return lerpColor({0.90f, 0.82f, 0.24f}, {0.82f, 0.22f, 0.14f}, (t - 0.66f) / 0.34f);
}

Color3 precipitationColor(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    if (t < 0.5f)
    {
        return lerpColor({0.74f, 0.61f, 0.38f}, {0.42f, 0.68f, 0.28f}, t / 0.5f);
    }
    return lerpColor({0.42f, 0.68f, 0.28f}, {0.12f, 0.38f, 0.63f}, (t - 0.5f) / 0.5f);
}

Color3 moistureColor(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    if (t < 0.5f)
    {
        return lerpColor({0.63f, 0.52f, 0.34f}, {0.31f, 0.55f, 0.20f}, t / 0.5f);
    }
    return lerpColor({0.31f, 0.55f, 0.20f}, {0.06f, 0.44f, 0.36f}, (t - 0.5f) / 0.5f);
}

Color3 slopeColor(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    if (t < 0.4f)
    {
        return lerpColor({0.15f, 0.38f, 0.12f}, {0.57f, 0.56f, 0.26f}, t / 0.4f);
    }
    if (t < 0.75f)
    {
        return lerpColor({0.57f, 0.56f, 0.26f}, {0.55f, 0.49f, 0.44f}, (t - 0.4f) / 0.35f);
    }
    return lerpColor({0.55f, 0.49f, 0.44f}, {0.95f, 0.95f, 0.95f}, (t - 0.75f) / 0.25f);
}

Color3 debugVertexColor(const terrain::TerrainVertex& v, float minH, float maxH, RenderMode mode)
{
    switch (mode)
    {
    case RenderMode::SurfaceBiomes:
        return biomeVertexColor(v, minH, maxH);
    case RenderMode::Provinces:
    {
        const terrain::BiomeColor c = terrain::provinceColor(v.provinceId);
        return {c.r, c.g, c.b};
    }
    case RenderMode::Landforms:
    {
        const terrain::BiomeColor c = terrain::landformColor(static_cast<terrain::LandformId>(v.landform));
        return {c.r, c.g, c.b};
    }
    case RenderMode::Ecology:
    {
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

Color4 waterVertexColor(const terrain::TerrainVertex& v)
{
    const float t = terrain::smoothstep(0.02f, 0.85f, std::clamp(v.riverWeight, 0.0f, 1.0f));
    return {
        terrain::lerp(0.03f, 0.09f, t),
        terrain::lerp(0.28f, 0.55f, t),
        terrain::lerp(0.58f, 0.72f, t),
        terrain::lerp(0.0f, 0.76f, t),
    };
}

void rebuildTerrainColorBuffer(
    const terrain::TerrainMesh& mesh,
    RenderMode mode,
    std::vector<float>& terrainColors)
{
    terrainColors.resize(mesh.vertices.size() * 3u);
    for (size_t i = 0; i < mesh.vertices.size(); ++i)
    {
        const Color3 color = debugVertexColor(mesh.vertices[i], mesh.minHeight, mesh.maxHeight, mode);
        const size_t base = i * 3u;
        terrainColors[base + 0u] = color.r;
        terrainColors[base + 1u] = color.g;
        terrainColors[base + 2u] = color.b;
    }
}

void rebuildWaterColorBuffer(const terrain::TerrainMesh& mesh, std::vector<float>& waterColors)
{
    waterColors.resize(mesh.waterVertices.size() * 4u);
    for (size_t i = 0; i < mesh.waterVertices.size(); ++i)
    {
        const Color4 color = waterVertexColor(mesh.waterVertices[i]);
        const size_t base = i * 4u;
        waterColors[base + 0u] = color.r;
        waterColors[base + 1u] = color.g;
        waterColors[base + 2u] = color.b;
        waterColors[base + 3u] = color.a;
    }
}

} // namespace

const char* renderModeName(RenderMode mode)
{
    switch (mode)
    {
    case RenderMode::SurfaceBiomes:
        return "Surface biomes";
    case RenderMode::Provinces:
        return "Provinces";
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
      terrainColorsValid_(false),
      waterColorsValid_(false),
      cachedTerrainVertexCount_(0u),
      cachedWaterVertexCount_(0u) {}

Renderer::~Renderer()
{
    shutdown();
}

bool Renderer::init()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_DisplayMode displayMode;
    SDL_GetCurrentDisplayMode(0, &displayMode);
    width_ = displayMode.w;
    height_ = displayMode.h;

    window_ = SDL_CreateWindow("Terrain Generator Demo",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               width_,
                               height_,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window_)
    {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        return false;
    }

    glContext_ = SDL_GL_CreateContext(window_);
    if (!glContext_)
    {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << '\n';
        return false;
    }

    SDL_GL_SetSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    glCullFace(GL_BACK);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    const GLfloat ambient[] = {0.25f, 0.25f, 0.27f, 1.0f};
    const GLfloat diffuse[] = {0.95f, 0.90f, 0.85f, 1.0f};
    const GLfloat specular[] = {0.3f, 0.3f, 0.3f, 1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_POSITION, kLightDir);

    glClearColor(0.67f, 0.78f, 0.92f, 1.0f);

    std::cout << "GL Vendor: " << glGetString(GL_VENDOR) << '\n';
    std::cout << "GL Renderer: " << glGetString(GL_RENDERER) << '\n';
    std::cout << "GL Version: " << glGetString(GL_VERSION) << '\n';
    return true;
}

void Renderer::shutdown()
{
    if (glContext_)
    {
        SDL_GL_DeleteContext(glContext_);
        glContext_ = nullptr;
    }
    if (window_)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

bool Renderer::shouldClose() const
{
    return shouldClose_;
}

void Renderer::swapBuffers()
{
    if (window_)
    {
        SDL_GL_SwapWindow(window_);
    }
}

void Renderer::orbit(float deltaYaw, float deltaPitch)
{
    yawDeg_ += deltaYaw;
    pitchDeg_ += deltaPitch;
    pitchDeg_ = std::clamp(pitchDeg_, -30.0f, 88.0f);
}

void Renderer::zoom(float deltaDistance)
{
    distance_ += deltaDistance;
    distance_ = std::clamp(distance_, 18.0f, 1600.0f);
}

void Renderer::pan(float deltaX, float deltaY)
{
    float fX, fZ, rX, rZ;
    yawDirections(yawDeg_, fX, fZ, rX, rZ);
    targetX_ += rX * deltaX;
    targetZ_ += rZ * deltaX;
    targetY_ += deltaY;
}

void Renderer::moveForward(float amount)
{
    float fX, fZ, rX, rZ;
    yawDirections(yawDeg_, fX, fZ, rX, rZ);
    targetX_ += fX * amount;
    targetZ_ += fZ * amount;
}

void Renderer::moveRight(float amount)
{
    float fX, fZ, rX, rZ;
    yawDirections(yawDeg_, fX, fZ, rX, rZ);
    targetX_ += rX * amount;
    targetZ_ += rZ * amount;
}

void Renderer::setTarget(float x, float y, float z)
{
    targetX_ = x;
    targetY_ = y;
    targetZ_ = z;
}

void Renderer::setRenderMode(RenderMode mode)
{
    if (renderMode_ != mode)
    {
        terrainColorsValid_ = false;
    }
    renderMode_ = mode;
}

RenderMode Renderer::renderMode() const
{
    return renderMode_;
}

void Renderer::invalidateMeshCache()
{
    terrainColorsValid_ = false;
    waterColorsValid_ = false;
    cachedTerrainVertexCount_ = 0u;
    cachedWaterVertexCount_ = 0u;
    terrainColors_.clear();
    waterColors_.clear();
}

bool Renderer::captureScreenshot(const std::string& filepath) const
{
    if (!window_)
    {
        return false;
    }

    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GL_GetDrawableSize(window_, &drawableWidth, &drawableHeight);
    if (drawableWidth <= 0 || drawableHeight <= 0)
    {
        return false;
    }

    std::vector<unsigned char> pixels(
        static_cast<size_t>(drawableWidth) * static_cast<size_t>(drawableHeight) * 3u,
        0u);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, drawableWidth, drawableHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR)
    {
        return false;
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0,
        drawableWidth,
        drawableHeight,
        24,
        SDL_PIXELFORMAT_RGB24);
    if (!surface)
    {
        return false;
    }

    const size_t rowBytes = static_cast<size_t>(drawableWidth) * 3u;
    for (int y = 0; y < drawableHeight; ++y)
    {
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

void Renderer::render(const terrain::TerrainMesh& mesh)
{
    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GL_GetDrawableSize(window_, &drawableWidth, &drawableHeight);
    if (drawableWidth <= 0 || drawableHeight <= 0)
    {
        return;
    }

    width_ = drawableWidth;
    height_ = drawableHeight;

    glViewport(0, 0, drawableWidth, drawableHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, static_cast<double>(drawableWidth) / static_cast<double>(drawableHeight), 0.5, 5000.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    const float yawRad = degToRad(yawDeg_);
    const float pitchRad = degToRad(pitchDeg_);

    const float eyeX = targetX_ + distance_ * std::cos(pitchRad) * std::cos(yawRad);
    const float eyeY = targetY_ + distance_ * std::sin(pitchRad);
    const float eyeZ = targetZ_ + distance_ * std::cos(pitchRad) * std::sin(yawRad);

    gluLookAt(eyeX, eyeY, eyeZ, targetX_, targetY_, targetZ_, 0.0, 1.0, 0.0);
    glLightfv(GL_LIGHT0, GL_POSITION, kLightDir);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    const bool terrainUsesLighting = usesLighting(renderMode_);
    if (terrainUsesLighting)
    {
        glEnable(GL_LIGHTING);
    }
    else
    {
        glDisable(GL_LIGHTING);
    }

    if (!terrainColorsValid_ || cachedTerrainVertexCount_ != mesh.vertices.size())
    {
        rebuildTerrainColorBuffer(mesh, renderMode_, terrainColors_);
        cachedTerrainVertexCount_ = mesh.vertices.size();
        terrainColorsValid_ = true;
    }

    if (!mesh.vertices.empty() && !mesh.indices.empty())
    {
        const GLsizei stride = static_cast<GLsizei>(sizeof(terrain::TerrainVertex));
        const unsigned char* vertexBase = reinterpret_cast<const unsigned char*>(mesh.vertices.data());

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);

        glVertexPointer(3, GL_FLOAT, stride, vertexBase + offsetof(terrain::TerrainVertex, x));
        glNormalPointer(GL_FLOAT, stride, vertexBase + offsetof(terrain::TerrainVertex, nx));
        glColorPointer(3, GL_FLOAT, 0, terrainColors_.data());

        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(mesh.indices.size()),
            GL_UNSIGNED_INT,
            mesh.indices.data());

        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    }

    if (terrainUsesLighting && !mesh.waterVertices.empty() && !mesh.waterIndices.empty())
    {
        if (!waterColorsValid_ || cachedWaterVertexCount_ != mesh.waterVertices.size())
        {
            rebuildWaterColorBuffer(mesh, waterColors_);
            cachedWaterVertexCount_ = mesh.waterVertices.size();
            waterColorsValid_ = true;
        }

        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);

        const GLsizei stride = static_cast<GLsizei>(sizeof(terrain::TerrainVertex));
        const unsigned char* waterBase = reinterpret_cast<const unsigned char*>(mesh.waterVertices.data());

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);

        glVertexPointer(3, GL_FLOAT, stride, waterBase + offsetof(terrain::TerrainVertex, x));
        glColorPointer(4, GL_FLOAT, 0, waterColors_.data());

        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(mesh.waterIndices.size()),
            GL_UNSIGNED_INT,
            mesh.waterIndices.data());

        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);

        glDisable(GL_POLYGON_OFFSET_FILL);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glEnable(GL_LIGHTING);
    }
    else if (!terrainUsesLighting)
    {
        glEnable(GL_LIGHTING);
    }
}

void runDemo()
{
    terrain::TerrainSettings settings;
    settings.width = 513;
    settings.depth = 513;
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
    settings.rivers.sourceDensity = 0.00018f;
    settings.rivers.sourceAccumulation = 85.0f;
    settings.rivers.mainAccumulation = 200.0f;
    settings.rivers.maxHalfWidth = 3;
    settings.rivers.baseCarveFraction = 0.025f;
    settings.rivers.maxCarveFraction = 0.09f;

    terrain::TerrainGenerator generator(settings);
    terrain::TerrainMesh mesh = generator.generateMesh();

    auto printRiverStats = [&mesh]()
    {
        size_t wetCount = 0;
        float maxWeight = 0.0f;
        for (const terrain::TerrainVertex& v : mesh.vertices)
        {
            if (v.riverWeight > 0.02f)
            {
                ++wetCount;
            }
            maxWeight = std::max(maxWeight, v.riverWeight);
        }

        const float coverage = mesh.vertices.empty()
                                   ? 0.0f
                                   : 100.0f * static_cast<float>(wetCount) /
                                         static_cast<float>(mesh.vertices.size());
        std::cout << "River coverage: " << coverage << "%, max weight: " << maxWeight
                  << ", settlements: " << mesh.settlementVertices.size() << '\n';
    };

    auto printBiomeStats = [&mesh]()
    {
        std::array<size_t, static_cast<size_t>(terrain::BiomeId::Count)> counts{};
        uint16_t maxProvinceId = 0u;
        for (const terrain::TerrainVertex& v : mesh.vertices)
        {
            ++counts[static_cast<size_t>(v.primaryBiome)];
            maxProvinceId = std::max(maxProvinceId, v.provinceId);
        }

        std::cout << "Surface coverage across " << (mesh.vertices.empty() ? 0u : static_cast<unsigned>(maxProvinceId + 1u)) << " provinces:";
        const float invTotal = mesh.vertices.empty() ? 0.0f : 100.0f / static_cast<float>(mesh.vertices.size());
        for (size_t idx = 0; idx < counts.size(); ++idx)
        {
            if (counts[idx] == 0)
            {
                continue;
            }
            std::cout << ' ' << terrain::biomeName(static_cast<terrain::BiomeId>(idx)) << ' '
                      << counts[idx] * invTotal << '%';
        }
        std::cout << '\n';
    };

    Renderer renderer(1280, 800);
    if (!renderer.init())
    {
        std::cerr << "Renderer init failed\n";
        return;
    }

    auto setMode = [&renderer](RenderMode mode)
    {
        renderer.setRenderMode(mode);
        std::cout << "Render mode: " << renderModeName(mode) << '\n';
    };

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
    std::cout << "  1/2/3: river preset (light/medium/heavy)\n";
    std::cout << "  F/V/L/B/T/Y/M/K: surface / provinces / landforms / ecology / temperature / precipitation / moisture / slope\n";
    std::cout << "  P: save screenshot\n";
    std::cout << "  ESC: quit\n";
    printRiverStats();
    printBiomeStats();

    SDL_Event event;
    bool orbiting = false;
    bool panning = false;
    int prevMouseX = 0;
    int prevMouseY = 0;
    using Clock = std::chrono::steady_clock;
    auto lastFrameTime = Clock::now();

    const auto applyRiverPreset = [&](int preset)
    {
        if (preset == 1)
        {
            settings.rivers.sourceDensity = 0.00010f;
            settings.rivers.sourceAccumulation = 110.0f;
            settings.rivers.mainAccumulation = 240.0f;
            settings.rivers.maxHalfWidth = 2;
            settings.rivers.baseCarveFraction = 0.018f;
            settings.rivers.maxCarveFraction = 0.065f;
        }
        else if (preset == 2)
        {
            settings.rivers.sourceDensity = 0.00018f;
            settings.rivers.sourceAccumulation = 85.0f;
            settings.rivers.mainAccumulation = 200.0f;
            settings.rivers.maxHalfWidth = 3;
            settings.rivers.baseCarveFraction = 0.025f;
            settings.rivers.maxCarveFraction = 0.09f;
        }
        else
        {
            settings.rivers.sourceDensity = 0.00030f;
            settings.rivers.sourceAccumulation = 60.0f;
            settings.rivers.mainAccumulation = 150.0f;
            settings.rivers.maxHalfWidth = 4;
            settings.rivers.baseCarveFraction = 0.035f;
            settings.rivers.maxCarveFraction = 0.12f;
        }

        generator.setSettings(settings);
        mesh = generator.generateMesh();
        renderer.invalidateMeshCache();
        std::cout << "Applied river preset " << preset << '\n';
        printRiverStats();
        printBiomeStats();
    };

    while (!renderer.shouldClose())
    {
        const auto frameTime = Clock::now();
        const float deltaSeconds = std::clamp(
            std::chrono::duration<float>(frameTime - lastFrameTime).count(),
            0.0f,
            0.1f);
        lastFrameTime = frameTime;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                return;
            }

            if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    return;
                }
                if (event.key.keysym.sym == SDLK_r)
                {
                    settings.seed += 1u;
                    generator.setSettings(settings);
                    mesh = generator.generateMesh();
                    renderer.invalidateMeshCache();
                    std::cout << "Regenerated terrain with seed " << settings.seed << '\n';
                    printRiverStats();
                    printBiomeStats();
                }
                if (event.key.keysym.sym == SDLK_1)
                {
                    applyRiverPreset(1);
                }
                if (event.key.keysym.sym == SDLK_2)
                {
                    applyRiverPreset(2);
                }
                if (event.key.keysym.sym == SDLK_3)
                {
                    applyRiverPreset(3);
                }
                if (event.key.keysym.sym == SDLK_f)
                {
                    setMode(RenderMode::SurfaceBiomes);
                }
                if (event.key.keysym.sym == SDLK_v)
                {
                    setMode(RenderMode::Provinces);
                }
                if (event.key.keysym.sym == SDLK_l)
                {
                    setMode(RenderMode::Landforms);
                }
                if (event.key.keysym.sym == SDLK_b)
                {
                    setMode(RenderMode::Ecology);
                }
                if (event.key.keysym.sym == SDLK_t)
                {
                    setMode(RenderMode::Temperature);
                }
                if (event.key.keysym.sym == SDLK_y)
                {
                    setMode(RenderMode::Precipitation);
                }
                if (event.key.keysym.sym == SDLK_m)
                {
                    setMode(RenderMode::Moisture);
                }
                if (event.key.keysym.sym == SDLK_k)
                {
                    setMode(RenderMode::Slope);
                }
                if (event.key.keysym.sym == SDLK_p)
                {
                    const std::time_t now = std::time(nullptr);
                    const std::string screenshotPath =
                        "screenshot_" + std::to_string(static_cast<long long>(now)) + ".bmp";
                    if (renderer.captureScreenshot(screenshotPath))
                    {
                        std::cout << "Saved screenshot to " << screenshotPath << '\n';
                    }
                    else
                    {
                        std::cout << "Failed to save screenshot\n";
                    }
                }
            }

            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
            {
                orbiting = true;
                prevMouseX = event.button.x;
                prevMouseY = event.button.y;
            }
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
            {
                orbiting = false;
            }

            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT)
            {
                panning = true;
                prevMouseX = event.button.x;
                prevMouseY = event.button.y;
            }
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT)
            {
                panning = false;
            }

            if (event.type == SDL_MOUSEMOTION)
            {
                const int dx = event.motion.x - prevMouseX;
                const int dy = event.motion.y - prevMouseY;

                if (orbiting)
                {
                    renderer.orbit(static_cast<float>(dx) * 0.28f, static_cast<float>(-dy) * 0.28f);
                }
                if (panning)
                {
                    const float panScale = std::max(0.3f, 0.005f * mesh.horizontalScale * 300.0f);
                    renderer.pan(static_cast<float>(-dx) * panScale, static_cast<float>(dy) * panScale);
                }

                prevMouseX = event.motion.x;
                prevMouseY = event.motion.y;
            }

            if (event.type == SDL_MOUSEWHEEL)
            {
                renderer.zoom(static_cast<float>(-event.wheel.y) * 14.0f);
            }
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        const float moveSpeed = 170.0f * deltaSeconds;
        if (keys[SDL_SCANCODE_W])
        {
            renderer.moveForward(-moveSpeed);
        }
        if (keys[SDL_SCANCODE_S])
        {
            renderer.moveForward(moveSpeed);
        }
        if (keys[SDL_SCANCODE_A])
        {
            renderer.moveRight(moveSpeed);
        }
        if (keys[SDL_SCANCODE_D])
        {
            renderer.moveRight(-moveSpeed);
        }
        if (keys[SDL_SCANCODE_Q])
        {
            renderer.pan(0.0f, -moveSpeed);
        }
        if (keys[SDL_SCANCODE_E])
        {
            renderer.pan(0.0f, moveSpeed);
        }

        renderer.render(mesh);
        renderer.swapBuffers();
    }
}

} // namespace renderer
