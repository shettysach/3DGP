#include <sys/stat.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "graph/compile.h"
#include "graph/serialize.h"
#include "graph/types.h"
#include "renderer/core.h"
#include "terrain/biomes.h"

namespace renderer {

void runDemo() {
    using namespace std::chrono;

    terrain::TerrainSettings settings;
    settings.width = 720;
    settings.depth = 720;
    settings.horizontalScale = 1.0f;
    settings.verticalScale = 96.0f;
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
        std::array<size_t, static_cast<size_t>(terrain::BiomeId::Count)>
            counts {};
        for (const terrain::TerrainVertex& v : mesh.vertices) {
            ++counts[static_cast<size_t>(v.primaryBiome)];
        }

        std::cout << "Surface coverage:";
        const float invTotal = mesh.vertices.empty()
            ? 0.0f
            : 100.0f / static_cast<float>(mesh.vertices.size());
        for (size_t idx = 0; idx < counts.size(); ++idx) {
            if (counts[idx] == 0) {
                continue;
            }
            std::cout << ' '
                      << terrain::biomeName(static_cast<terrain::BiomeId>(idx))
                      << ' ' << counts[idx] * invTotal << '%';
        }
        std::cout << '\n';
    };

    Renderer renderer(1280, 800);
    if (!renderer.init()) {
        std::cerr << "Renderer init failed\n";
        return;
    }

    terrain::TerrainGenerator generator(settings);

    // Watch graphs/current.json for changes
    std::string graphPath = "graphs/current.json";
    time_t lastMtime = 0;
    auto loadGraph = [&]() -> bool {
        struct stat st;
        if (stat(graphPath.c_str(), &st) != 0)
            return false;
        if (st.st_mtime == lastMtime)
            return false;
        lastMtime = st.st_mtime;

        std::ifstream in(graphPath);
        if (!in)
            return false;
        std::stringstream ss;
        ss << in.rdbuf();

        try {
            graph::EditorGraph eg = graph::fromJson(ss.str());
            auto cg =
                std::make_shared<graph::CompiledGraph>(graph::compile(eg));
            generator.setBaseGraph(cg);
            std::cout << "Reloaded graph: " << cg->nodes.size() << " nodes\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Graph error: " << e.what() << '\n';
            return false;
        }
    };

    bool graphLoaded = loadGraph();
    if (!graphLoaded) {
        std::cout << "Waiting for " << graphPath
                  << " — run './build/terrain_demo graph' to create one\n";
    }
    std::cout << "Generating initial terrain at " << settings.width << 'x'
              << settings.depth << "...\n";
    terrain::TerrainMesh mesh = generator.generateMesh();

    const float centerX =
        (static_cast<float>(settings.width - 1) * settings.horizontalScale)
        * 0.5f;
    const float centerZ =
        (static_cast<float>(settings.depth - 1) * settings.horizontalScale)
        * 0.5f;
    renderer
        .setTarget(centerX, (mesh.minHeight + mesh.maxHeight) * 0.35f, centerZ);
    renderer.setDistance(static_cast<float>(settings.width) * 1.1f);

    std::cout << "Controls:\n";
    std::cout << "  Mouse Wheel: Zoom In / Zoom Out\n";
    std::cout << "  Left Mouse Drag: Orbit Camera\n";
    std::cout << "  Right Mouse Drag: Pan Camera\n";
    std::cout << "  WASD: Move Camera\n";
    std::cout << "  Q/E: Move Up/Down\n";
    std::cout << "  SPACE: Reset View (Frame Entire Map)\n";
    std::cout << "  R: Regenerate Terrain\n";
    std::cout << "  1-4: Switch Visualization Modes\n";
    std::cout << "  P: Export Terrain Stages to PNG\n";
    std::cout << "  ESC: Quit\n";
    printBiomeStats(mesh);

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
            0.1f
        );
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
                case SDLK_p:
                    settings.exportImages = true;
                    generator.setSettings(settings);
                    mesh = generator.generateMesh();
                    settings.exportImages = false;
                    generator.setSettings(settings);
                    std::cout << "Exported terrain stages to PPM files.\n";
                    break;
                case SDLK_1:
                    renderer.setVisualizationMode(0);
                    std::cout << "Visualization: Normal (Textured)\n";
                    break;
                case SDLK_2:
                    renderer.setVisualizationMode(1);
                    std::cout << "Visualization: Voronoi Territories\n";
                    break;
                case SDLK_3:
                    renderer.setVisualizationMode(2);
                    std::cout << "Visualization: WFC Result (Regional Biomes)\n";
                    break;
                case SDLK_4:
                    renderer.setVisualizationMode(3);
                    std::cout << "Visualization: Blended Biomes\n";
                    break;
                case SDLK_SPACE: {
                    const float cx = (static_cast<float>(settings.width - 1) * settings.horizontalScale) * 0.5f;
                    const float cz = (static_cast<float>(settings.depth - 1) * settings.horizontalScale) * 0.5f;
                    renderer.setTarget(cx, (mesh.minHeight + mesh.maxHeight) * 0.4f, cz);
                    // Calculate distance to frame the map
                    const float mapSize = static_cast<float>(std::max(settings.width, settings.depth));
                    renderer.setDistance(mapSize * 1.1f);
                    std::cout << "Camera reset to frame entire terrain.\n";
                    break;
                }
                default:
                        break;
                }
            }

            if (event.type == SDL_MOUSEBUTTONDOWN
                && event.button.button == SDL_BUTTON_LEFT) {
                orbiting = true;
                prevMouseX = event.button.x;
                prevMouseY = event.button.y;
            }
            if (event.type == SDL_MOUSEBUTTONUP
                && event.button.button == SDL_BUTTON_LEFT) {
                orbiting = false;
            }

            if (event.type == SDL_MOUSEBUTTONDOWN
                && event.button.button == SDL_BUTTON_RIGHT) {
                panning = true;
                prevMouseX = event.button.x;
                prevMouseY = event.button.y;
            }
            if (event.type == SDL_MOUSEBUTTONUP
                && event.button.button == SDL_BUTTON_RIGHT) {
                panning = false;
            }

            if (event.type == SDL_MOUSEMOTION) {
                const int dx = event.motion.x - prevMouseX;
                const int dy = event.motion.y - prevMouseY;

                if (orbiting) {
                    renderer.orbit(
                        static_cast<float>(dx) * 0.28f,
                        static_cast<float>(-dy) * 0.28f
                    );
                }
                if (panning) {
                    const float panScale =
                        std::max(0.3f, 0.005f * mesh.horizontalScale * 300.0f);
                    renderer.pan(
                        static_cast<float>(-dx) * panScale,
                        static_cast<float>(dy) * panScale
                    );
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

        // Reload terrain if graph file changed
        if (loadGraph()) {
            mesh = generator.generateMesh();
            renderer.invalidateMeshCache();
            printBiomeStats(mesh);
        }

        renderer.render(mesh);
        renderer.swapBuffers();
    }
}

void exportImages() {
    terrain::TerrainSettings settings;
    settings.width = 1024;
    settings.depth = 1024;
    settings.horizontalScale = 1.0f;
    settings.verticalScale = 96.0f;
    settings.seed = 2026u;
    settings.exportImages = true;

    terrain::TerrainGenerator generator(settings);

    // Try to load current graph if it exists
    std::string graphPath = "graphs/current.json";
    std::ifstream in(graphPath);
    if (in) {
        std::stringstream ss;
        ss << in.rdbuf();
        try {
            graph::EditorGraph eg = graph::fromJson(ss.str());
            auto cg = std::make_shared<graph::CompiledGraph>(graph::compile(eg));
            generator.setBaseGraph(cg);
            std::cout << "Loaded graph for export.\n";
        } catch (...) {
            std::cout << "Failed to load graph, using default.\n";
        }
    }

    std::cout << "Generating terrain and exporting stages...\n";
    generator.generateMesh();
    std::cout << "Export complete.\n";
}

} // namespace renderer
