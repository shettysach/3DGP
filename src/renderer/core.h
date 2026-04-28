#ifndef CORE_H
#define CORE_H

#include <cstddef>
#include <string>
#include <vector>

#include "terrain.h"

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

struct TerrainUniformLocations {
    GLint viewProj = -1;
    GLint sunLightDir = -1;
    GLint sunColor = -1;
    GLint ambientColor = -1;
    GLint grassTex = -1;
    GLint rockTex = -1;
    GLint sandTex = -1;
    GLint snowTex = -1;
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

    SDL_Window* window() const {
        return window_;
    }

    SDL_GLContext glContext() const {
        return glContext_;
    }

    void invalidateMeshCache();

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
    bool terrainBuffersValid_;
    std::size_t cachedTerrainVertexCount_;
    std::size_t cachedTerrainIndexCount_;
    GLuint terrainVao_;
    GLuint terrainVbo_;
    GLuint terrainIbo_;
    GLuint terrainProgram_;
    GLuint grassTexture_;
    GLuint rockTexture_;
    GLuint snowTexture_;
    GLuint sandTexture_;
    TerrainUniformLocations terrainUniforms_;
    std::vector<float> terrainBaseColors_;
};

void runDemo();

} // namespace renderer

#endif // CORE_H
