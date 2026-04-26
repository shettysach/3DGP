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
        NodeKind::TerrainSynthesis,
        "Terrain Synthesis",
        {
            {"continental"},
            {"ridges"},
            {"detail"},
            {"rangeMask"},
            {"basin"},
            {"detailBand"},
            {"rimMask"},
            {"plainsBase"},
            {"macroRelief"},
            {"hilliness"},
            {"basinNoise"},
            {"plateauMask"},
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
    case NodeKind::TerrainSynthesis:
        return TerrainSynthesisParams{};
    }
    throw std::invalid_argument("Unknown node kind");
}

// === Default Graph ===
//
//  12 FBm/RidgedFbm source nodes → TerrainSynthesis
//
EditorGraph defaultGraph() {
    EditorGraph g;

    const float baseFreq = 0.007f;

    // Row 1 (y = 100)
    g.nodes.push_back({0,  NodeKind::Fbm,        100.0f, 100.0f, NoiseParams{baseFreq, 6, 2.0f, 0.50f, 2.0f, 0.0f,    0.0f}});     // continental
    g.nodes.push_back({1,  NodeKind::RidgedFbm,  300.0f, 100.0f, NoiseParams{baseFreq, 6, 2.0f, 0.50f, 2.0f, 0.0f,    0.0f}});     // ridges
    g.nodes.push_back({2,  NodeKind::Fbm,        500.0f, 100.0f, NoiseParams{baseFreq * 2.70f, 4, 2.0f, 0.50f, 2.0f, 0.0f,    0.0f}});     // detail
    g.nodes.push_back({3,  NodeKind::Fbm,        700.0f, 100.0f, NoiseParams{baseFreq * 0.30f, 3, 2.0f, 0.45f, 2.0f, 400.0f,  -250.0f}});  // rangeMask
    g.nodes.push_back({4,  NodeKind::Fbm,        900.0f, 100.0f, NoiseParams{baseFreq * 0.28f, 3, 2.0f, 0.52f, 2.0f, -191.7f, 83.4f}});   // basin
    g.nodes.push_back({5,  NodeKind::Fbm,        1100.0f, 100.0f, NoiseParams{baseFreq * 1.90f, 4, 2.0f, 0.50f, 2.0f, 52.3f,   -61.8f}});  // detailBand

    // Row 2 (y = 300)
    g.nodes.push_back({6,  NodeKind::Fbm,        100.0f, 300.0f, NoiseParams{baseFreq * 0.17f, 3, 2.0f, 0.45f, 2.0f, 420.0f,  -301.0f}});  // rimMask
    g.nodes.push_back({7,  NodeKind::Fbm,        300.0f, 300.0f, NoiseParams{baseFreq, 4, 2.0f, 0.50f, 2.0f, -63.2f,  41.8f}});   // plainsBase
    g.nodes.push_back({8,  NodeKind::Fbm,        500.0f, 300.0f, NoiseParams{baseFreq * 0.30f, 3, 2.0f, 0.48f, 2.0f, 219.4f,  -174.6f}});  // macroRelief
    g.nodes.push_back({9, NodeKind::Fbm,        700.0f, 300.0f, NoiseParams{baseFreq * 0.82f, 5, 2.0f, 0.50f, 2.0f, -141.5f, 96.8f}});   // hilliness
    g.nodes.push_back({10, NodeKind::Fbm,        900.0f, 300.0f, NoiseParams{baseFreq * 0.18f, 2, 2.0f, 0.55f, 2.0f, -331.7f, 271.4f}});  // basinNoise
    g.nodes.push_back({11, NodeKind::Fbm,        1100.0f, 300.0f, NoiseParams{0.028f * baseFreq, 3, 2.0f, 0.52f, 2.0f, 0.0f,    0.0f}});     // plateauMask

    // Sink
    g.nodes.push_back({12, NodeKind::TerrainSynthesis, 600.0f, 550.0f, TerrainSynthesisParams{}});

    // Links: source node → TerrainSynthesis input slot
    g.links.push_back({0,  {0,  0}, {12, 0}});   // continental
    g.links.push_back({1,  {1,  0}, {12, 1}});   // ridges
    g.links.push_back({2,  {2,  0}, {12, 2}});   // detail
    g.links.push_back({3,  {3,  0}, {12, 3}});   // rangeMask
    g.links.push_back({4,  {4,  0}, {12, 4}});   // basin
    g.links.push_back({5,  {5,  0}, {12, 5}});   // detailBand
    g.links.push_back({6,  {6,  0}, {12, 6}});   // rimMask
    g.links.push_back({7,  {7,  0}, {12, 7}});   // plainsBase
    g.links.push_back({8,  {8,  0}, {12, 8}});   // macroRelief
    g.links.push_back({9,  {9,  0}, {12, 9}});   // hilliness
    g.links.push_back({10, {10, 0}, {12, 10}});  // basinNoise
    g.links.push_back({11, {11, 0}, {12, 11}});  // plateauMask

    return g;
}

} // namespace graph
