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
#include <windows.h>
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>

#elif defined(__APPLE__)
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#elif defined(__linux__)
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#else
#error "Unknown platform"
#endif

namespace renderer {

enum class RenderMode {
    SurfaceBiomes = 0,
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
    bool terrainBuffersValid_;
    bool waterBuffersValid_;
    bool terrainColorsValid_;
    std::size_t cachedTerrainVertexCount_;
    std::size_t cachedTerrainIndexCount_;
    std::size_t cachedWaterVertexCount_;
    std::size_t cachedWaterIndexCount_;
    GLuint terrainVao_;
    GLuint terrainVbo_;
    GLuint terrainIbo_;
    GLuint waterVao_;
    GLuint waterVbo_;
    GLuint waterIbo_;
    GLuint skyVao_;
    GLuint terrainProgram_;
    GLuint skyProgram_;
    GLuint waterProgram_;
    GLuint shadowProgram_;
    GLuint shadowFramebuffer_;
    GLuint shadowDepthTexture_;
    GLuint grassTexture_;
    GLuint rockTexture_;
    GLuint snowTexture_;
    GLuint sandTexture_;
    GLuint waterTexture_;
    int shadowMapSize_;
    std::vector<float> terrainBaseColors_;
};

void runDemo();
const char* renderModeName(RenderMode mode);

} // namespace renderer

#endif // RENDERER_H
