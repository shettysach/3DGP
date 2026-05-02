#include "graph/view.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <unordered_set>

#include "graph/compile.h"
#include "graph/serialize.h"
#include "graph/types.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "imnodes.h"

namespace graph {

// op id * 1000 + slot (slot 0..99)
// ip id * 1000 + 200 + slot (slot 0..99)

static int outPinId(NodeId nid, uint8_t slot) {
    return static_cast<int>(nid) * 1000 + static_cast<int>(slot);
}

static int inPinId(NodeId nid, uint8_t slot) {
    return static_cast<int>(nid) * 1000 + 200 + static_cast<int>(slot);
}

static void decodePin(int pinId, NodeId& nid, uint8_t& slot, bool& isOutput) {
    nid = static_cast<NodeId>(pinId / 1000);
    int r = pinId % 1000;
    if (r >= 200) {
        isOutput = false;
        slot = static_cast<uint8_t>(r - 200);
    } else {
        isOutput = true;
        slot = static_cast<uint8_t>(r);
    }
}

// App state

static SDL_Window* gWindow = nullptr;
static SDL_GLContext gGlCtx = nullptr;
static bool gRunning = true;

static EditorGraph gGraph;
static std::string gFilePath = "graphs/current.json";

static NodeId gNextNodeId = 0;
static LinkId gNextLinkId = 0;

static NodeId nextNodeId() {
    return gNextNodeId++;
}

static LinkId nextLinkId() {
    return gNextLinkId++;
}

// SDL + imgui init and end

static void initSDL() {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE
    );
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    gWindow = SDL_CreateWindow(
        "Graph Editor",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280,
        800,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    gGlCtx = SDL_GL_CreateContext(gWindow);
    SDL_GL_SetSwapInterval(1);
}

static void initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImNodes::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.0f;
    ImGui::GetStyle().ScaleAllSizes(1.0f);

    ImNodesStyle& ns = ImNodes::GetStyle();
    ns.Flags |= ImNodesStyleFlags_GridLines;
    ns.PinCircleRadius *= 1.0f;
    ns.LinkHoverDistance *= 1.0f;

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

// Saving graph json

static void saveToFile() {
    mkdir("graphs", 0755); // Ensure output dir
    std::ofstream out(gFilePath);
    if (!out)
        return;
    out << toJson(gGraph);
}

static void syncIdsFromGraph(const EditorGraph& graph) {
    for (const auto& n : graph.nodes)
        if (n.id >= gNextNodeId)
            gNextNodeId = n.id + 1;

    for (const auto& l : graph.links)
        if (l.id >= gNextLinkId)
            gNextLinkId = l.id + 1;
}

static void loadFromFile() {
    std::ifstream in(gFilePath);
    if (!in)
        return;
    std::string text {
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>()
    };
    try {
        gGraph = fromJson(text);
        syncIdsFromGraph(gGraph);
    } catch (const std::exception&) {
        // keep default graph
    }
}

// UI

static void handleDeleteSelected() {
    int nodeCount = ImNodes::NumSelectedNodes();
    int linkCount = ImNodes::NumSelectedLinks();

    std::unordered_set<NodeId> rmNodes;
    std::unordered_set<int> rmLinks;

    if (nodeCount > 0) {
        std::vector<int> ids(nodeCount);
        ImNodes::GetSelectedNodes(ids.data());
        rmNodes.insert(ids.begin(), ids.end());

        gGraph.nodes.erase(
            std::remove_if(
                gGraph.nodes.begin(),
                gGraph.nodes.end(),
                [&](const EditorNode& n) { return rmNodes.count(n.id); }
            ),
            gGraph.nodes.end()
        );
        ImNodes::ClearNodeSelection();
    }

    if (linkCount > 0) {
        std::vector<int> ids(linkCount);
        ImNodes::GetSelectedLinks(ids.data());
        rmLinks.insert(ids.begin(), ids.end());
    }

    if (nodeCount > 0 || linkCount > 0) {
        gGraph.links.erase(
            std::remove_if(
                gGraph.links.begin(),
                gGraph.links.end(),
                [&](const EditorLink& l) {
                    return rmLinks.count(l.id) || rmNodes.count(l.from.nodeId)
                        || rmNodes.count(l.to.nodeId);
                }
            ),
            gGraph.links.end()
        );
        ImNodes::ClearLinkSelection();
    }
}

static void addNode(NodeKind kind) {
    NodeId id = nextNodeId();
    gGraph.nodes.push_back(
        {id, kind, 200.0f, 100.0f + id * 30.0f, defaultParams(kind)}
    );
}

static float drawToolbar() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, 0.0f));
    ImGui::Begin(
        "Toolbar",
        nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoBringToFrontOnFocus
    );

    if (ImGui::Button("Compile")) {
        graph::compile(gGraph);
        saveToFile();
    }

    ImGui::SameLine();
    if (ImGui::Button("Presets v")) {
        ImGui::OpenPopup("PresetsPopup");
    }
    if (ImGui::BeginPopup("PresetsPopup")) {
        if (ImGui::Selectable("preset 0")) {
            gGraph = defaultGraph();
            gNextNodeId = 0;
            gNextLinkId = 0;
            syncIdsFromGraph(gGraph);
            saveToFile();
        }
        if (ImGui::Selectable("preset 1")) {
            gGraph = preset1Graph();
            gNextNodeId = 0;
            gNextLinkId = 0;
            syncIdsFromGraph(gGraph);
            saveToFile();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Noise v")) {
        ImGui::OpenPopup("NoisePopup");
    }
    if (ImGui::BeginPopup("NoisePopup")) {
        if (ImGui::Selectable("FBm"))
            addNode(NodeKind::Fbm);
        if (ImGui::Selectable("Ridged"))
            addNode(NodeKind::RidgedFbm);
        if (ImGui::Selectable("Fractal Perlin"))
            addNode(NodeKind::FractalPerlin);
        if (ImGui::Selectable("Perlin"))
            addNode(NodeKind::Perlin);
        if (ImGui::Selectable("Simplex"))
            addNode(NodeKind::Simplex);
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Vec2 v")) {
        ImGui::OpenPopup("Vec2Popup");
    }
    if (ImGui::BeginPopup("Vec2Popup")) {
        if (ImGui::Selectable("Position"))
            addNode(NodeKind::Position);
        if (ImGui::Selectable("CreateVec2"))
            addNode(NodeKind::CreateVec2);
        if (ImGui::Selectable("Add2"))
            addNode(NodeKind::Add2);
        if (ImGui::Selectable("Scale2"))
            addNode(NodeKind::Scale2);
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Float v")) {
        ImGui::OpenPopup("FloatPopup");
    }
    if (ImGui::BeginPopup("FloatPopup")) {
        if (ImGui::Selectable("Terrace"))
            addNode(NodeKind::Terrace);
        if (ImGui::Selectable("Smoothstep"))
            addNode(NodeKind::Smoothstep);
        if (ImGui::Selectable("Lerp"))
            addNode(NodeKind::Lerp);
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Terrain v")) {
        ImGui::OpenPopup("TerrainPopup");
    }
    if (ImGui::BeginPopup("TerrainPopup")) {
        if (ImGui::Selectable("Mountain"))
            addNode(NodeKind::Mountain);
        if (ImGui::Selectable("Valley"))
            addNode(NodeKind::Valley);
        if (ImGui::Selectable("Plains"))
            addNode(NodeKind::Plains);
        if (ImGui::Selectable("Plateau"))
            addNode(NodeKind::Plateau);
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Blend")) {
        addNode(NodeKind::Blend);
    }

    if (ImNodes::NumSelectedNodes() > 0 || ImNodes::NumSelectedLinks() > 0) {
        ImGui::SameLine();
        if (ImGui::Button("Delete"))
            handleDeleteSelected();
    }

    float height = ImGui::GetWindowSize().y;
    ImGui::End();
    return height;
}

static void drawNodeEditor(float toolbarHeight) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float top = vp->WorkPos.y + toolbarHeight;
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, top));
    ImGui::SetNextWindowSize(
        ImVec2(vp->WorkSize.x, vp->WorkPos.y + vp->WorkSize.y - top)
    );
    ImGui::Begin(
        "Graph",
        nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
    );

    ImNodes::BeginNodeEditor();

    // Set node positions
    for (const auto& node : gGraph.nodes)
        ImNodes::SetNodeGridSpacePos(node.id, ImVec2(node.posX, node.posY));

    // Draw nodes

    for (const auto& node : gGraph.nodes) {
        const NodeDef& def = nodeDefinition(node.kind);

        ImNodes::BeginNode(node.id);
        ImNodes::BeginNodeTitleBar();
        ImGui::Text("%s", def.name);
        ImNodes::EndNodeTitleBar();

        for (size_t i = 0; i < def.inputs.size(); ++i) {
            ImNodes::BeginInputAttribute(
                inPinId(node.id, static_cast<uint8_t>(i))
            );
            ImGui::Text("%s", def.inputs[i].label);
            ImNodes::EndInputAttribute();
        }

        for (size_t i = 0; i < def.outputs.size(); ++i) {
            ImNodes::BeginOutputAttribute(
                outPinId(node.id, static_cast<uint8_t>(i))
            );
            ImGui::Text("%s", def.outputs[i].label);
            ImNodes::EndOutputAttribute();
        }

        ImNodes::EndNode();
    }

    // Draw links

    for (const auto& link : gGraph.links) {
        ImNodes::Link(
            link.id,
            outPinId(link.from.nodeId, link.from.slot),
            inPinId(link.to.nodeId, link.to.slot)
        );
    }

    ImNodes::EndNodeEditor();

    // Handle new links
    int startPin, endPin;
    if (ImNodes::IsLinkCreated(&startPin, &endPin)) {
        NodeId fromNid, toNid;
        uint8_t fromSlot, toSlot;
        bool startOut, endOut;
        decodePin(startPin, fromNid, fromSlot, startOut);
        decodePin(endPin, toNid, toSlot, endOut);

        // Ensure direction output → input
        if (!startOut && endOut) {
            std::swap(fromNid, toNid);
            std::swap(fromSlot, toSlot);
        }

        if (startOut && !endOut) {
            const EditorNode* fromNode = nullptr;
            const EditorNode* toNode = nullptr;

            for (const auto& n : gGraph.nodes) {
                if (n.id == fromNid)
                    fromNode = &n;
                if (n.id == toNid)
                    toNode = &n;
            }

            if (fromNode && toNode) {
                const NodeDef& srcDef = nodeDefinition(fromNode->kind);
                const NodeDef& dstDef = nodeDefinition(toNode->kind);

                bool typeCheck =
                    srcDef.outputs[fromSlot].type == dstDef.inputs[toSlot].type;
                bool preconnected = std::any_of(
                    gGraph.links.begin(),
                    gGraph.links.end(),
                    [&](const EditorLink& l) {
                        return l.to.nodeId == toNid && l.to.slot == toSlot;
                    }
                );

                if (typeCheck && !preconnected) {
                    gGraph.links.push_back(
                        {nextLinkId(), {fromNid, fromSlot}, {toNid, toSlot}}
                    );
                }
            }
        }
    }

    // Update node positions
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
    if (selCount == 1) {
        std::vector<int> ids(1);
        ImNodes::GetSelectedNodes(ids.data());
        NodeId selId = static_cast<NodeId>(ids[0]);

        EditorNode* node = nullptr;
        for (auto& n : gGraph.nodes) {
            if (n.id == selId) {
                node = &n;
                break;
            }
        }
        if (!node) {
            ImGui::End();
            return;
        }

        ImGui::Separator();
        ImGui::Text("Kind: %s", kindToString(node->kind));

        if (node->kind == NodeKind::Fbm || node->kind == NodeKind::RidgedFbm
            || node->kind == NodeKind::FractalPerlin
            || node->kind == NodeKind::Perlin
            || node->kind == NodeKind::Simplex) {
            auto& np = std::get<NoiseParams>(node->params);
            ImGui::DragFloat(
                "Frequency",
                &np.frequency,
                0.0001f,
                0.0001f,
                0.1f,
                "%.4f"
            );
            if (node->kind == NodeKind::Fbm || node->kind == NodeKind::RidgedFbm
                || node->kind == NodeKind::FractalPerlin) {
                ImGui::SliderInt("Octaves", &np.octaves, 1, 10);
                ImGui::DragFloat(
                    "Lacunarity",
                    &np.lacunarity,
                    0.01f,
                    1.0f,
                    5.0f
                );
                ImGui::DragFloat("Gain", &np.gain, 0.01f, 0.1f, 1.0f);
            }
            ImGui::DragFloat("X Offset", &np.xOffset, 1.0f);
            ImGui::DragFloat("Z Offset", &np.zOffset, 1.0f);
            if (node->kind == NodeKind::RidgedFbm) {
                ImGui::DragFloat("Sharpness", &np.sharpness, 0.1f, 0.5f, 5.0f);
            }
        } else if (node->kind == NodeKind::Mountain) {
            auto& mp = std::get<MountainParams>(node->params);
            ImGui::DragFloat("Height Scale", &mp.heightScale, 0.01f, 0.1f, 3.0f, "%.2f");
            ImGui::DragFloat("Coverage", &mp.coverage, 0.01f, 0.1f, 1.0f, "%.2f");
            ImGui::DragFloat("Sharpness", &mp.sharpness, 0.01f, 0.5f, 3.0f, "%.2f");
        } else if (node->kind == NodeKind::Valley) {
            auto& vp = std::get<ValleyParams>(node->params);
            ImGui::DragFloat("Depth Scale", &vp.depthScale, 0.01f, 0.1f, 2.0f, "%.2f");
            ImGui::DragFloat("Coverage", &vp.coverage, 0.01f, 0.1f, 1.0f, "%.2f");
        } else if (node->kind == NodeKind::Plains) {
            auto& pp = std::get<PlainsParams>(node->params);
            ImGui::DragFloat("Height Scale", &pp.heightScale, 0.01f, 0.1f, 3.0f, "%.2f");
            ImGui::DragFloat("Relief", &pp.relief, 0.01f, 0.0f, 1.0f, "%.2f");
        } else if (node->kind == NodeKind::Plateau) {
            auto& tp = std::get<PlateauParams>(node->params);
            ImGui::DragFloat("Height Scale", &tp.heightScale, 0.01f, 0.1f, 2.0f, "%.2f");
            ImGui::DragFloat("Coverage", &tp.coverage, 0.01f, 0.1f, 1.0f, "%.2f");
            ImGui::DragFloat("Cliffness", &tp.cliffness, 0.01f, 0.1f, 3.0f, "%.2f");
        } else if (node->kind == NodeKind::Terrace) {
            auto& tp = std::get<TerraceParams>(node->params);
            ImGui::DragFloat("Steps", &tp.steps, 0.1f, 1.0f, 100.0f, "%.1f");
        } else if (node->kind == NodeKind::Smoothstep) {
            auto& sp = std::get<SmoothstepParams>(node->params);
            ImGui::DragFloat("A", &sp.a, 0.01f, -10.0f, 10.0f, "%.2f");
            ImGui::DragFloat("B", &sp.b, 0.01f, -10.0f, 10.0f, "%.2f");
        } else if (node->kind == NodeKind::Lerp) {
            auto& lp = std::get<LerpParams>(node->params);
            ImGui::DragFloat("A", &lp.a, 0.01f, -10.0f, 10.0f, "%.2f");
            ImGui::DragFloat("B", &lp.b, 0.01f, -10.0f, 10.0f, "%.2f");
            ImGui::DragFloat("T", &lp.t, 0.01f, 0.0f, 1.0f, "%.2f");
        } else if (node->kind == NodeKind::Blend) {
            ImGui::Text("(no tunable params)");
        } else if (node->kind == NodeKind::CreateVec2) {
            auto& cp = std::get<CreateVec2Params>(node->params);
            ImGui::DragFloat("X", &cp.x, 0.0f);
            ImGui::DragFloat("Y", &cp.y, 0.0f);
        } else if (node->kind == NodeKind::Scale2) {
            auto& sp = std::get<Scale2Params>(node->params);
            ImGui::DragFloat("Scale", &sp.scale, 0.1f, 0.1f, 100.0f, "%.1f");
        }
    } else {
        ImGui::Text("%d nodes selected", selCount);
    }

    ImGui::End();
}

// Event loop

static void processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        if (event.type == SDL_QUIT)
            gRunning = false;
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
            gRunning = false;
    }
}

// Main

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

        float h = drawToolbar();
        drawNodeEditor(h);
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
