#ifndef RENDERER_H
#define RENDERER_H

#include "terrain.h"

#ifdef _WIN32
#include <SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>

#elif defined(__APPLE__)
#include <SDL2/SDL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>

#elif defined(__linux__)
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>

#else
#error "Unknown platform"
#endif

namespace renderer
{

class Renderer
{
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
};

void runDemo();

} // namespace renderer

#endif // RENDERER_H
