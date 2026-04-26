#include "graph/graph_view.h"
#include "graph/graph_compile.h"
#include "graph/graph_serialize.h"
#include "graph/types.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "imnodes.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

namespace graph {

// ---------- Pin ID encoding ----------
// Output: nodeId * 1000 + slot         (slot range 0..99)
// Input:  nodeId * 1000 + 200 + slot   (slot range 0..99)

static int outPinId(NodeId nid, uint8_t slot) {
    return static_cast<int>(nid) * 1000 + static_cast<int>(slot);
}

static int inPinId(NodeId nid, uint8_t slot) {
    return static_cast<int>(nid) * 1000 + 200 + static_cast<int>(slot);
}

static bool decodePin(int pinId, NodeId& nid, uint8_t& slot, bool& isOutput) {
    nid = static_cast<NodeId>(pinId / 1000);
    int r = pinId % 1000;
    if (r >= 200) {
        isOutput = false;
        slot = static_cast<uint8_t>(r - 200);
    } else {
        isOutput = true;
        slot = static_cast<uint8_t>(r);
    }
    return true;
}

// ---------- App state ----------

static SDL_Window*   gWindow   = nullptr;
static SDL_GLContext gGlCtx    = nullptr;
static bool          gRunning  = true;

static EditorGraph gGraph;
static std::string gErrorMsg;
static std::string gFilePath = "graphs/current.json";

// ---------- Id generators ----------

static NodeId nextNodeId() {
    NodeId maxId = 0;
    for (const auto& n : gGraph.nodes) {
        if (n.id > maxId) maxId = n.id;
    }
    return maxId + 1;
}

static LinkId nextLinkId() {
    LinkId maxId = 0;
    for (const auto& l : gGraph.links) {
        if (l.id > maxId) maxId = l.id;
    }
    return maxId + 1;
}

// ---------- SDL + ImGui init / shutdown ----------

static void initSDL() {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    gWindow = SDL_CreateWindow(
        "Graph Editor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 800,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    gGlCtx = SDL_GL_CreateContext(gWindow);
    SDL_GL_SetSwapInterval(1);
}

static void initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImNodes::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.5f;
    ImGui::GetStyle().ScaleAllSizes(1.5f);

    ImNodesStyle& ns = ImNodes::GetStyle();
    ns.Flags |= ImNodesStyleFlags_GridLines;

    ImGui_ImplSDL2_InitForOpenGL(gWindow, gGlCtx);
    ImGui_ImplOpenGL3_Init("#version 330");
}

static void shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gGlCtx);
    SDL_DestroyWindow(gWindow);
    SDL_Quit();
}

// ---------- IO ----------

static void ensureGraphDir() {
    mkdir("graphs", 0755);
}

static void saveToFile() {
    ensureGraphDir();
    std::ofstream out(gFilePath);
    if (!out) {
        gErrorMsg = "Failed to open " + gFilePath + " for writing";
        return;
    }
    out << toJson(gGraph);
    gErrorMsg.clear();
}

static void loadFromFile() {
    std::ifstream in(gFilePath);
    if (!in) return;
    std::string text((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    try {
        gGraph = fromJson(text);
    } catch (const std::exception&) {
        // keep default graph
    }
}

// ---------- UI ----------

static void drawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                saveToFile();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

static void drawToolbar() {
    ImGui::Begin("Toolbar");

    // Compile button
    if (ImGui::Button("Compile")) {
        // Validate: compile to check for errors
        try {
            graph::compile(gGraph);
            gErrorMsg.clear();
            // Write JSON so the terrain viewer picks it up
            saveToFile();
        } catch (const std::exception& e) {
            gErrorMsg = e.what();
        }
    }

    ImGui::SameLine();

    // Add node buttons
    if (ImGui::Button("+ FBm")) {
        NodeId id = nextNodeId();
        gGraph.nodes.push_back({id, NodeKind::Fbm, 200.0f, 100.0f + id * 30.0f,
                                "Fbm " + std::to_string(id), NoiseParams{}});
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Ridged")) {
        NodeId id = nextNodeId();
        gGraph.nodes.push_back({id, NodeKind::RidgedFbm, 200.0f, 100.0f + id * 30.0f,
                                "Ridged " + std::to_string(id), NoiseParams{}});
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Synthesis")) {
        NodeId id = nextNodeId();
        gGraph.nodes.push_back({id, NodeKind::TerrainSynthesis, 200.0f, 100.0f + id * 30.0f,
                                "Synthesis", TerrainSynthesisParams{}});
    }

    if (ImNodes::NumSelectedNodes() > 0) {
        ImGui::SameLine();
        if (ImGui::Button("Delete Selected")) {
            // Collect selected node ids
            std::vector<NodeId> toRemove;
            int count = ImNodes::NumSelectedNodes();
            if (count > 0) {
                std::vector<int> ids(static_cast<size_t>(count));
                ImNodes::GetSelectedNodes(ids.data());
                for (int id : ids) {
                    toRemove.push_back(static_cast<NodeId>(id));
                }
            }

            // Remove nodes
            gGraph.nodes.erase(
                std::remove_if(gGraph.nodes.begin(), gGraph.nodes.end(),
                               [&](const EditorNode& n) {
                                   return std::find(toRemove.begin(), toRemove.end(), n.id) != toRemove.end();
                               }),
                gGraph.nodes.end());

            // Remove links that reference deleted nodes
            gGraph.links.erase(
                std::remove_if(gGraph.links.begin(), gGraph.links.end(),
                               [&](const EditorLink& l) {
                                   return std::find(toRemove.begin(), toRemove.end(), l.from.nodeId) != toRemove.end() ||
                                          std::find(toRemove.begin(), toRemove.end(), l.to.nodeId) != toRemove.end();
                               }),
                gGraph.links.end());

            ImNodes::ClearNodeSelection();
        }
    }

    // Error display
    if (!gErrorMsg.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", gErrorMsg.c_str());
    }

    ImGui::End();
}

static void drawNodeEditor() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 200, vp->WorkPos.y + 80));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x - 200, vp->WorkSize.y - 80));
    ImGui::Begin("Graph", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImNodes::BeginNodeEditor();

    // Set node positions
    for (const auto& node : gGraph.nodes) {
        ImNodes::SetNodeGridSpacePos(node.id, ImVec2(node.posX, node.posY));
    }

    // Draw nodes
    for (const auto& node : gGraph.nodes) {
        const NodeDef& def = nodeDefinition(node.kind);

        ImNodes::BeginNode(node.id);
        ImNodes::BeginNodeTitleBar();
        ImGui::Text("%s", node.title.empty() ? def.name : node.title.c_str());
        ImNodes::EndNodeTitleBar();

        for (size_t i = 0; i < def.inputs.size(); ++i) {
            ImNodes::BeginInputAttribute(inPinId(node.id, static_cast<uint8_t>(i)));
            ImGui::Text("%s", def.inputs[i].label);
            ImNodes::EndInputAttribute();
        }

        for (size_t i = 0; i < def.outputs.size(); ++i) {
            ImNodes::BeginOutputAttribute(outPinId(node.id, static_cast<uint8_t>(i)));
            ImGui::Text("%s", def.outputs[i].label);
            ImNodes::EndOutputAttribute();
        }

        ImNodes::EndNode();
    }

    // Draw links
    for (const auto& link : gGraph.links) {
        ImNodes::Link(link.id,
                      outPinId(link.from.nodeId, link.from.slot),
                      inPinId(link.to.nodeId, link.to.slot));
    }

    ImNodes::EndNodeEditor();

    // ----- Handle new links -----
    {
        int startPin, endPin;
        if (ImNodes::IsLinkCreated(&startPin, &endPin)) {
            NodeId fromNid, toNid;
            uint8_t fromSlot, toSlot;
            bool startOut, endOut;
            decodePin(startPin, fromNid, fromSlot, startOut);
            decodePin(endPin,   toNid,   toSlot,   endOut);

            // Ensure direction: output → input
            if (!startOut && endOut) {
                std::swap(fromNid, toNid);
                std::swap(fromSlot, toSlot);
            }

            if (startOut && !endOut) {
                // Check this isn't a duplicate
                bool dup = false;
                for (const auto& l : gGraph.links) {
                    if (l.from.nodeId == fromNid && l.from.slot == fromSlot &&
                        l.to.nodeId == toNid && l.to.slot == toSlot) {
                        dup = true;
                        break;
                    }
                }
                // Check input pin isn't already connected
                bool inputTaken = false;
                for (const auto& l : gGraph.links) {
                    if (l.to.nodeId == toNid && l.to.slot == toSlot) {
                        inputTaken = true;
                        break;
                    }
                }
                if (!dup && !inputTaken) {
                    gGraph.links.push_back({nextLinkId(), {fromNid, fromSlot}, {toNid, toSlot}});
                    gErrorMsg.clear();
                }
            }
        }
    }

    // ----- Handle deleted links -----
    {
        int destroyedId;
        while (ImNodes::IsLinkDestroyed(&destroyedId)) {
            gGraph.links.erase(
                std::remove_if(gGraph.links.begin(), gGraph.links.end(),
                               [destroyedId](const EditorLink& l) { return l.id == destroyedId; }),
                gGraph.links.end());
        }
    }

    // ----- Update node positions -----
    for (auto& node : gGraph.nodes) {
        ImVec2 pos = ImNodes::GetNodeGridSpacePos(node.id);
        node.posX = pos.x;
        node.posY = pos.y;
    }

    ImGui::End();
}

static void drawInspector() {
    ImGui::Begin("Inspector");

    int selCount = ImNodes::NumSelectedNodes();
    if (selCount == 0) {
        ImGui::Text("Select a node to edit parameters");
    } else if (selCount == 1) {
        std::vector<int> ids(1);
        ImNodes::GetSelectedNodes(ids.data());
        NodeId selId = static_cast<NodeId>(ids[0]);

        EditorNode* node = nullptr;
        for (auto& n : gGraph.nodes) {
            if (n.id == selId) { node = &n; break; }
        }
        if (!node) { ImGui::End(); return; }

        char titleBuf[128];
        std::strncpy(titleBuf, node->title.c_str(), sizeof(titleBuf) - 1);
        titleBuf[sizeof(titleBuf) - 1] = '\0';
        if (ImGui::InputText("Name", titleBuf, sizeof(titleBuf))) {
            node->title = titleBuf;
        }

        ImGui::Separator();
        ImGui::Text("Kind: %s", kindToString(node->kind));

        if (node->kind == NodeKind::Fbm || node->kind == NodeKind::RidgedFbm) {
            auto& np = std::get<NoiseParams>(node->params);
            ImGui::DragFloat("Frequency", &np.frequency, 0.0001f, 0.0001f, 0.1f, "%.4f");
            ImGui::SliderInt("Octaves", &np.octaves, 1, 10);
            ImGui::DragFloat("Lacunarity", &np.lacunarity, 0.01f, 1.0f, 5.0f);
            ImGui::DragFloat("Gain", &np.gain, 0.01f, 0.1f, 1.0f);
            ImGui::DragFloat("X Offset", &np.xOffset, 1.0f);
            ImGui::DragFloat("Z Offset", &np.zOffset, 1.0f);
            ImGui::Checkbox("Remap to Unit", &np.remapToUnit);
            if (node->kind == NodeKind::RidgedFbm) {
                ImGui::DragFloat("Sharpness", &np.sharpness, 0.1f, 0.5f, 5.0f);
            }
        } else if (node->kind == NodeKind::TerrainSynthesis) {
            auto& tp = std::get<TerrainSynthesisParams>(node->params);
            ImGui::DragFloat("Vertical Scale", &tp.verticalScale, 1.0f, 1.0f, 500.0f);
            ImGui::Checkbox("Island Falloff", &tp.islandFalloff);
            ImGui::DragFloat("Falloff Radius", &tp.falloffRadius, 0.01f, 0.1f, 1.0f);
            ImGui::DragFloat("Falloff Power", &tp.falloffPower, 0.1f, 0.5f, 5.0f);
        }
    } else {
        ImGui::Text("%d nodes selected", selCount);
    }

    ImGui::End();
}

// ---------- Event loop ----------

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
        // Ctrl+S to save
        if (event.type == SDL_KEYDOWN &&
            event.key.keysym.sym == SDLK_s &&
            (event.key.keysym.mod & KMOD_CTRL)) {
            saveToFile();
        }
    }
}

// ---------- Main ----------

void run() {
    initSDL();

    // Load graph from file, or use default
    loadFromFile();
    if (gGraph.nodes.empty()) {
        gGraph = defaultGraph();
    }

    initImGui();

    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);

    while (gRunning) {
        processEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        drawMenuBar();
        drawToolbar();
        drawNodeEditor();
        drawInspector();

        ImGui::Render();

        int dw = static_cast<int>(ImGui::GetIO().DisplaySize.x);
        int dh = static_cast<int>(ImGui::GetIO().DisplaySize.y);
        glViewport(0, 0, dw, dh);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(gWindow);
    }

    saveToFile();
    shutdown();
}

} // namespace graph
