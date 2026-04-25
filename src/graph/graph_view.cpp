#include "graph/graph_view.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "imnodes.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <iostream>

namespace graph {

static SDL_Window* gWindow = nullptr;
static SDL_GLContext gGlContext = nullptr;
static bool gRunning = true;

static bool initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    gWindow = SDL_CreateWindow(
        "Graph View",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280,
        800,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!gWindow) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        return false;
    }

    gGlContext = SDL_GL_CreateContext(gWindow);
    if (!gGlContext) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << '\n';
        return false;
    }

    SDL_GL_SetSwapInterval(1);
    return true;
}

static bool initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImNodes::CreateContext();

    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForOpenGL(gWindow, gGlContext)) {
        std::cerr << "ImGui_ImplSDL2_InitForOpenGL failed\n";
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        std::cerr << "ImGui_ImplOpenGL3_Init failed\n";
        return false;
    }

    return true;
}

static void shutdownImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
}

static void shutdownSDL() {
    if (gGlContext) {
        SDL_GL_DeleteContext(gGlContext);
        gGlContext = nullptr;
    }
    if (gWindow) {
        SDL_DestroyWindow(gWindow);
        gWindow = nullptr;
    }
    SDL_Quit();
}

static void processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        if (event.type == SDL_QUIT) {
            gRunning = false;
        }
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            gRunning = false;
        }
    }
}

static void drawGraphEditor() {
    ImGui::Begin("Graph Editor");

    ImNodes::BeginNodeEditor();

    ImNodes::BeginNode(1);
    ImNodes::BeginNodeTitleBar();
    ImGui::Text("World X");
    ImNodes::EndNodeTitleBar();
    ImNodes::BeginOutputAttribute(2);
    ImGui::Text("x");
    ImNodes::EndOutputAttribute();
    ImNodes::EndNode();

    ImNodes::BeginNode(3);
    ImNodes::BeginNodeTitleBar();
    ImGui::Text("Noise");
    ImNodes::EndNodeTitleBar();
    ImNodes::BeginInputAttribute(4);
    ImGui::Text("x");
    ImNodes::EndInputAttribute();
    ImNodes::BeginInputAttribute(5);
    ImGui::Text("z");
    ImNodes::EndInputAttribute();
    ImNodes::BeginOutputAttribute(6);
    ImGui::Text("field");
    ImNodes::EndOutputAttribute();
    ImNodes::EndNode();

    ImNodes::BeginNode(7);
    ImNodes::BeginNodeTitleBar();
    ImGui::Text("Graph Output");
    ImNodes::EndNodeTitleBar();
    ImNodes::BeginInputAttribute(8);
    ImGui::Text("value");
    ImNodes::EndInputAttribute();
    ImNodes::EndNode();

    ImNodes::EndNodeEditor();

    ImGui::End();
}

void run() {
    if (!initSDL()) {
        return;
    }
    if (!initImGui()) {
        shutdownSDL();
        return;
    }

    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);

    while (gRunning) {
        processEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        drawGraphEditor();

        ImGui::Render();
        glViewport(0, 0,
            static_cast<int>(ImGui::GetIO().DisplaySize.x),
            static_cast<int>(ImGui::GetIO().DisplaySize.y));
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(gWindow);
    }

    shutdownImGui();
    shutdownSDL();
}

} // namespace graph
