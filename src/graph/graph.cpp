#include "graph/types.h"

#include <stdexcept>

namespace graph {

// === Node Definition Table ===

namespace {

const NodeDef kNodeDefs[] = {
    {
        NodeKind::Fbm,
        "FBm Noise",
        {/* inputs */},
        {{"field"}},
    },
    {
        NodeKind::RidgedFbm,
        "Ridged Noise",
        {/* inputs */},
        {{"field"}},
    },
    {
        NodeKind::Simplex,
        "Simplex Noise",
        {/* inputs */},
        {{"field"}},
    },
    {
        NodeKind::Perlin,
        "Perlin Noise",
        {/* inputs */},
        {{"field"}},
    },
    {
        NodeKind::Mountains,
        "Mountains",
        {
            {"continental"},
            {"ridges"},
            {"detail"},
            {"rangeMask"},
        },
        {{"out"}},
    },
    {
        NodeKind::Valleys,
        "Valleys",
        {
            {"continental"},
            {"basin"},
            {"detail"},
            {"rimMask"},
        },
        {{"out"}},
    },
    {
        NodeKind::Plains,
        "Plains",
        {
            {"continental"},
            {"plainsBase"},
            {"macroRelief"},
            {"hilliness"},
            {"detail"},
        },
        {{"out"}},
    },
    {
        NodeKind::Plateaus,
        "Plateaus",
        {
            {"continental"},
            {"plateauMask"},
            {"detail"},
        },
        {{"out"}},
    },
    {
        NodeKind::TerrainSynthesis,
        "Terrain Synthesis",
        {
            {"mountain"},
            {"valley"},
            {"plains"},
            {"plateau"},
            {"detail"},
        },
        {/* no outputs — final sink */},
    },
};

} // namespace

const NodeDef& nodeDefinition(NodeKind kind) {
    for (const auto& def : kNodeDefs) {
        if (def.kind == kind) {
            return def;
        }
    }
    throw std::invalid_argument("Unknown node kind");
}

// === Default Params ===

NodeParams defaultParams(NodeKind kind) {
    switch (kind) {
    case NodeKind::Fbm:
        return NoiseParams{};
    case NodeKind::RidgedFbm:
        return NoiseParams{};
    case NodeKind::Simplex:
        return NoiseParams{};
    case NodeKind::Perlin:
        return NoiseParams{};
    case NodeKind::Mountains:
        return MountainParams{};
    case NodeKind::Valleys:
        return ValleyParams{};
    case NodeKind::Plains:
        return PlainsParams{};
    case NodeKind::Plateaus:
        return PlateauParams{};
    case NodeKind::TerrainSynthesis:
        return TerrainSynthesisParams{};
    }
    throw std::invalid_argument("Unknown node kind");
}

// === Default Graph ===
//
//  10 noise sources → Mountains / Valleys / Plains / Plateaus → TerrainSynthesis
//
EditorGraph defaultGraph() {
    EditorGraph g;

    const float baseFreq = 0.007f;

    // ---- Tier 1 (left): Noise sources, spread vertically ----
    // x = 100, y = 100 + i*160
    g.nodes.push_back({0,  NodeKind::Fbm,        100.0f,  100.0f, NoiseParams{baseFreq, 6, 2.0f, 0.50f, 2.0f, 0.0f,     0.0f}});     // continental
    g.nodes.push_back({1,  NodeKind::RidgedFbm,  100.0f,  260.0f, NoiseParams{baseFreq, 6, 2.0f, 0.50f, 2.0f, 0.0f,     0.0f}});     // ridges
    g.nodes.push_back({2,  NodeKind::Simplex,    100.0f,  420.0f,  NoiseParams{baseFreq * 2.70f, 1, 2.0f, 0.50f, 2.0f, 0.0f,     0.0f}});     // detail
    g.nodes.push_back({3,  NodeKind::Perlin,     100.0f,  580.0f,  NoiseParams{baseFreq * 0.30f, 1, 2.0f, 0.45f, 2.0f, 400.0f,  -250.0f}});  // rangeMask
    g.nodes.push_back({4,  NodeKind::Fbm,        100.0f,  740.0f, NoiseParams{baseFreq * 0.28f, 3, 2.0f, 0.52f, 2.0f, -191.7f, 83.4f}});   // basin
    g.nodes.push_back({5,  NodeKind::Fbm,        100.0f,  900.0f, NoiseParams{baseFreq * 0.17f, 3, 2.0f, 0.45f, 2.0f, 420.0f,  -301.0f}});  // rimMask
    g.nodes.push_back({6,  NodeKind::Fbm,        100.0f,  1060.0f, NoiseParams{baseFreq, 4, 2.0f, 0.50f, 2.0f, -63.2f,  41.8f}});   // plainsBase
    g.nodes.push_back({7,  NodeKind::Fbm,        100.0f,  1220.0f, NoiseParams{baseFreq * 0.30f, 3, 2.0f, 0.48f, 2.0f, 219.4f,  -174.6f}});  // macroRelief
    g.nodes.push_back({8,  NodeKind::Fbm,        100.0f,  1380.0f, NoiseParams{baseFreq * 0.82f, 5, 2.0f, 0.50f, 2.0f, -141.5f, 96.8f}});   // hilliness
    g.nodes.push_back({9,  NodeKind::Simplex,    100.0f,  1540.0f, NoiseParams{baseFreq * 0.028f, 1, 2.0f, 0.52f, 2.0f, 0.0f,     0.0f}});     // plateauMask

    // ---- Tier 2 (middle): Subsystems ----
    // x = 500
    g.nodes.push_back({10, NodeKind::Mountains,  500.0f,  300.0f, MountainParams{}});
    g.nodes.push_back({11, NodeKind::Valleys,    500.0f,  600.0f, ValleyParams{}});
    g.nodes.push_back({12, NodeKind::Plains,     500.0f,  900.0f, PlainsParams{}});
    g.nodes.push_back({13, NodeKind::Plateaus,   500.0f,  1200.0f, PlateauParams{}});

    // ---- Tier 3 (right): Sink ----
    // x = 900, centered vertically
    g.nodes.push_back({14, NodeKind::TerrainSynthesis, 900.0f,  750.0f, TerrainSynthesisParams{}});

    // ---- Links ----

    // Mountains (10) ← continental, ridges, detail, rangeMask
    g.links.push_back({0, {0, 0}, {10, 0}});  // continental → mountains
    g.links.push_back({1, {1, 0}, {10, 1}});  // ridges → mountains
    g.links.push_back({2, {2, 0}, {10, 2}});  // detail → mountains
    g.links.push_back({3, {3, 0}, {10, 3}});  // rangeMask → mountains

    // Valleys (11) ← continental, basin, detail, rimMask
    g.links.push_back({4, {0, 0}, {11, 0}});  // continental → valleys
    g.links.push_back({5, {4, 0}, {11, 1}});  // basin → valleys
    g.links.push_back({6, {2, 0}, {11, 2}});  // detail → valleys
    g.links.push_back({7, {5, 0}, {11, 3}});  // rimMask → valleys

    // Plains (12) ← continental, plainsBase, macroRelief, hilliness, detail
    g.links.push_back({8,  {0, 0}, {12, 0}});  // continental → plains
    g.links.push_back({9,  {6, 0}, {12, 1}});  // plainsBase → plains
    g.links.push_back({10, {7, 0}, {12, 2}});  // macroRelief → plains
    g.links.push_back({11, {8, 0}, {12, 3}});  // hilliness → plains
    g.links.push_back({12, {2, 0}, {12, 4}});  // detail → plains

    // Plateaus (13) ← continental, plateauMask, detail
    g.links.push_back({13, {0, 0}, {13, 0}});  // continental → plateaus
    g.links.push_back({14, {9, 0}, {13, 1}});  // plateauMask → plateaus
    g.links.push_back({15, {2, 0}, {13, 2}});  // detail → plateaus

    // TerrainSynthesis (14) ← mountain, valley, plains, plateau, detail
    g.links.push_back({16, {10, 0}, {14, 0}});  // mountains → synthesis
    g.links.push_back({17, {11, 0}, {14, 1}});  // valleys → synthesis
    g.links.push_back({18, {12, 0}, {14, 2}});  // plains → synthesis
    g.links.push_back({19, {13, 0}, {14, 3}});  // plateaus → synthesis
    g.links.push_back({20, {2, 0},  {14, 4}});  // detail → synthesis

    return g;
}

} // namespace graph
