#include "renderer/core.h"

#include "terrain/biomes.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <iostream>
#include <string>

namespace renderer {

void runDemo() {
    using namespace std::chrono;

    terrain::TerrainSettings settings;
    settings.width = 512;
    settings.depth = 512;
    settings.horizontalScale = 1.0f;
    settings.verticalScale = 96.0f;
    settings.islandFalloff = false;
    settings.seed = 2026u;
    settings.noise.frequency = 0.0052f;
    settings.noise.octaves = 6;
    settings.noise.lacunarity = 2.0f;
    settings.noise.gain = 0.5f;
    settings.noise.ridgeSharpness = 1.5f;
    settings.noise.warpFrequency = 0.0038f;
    settings.noise.warpAmplitude = 20.0f;
    settings.rivers.sourceDensity = 0.00005f;
    settings.rivers.sourceAccumulation = 135.0f;
    settings.rivers.mainAccumulation = 320.0f;
    settings.rivers.minSourceSeparation = 28;

    auto printBiomeStats = [](const terrain::TerrainMesh& mesh) {
        std::array<size_t, static_cast<size_t>(terrain::BiomeId::Count)> counts{};
        for (const terrain::TerrainVertex& v : mesh.vertices) {
            ++counts[static_cast<size_t>(v.primaryBiome)];
        }

        std::cout << "Surface coverage:";
        const float invTotal = mesh.vertices.empty() ? 0.0f : 100.0f / static_cast<float>(mesh.vertices.size());
        for (size_t idx = 0; idx < counts.size(); ++idx) {
            if (counts[idx] == 0) {
                continue;
            }
            std::cout << ' ' << terrain::biomeName(static_cast<terrain::BiomeId>(idx)) << ' '
                      << counts[idx] * invTotal << '%';
        }
        std::cout << '\n';
    };

    Renderer renderer(1280, 800);
    if (!renderer.init()) {
        std::cerr << "Renderer init failed\n";
        return;
    }

    terrain::TerrainGenerator generator(settings);
    std::cout << "Generating initial terrain at " << settings.width << 'x' << settings.depth << "...\n";
    terrain::TerrainMesh mesh = generator.generateMesh();

    auto setMode = [&renderer](Mode mode) {
        renderer.setMode(mode);
        std::cout << "Render mode: " << modeName(mode) << '\n';
    };

    Mode currentMode = renderer.mode();

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
    std::cout << "  V: cycle forward through modes\n";
    std::cout << "  P: save screenshot\n";
    std::cout << "  ESC: quit\n";
    printBiomeStats(mesh);
    setMode(currentMode);

    SDL_Event event;
    bool orbiting = false;
    bool panning = false;
    int prevMouseX = 0;
    int prevMouseY = 0;
    using Clock = std::chrono::steady_clock;
    auto lastFrameTime = Clock::now();

    while (!renderer.shouldClose()) {
        const auto frameTime = Clock::now();
        const float deltaSeconds = std::clamp(
            std::chrono::duration<float>(frameTime - lastFrameTime).count(),
            0.0f,
            0.1f);
        lastFrameTime = frameTime;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                return;
            }

            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    return;
                case SDLK_r:
                    settings.seed += 1u;
                    generator.setSettings(settings);
                    mesh = generator.generateMesh();
                    renderer.invalidateMeshCache();
                    std::cout << "Regenerated terrain with seed " << settings.seed << '\n';
                    printBiomeStats(mesh);
                    break;
                case SDLK_v:
                    if (event.key.keysym.mod & KMOD_SHIFT) {
                        if (static_cast<int>(currentMode) == 0) {
                            currentMode = Mode::Slope;
                        } else {
                            currentMode = static_cast<Mode>(static_cast<int>(currentMode) - 1);
                        }
                    } else {
                        currentMode = static_cast<Mode>(
                            (static_cast<int>(currentMode) + 1) % (static_cast<int>(Mode::Slope) + 1));
                    }
                    renderer.setMode(currentMode);
                    std::cout << "Render mode: " << modeName(currentMode) << '\n';
                    break;
                case SDLK_p: {
                    const std::time_t now = std::time(nullptr);
                    const std::string screenshotPath =
                        "screenshot_" + std::to_string(static_cast<long long>(now)) + ".bmp";
                    if (renderer.captureScreenshot(screenshotPath)) {
                        std::cout << "Saved screenshot to " << screenshotPath << '\n';
                    } else {
                        std::cout << "Failed to save screenshot\n";
                    }
                    break;
                }
                default:
                    break;
                }
            }

            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                orbiting = true;
                prevMouseX = event.button.x;
                prevMouseY = event.button.y;
            }
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                orbiting = false;
            }

            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
                panning = true;
                prevMouseX = event.button.x;
                prevMouseY = event.button.y;
            }
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) {
                panning = false;
            }

            if (event.type == SDL_MOUSEMOTION) {
                const int dx = event.motion.x - prevMouseX;
                const int dy = event.motion.y - prevMouseY;

                if (orbiting) {
                    renderer.orbit(static_cast<float>(dx) * 0.28f, static_cast<float>(-dy) * 0.28f);
                }
                if (panning) {
                    const float panScale = std::max(0.3f, 0.005f * mesh.horizontalScale * 300.0f);
                    renderer.pan(static_cast<float>(-dx) * panScale, static_cast<float>(dy) * panScale);
                }

                prevMouseX = event.motion.x;
                prevMouseY = event.motion.y;
            }

            if (event.type == SDL_MOUSEWHEEL) {
                renderer.zoom(static_cast<float>(-event.wheel.y) * 14.0f);
            }
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        const float moveSpeed = 170.0f * deltaSeconds;
        if (keys[SDL_SCANCODE_W]) {
            renderer.moveForward(-moveSpeed);
        }
        if (keys[SDL_SCANCODE_S]) {
            renderer.moveForward(moveSpeed);
        }
        if (keys[SDL_SCANCODE_A]) {
            renderer.moveRight(moveSpeed);
        }
        if (keys[SDL_SCANCODE_D]) {
            renderer.moveRight(-moveSpeed);
        }
        if (keys[SDL_SCANCODE_Q]) {
            renderer.pan(0.0f, -moveSpeed);
        }
        if (keys[SDL_SCANCODE_E]) {
            renderer.pan(0.0f, moveSpeed);
        }

        renderer.render(mesh);
        renderer.swapBuffers();
    }
}

} // namespace renderer
