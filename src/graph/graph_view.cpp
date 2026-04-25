#include "graph/graph_view.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "imnodes.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

namespace graph {

static SDL_Window* gWindow = nullptr;
static SDL_GLContext gGlContext = nullptr;
static bool gRunning = true;

static void initSDL() {
    SDL_Init(SDL_INIT_VIDEO);

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

    gGlContext = SDL_GL_CreateContext(gWindow);
    SDL_GL_SetSwapInterval(1);
}

static void initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImNodes::CreateContext();

    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 3.0f;

    ImGui::GetStyle().ScaleAllSizes(3.0f);

    ImNodesStyle& ns = ImNodes::GetStyle();
    ns.Flags |= ImNodesStyleFlags_GridLines;
    ns.LinkThickness = 6.0f;
    ns.PinCircleRadius = 8.0f;
    ns.PinLineThickness = 4.0f;
    ns.NodeBorderThickness = 4.0f;
    ns.GridSpacing = 64.0f;

    ImGui_ImplSDL2_InitForOpenGL(gWindow, gGlContext);
    ImGui_ImplOpenGL3_Init("#version 330");
}

static void shutdownImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
}

static void shutdownSDL() {
    SDL_GL_DeleteContext(gGlContext);
    SDL_DestroyWindow(gWindow);
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

static void sampleNodes() {
    static bool initialized = false;
    if (!initialized) {
        ImNodes::SetNodeGridSpacePos(1, ImVec2(100.0f, 200.0f));
        ImNodes::SetNodeGridSpacePos(3, ImVec2(400.0f, 200.0f));
        ImNodes::SetNodeGridSpacePos(7, ImVec2(700.0f, 200.0f));
        initialized = true;
    }

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
}

static void drawGraphEditor() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("Graph Editor", nullptr, flags);
    ImNodes::BeginNodeEditor();

    sampleNodes();

    ImNodes::EndNodeEditor();
    ImGui::End();
}

void run() {
    initSDL();
    initImGui();

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
