#ifndef RENDERER_H
#define RENDERER_H

#include "terrain.h"

#include <cstddef>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <GL/gl.h>
#include <GL/glu.h>
#include <windows.h>
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>

#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <SDL2/SDL.h>

#elif defined(__linux__)
#include <GL/gl.h>
#include <GL/glu.h>
#include <SDL2/SDL.h>

#else
#error "Unknown platform"
#endif

namespace renderer {

enum class RenderMode {
    SurfaceBiomes = 0,
    Provinces,
    Landforms,
    Ecology,
    Temperature,
    Precipitation,
    Moisture,
    Slope,
};

class Renderer {
  public:
    Renderer(int width, int height);
    ~Renderer();

    bool init();
    void shutdown();

    bool shouldClose() const;
    void swapBuffers();

    void render(const terrain::TerrainMesh& mesh);
    void orbit(float deltaYaw, float deltaPitch);
    void zoom(float deltaDistance);
    void pan(float deltaX, float deltaY);
    void moveForward(float amount);
    void moveRight(float amount);
    void setTarget(float x, float y, float z);
    void setRenderMode(RenderMode mode);
    RenderMode renderMode() const;
    void invalidateMeshCache();
    bool captureScreenshot(const std::string& filepath) const;

  private:
    int width_;
    int height_;
    SDL_Window* window_;
    SDL_GLContext glContext_;
    bool shouldClose_;
    float yawDeg_;
    float pitchDeg_;
    float distance_;
    float targetX_;
    float targetY_;
    float targetZ_;
    RenderMode renderMode_;
    bool terrainColorsValid_;
    bool waterColorsValid_;
    std::size_t cachedTerrainVertexCount_;
    std::size_t cachedWaterVertexCount_;
    std::vector<float> terrainColors_;
    std::vector<float> waterColors_;
};

void runDemo();
const char* renderModeName(RenderMode mode);

} // namespace renderer

#endif // RENDERER_H
