#include "renderer.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace renderer
{

namespace
{

constexpr float kPi = 3.14159265358979323846f;

float degToRad(float deg)
{
    return deg * kPi / 180.0f;
}

void normalize3(float& x, float& y, float& z)
{
    const float len = std::sqrt(x * x + y * y + z * z);
    if (len > 0.000001f)
    {
        x /= len;
        y /= len;
        z /= len;
    }
}

} // namespace

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
      targetZ_(0.0f) {}

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
    glEnable(GL_NORMALIZE);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    const GLfloat ambient[] = {0.25f, 0.25f, 0.27f, 1.0f};
    const GLfloat diffuse[] = {0.95f, 0.90f, 0.85f, 1.0f};
    const GLfloat specular[] = {0.3f, 0.3f, 0.3f, 1.0f};
    const GLfloat lightDir[] = {0.35f, 1.0f, 0.25f, 0.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_POSITION, lightDir);

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
    if (pitchDeg_ > 88.0f)
    {
        pitchDeg_ = 88.0f;
    }
    if (pitchDeg_ < -30.0f)
    {
        pitchDeg_ = -30.0f;
    }
}

void Renderer::zoom(float deltaDistance)
{
    distance_ += deltaDistance;
    if (distance_ < 18.0f)
    {
        distance_ = 18.0f;
    }
    if (distance_ > 1600.0f)
    {
        distance_ = 1600.0f;
    }
}

void Renderer::pan(float deltaX, float deltaY)
{
    const float yawRad = degToRad(yawDeg_);
    float rightX = std::cos(yawRad + kPi * 0.5f);
    float rightY = 0.0f;
    float rightZ = std::sin(yawRad + kPi * 0.5f);
    normalize3(rightX, rightY, rightZ);

    const float upX = 0.0f;
    const float upY = 1.0f;
    const float upZ = 0.0f;

    targetX_ += rightX * deltaX + upX * deltaY;
    targetY_ += rightY * deltaX + upY * deltaY;
    targetZ_ += rightZ * deltaX + upZ * deltaY;
}

void Renderer::moveForward(float amount)
{
    const float yawRad = degToRad(yawDeg_);
    float forwardX = std::cos(yawRad);
    float forwardY = 0.0f;
    float forwardZ = std::sin(yawRad);
    normalize3(forwardX, forwardY, forwardZ);
    targetX_ += forwardX * amount;
    targetY_ += forwardY * amount;
    targetZ_ += forwardZ * amount;
}

void Renderer::moveRight(float amount)
{
    const float yawRad = degToRad(yawDeg_);
    float rightX = std::cos(yawRad + kPi * 0.5f);
    float rightY = 0.0f;
    float rightZ = std::sin(yawRad + kPi * 0.5f);
    normalize3(rightX, rightY, rightZ);
    targetX_ += rightX * amount;
    targetY_ += rightY * amount;
    targetZ_ += rightZ * amount;
}

void Renderer::setTarget(float x, float y, float z)
{
    targetX_ = x;
    targetY_ = y;
    targetZ_ = z;
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
    glViewport(0, 0, width_, height_);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, static_cast<double>(width_) / static_cast<double>(height_), 0.5, 5000.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    const float yawRad = degToRad(yawDeg_);
    const float pitchRad = degToRad(pitchDeg_);

    const float eyeX = targetX_ + distance_ * std::cos(pitchRad) * std::cos(yawRad);
    const float eyeY = targetY_ + distance_ * std::sin(pitchRad);
    const float eyeZ = targetZ_ + distance_ * std::cos(pitchRad) * std::sin(yawRad);

    gluLookAt(eyeX, eyeY, eyeZ, targetX_, targetY_, targetZ_, 0.0, 1.0, 0.0);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < mesh.indices.size(); i += 3)
    {
        const terrain::TerrainVertex& a = mesh.vertices[mesh.indices[i + 0]];
        const terrain::TerrainVertex& b = mesh.vertices[mesh.indices[i + 1]];
        const terrain::TerrainVertex& c = mesh.vertices[mesh.indices[i + 2]];

        const float hA = (a.y - mesh.minHeight) / std::max(0.001f, mesh.maxHeight - mesh.minHeight);
        const float hB = (b.y - mesh.minHeight) / std::max(0.001f, mesh.maxHeight - mesh.minHeight);
        const float hC = (c.y - mesh.minHeight) / std::max(0.001f, mesh.maxHeight - mesh.minHeight);

        const float slopeA = std::max(0.0f, 1.0f - a.ny);
        const float slopeB = std::max(0.0f, 1.0f - b.ny);
        const float slopeC = std::max(0.0f, 1.0f - c.ny);

        const float mountainA = std::max(a.mountainWeight, slopeA * 1.15f);
        const float mountainB = std::max(b.mountainWeight, slopeB * 1.15f);
        const float mountainC = std::max(c.mountainWeight, slopeC * 1.15f);

        const float plainsA = 1.0f - std::min(1.0f, mountainA);
        const float plainsB = 1.0f - std::min(1.0f, mountainB);
        const float plainsC = 1.0f - std::min(1.0f, mountainC);

        const float rockBoostA = std::min(1.0f, slopeA * 3.8f + hA * 0.25f);
        const float rockBoostB = std::min(1.0f, slopeB * 3.8f + hB * 0.25f);
        const float rockBoostC = std::min(1.0f, slopeC * 3.8f + hC * 0.25f);

        const float rA = plainsA * (0.20f + 0.22f * hA) + mountainA * (0.33f + 0.25f * hA) +
                         rockBoostA * 0.12f;
        const float gA = plainsA * (0.33f + 0.36f * hA) + mountainA * (0.29f + 0.18f * hA) -
                         rockBoostA * 0.08f;
        const float bA = plainsA * (0.15f + 0.14f * hA) + mountainA * (0.25f + 0.20f * hA) +
                         rockBoostA * 0.05f;
        glColor3f(rA, gA, bA);
        glNormal3f(a.nx, a.ny, a.nz);
        glVertex3f(a.x, a.y, a.z);

        const float rB = plainsB * (0.20f + 0.22f * hB) + mountainB * (0.33f + 0.25f * hB) +
                         rockBoostB * 0.12f;
        const float gB = plainsB * (0.33f + 0.36f * hB) + mountainB * (0.29f + 0.18f * hB) -
                         rockBoostB * 0.08f;
        const float bB = plainsB * (0.15f + 0.14f * hB) + mountainB * (0.25f + 0.20f * hB) +
                         rockBoostB * 0.05f;
        glColor3f(rB, gB, bB);
        glNormal3f(b.nx, b.ny, b.nz);
        glVertex3f(b.x, b.y, b.z);

        const float rC = plainsC * (0.20f + 0.22f * hC) + mountainC * (0.33f + 0.25f * hC) +
                         rockBoostC * 0.12f;
        const float gC = plainsC * (0.33f + 0.36f * hC) + mountainC * (0.29f + 0.18f * hC) -
                         rockBoostC * 0.08f;
        const float bC = plainsC * (0.15f + 0.14f * hC) + mountainC * (0.25f + 0.20f * hC) +
                         rockBoostC * 0.05f;
        glColor3f(rC, gC, bC);
        glNormal3f(c.nx, c.ny, c.nz);
        glVertex3f(c.x, c.y, c.z);
    }
    glEnd();
}

void runDemo()
{
    terrain::TerrainSettings settings;
    settings.width = 257;
    settings.depth = 257;
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

    terrain::TerrainGenerator generator(settings);
    terrain::TerrainMesh mesh = generator.generateMesh();

    Renderer renderer(1280, 800);
    if (!renderer.init())
    {
        std::cerr << "Renderer init failed\n";
        return;
    }

    const float centerX = (static_cast<float>(settings.width - 1) * settings.horizontalScale) * 0.5f;
    const float centerZ = (static_cast<float>(settings.depth - 1) * settings.horizontalScale) * 0.5f;
    renderer.setTarget(centerX, (mesh.minHeight + mesh.maxHeight) * 0.30f, centerZ);
    renderer.zoom(60.0f);

    std::cout << "Controls:\n";
    std::cout << "  Left mouse drag: orbit\n";
    std::cout << "  Right mouse drag: pan\n";
    std::cout << "  Wheel: zoom\n";
    std::cout << "  WASD: move\n";
    std::cout << "  Q/E: move down/up\n";
    std::cout << "  R: regenerate terrain\n";
    std::cout << "  P: save screenshot\n";
    std::cout << "  ESC: quit\n";

    SDL_Event event;
    bool orbiting = false;
    bool panning = false;
    int prevMouseX = 0;
    int prevMouseY = 0;

    while (!renderer.shouldClose())
    {
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
                    std::cout << "Regenerated terrain with seed " << settings.seed << '\n';
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
        const float moveSpeed = 2.8f;
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
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

} // namespace renderer
