#include "graph/types.h"

#include <stdexcept>

namespace graph {

// === Node Definition Table ===

namespace {

const NodeDef kNodeDefs[] = {
    {
        NodeKind::Fbm,
        "FBm Noise",
        {{"vec2", true, PinType::Vec2}},
        {{"float", false, PinType::Float}},
    },
    {
        NodeKind::RidgedFbm,
        "Ridged Noise",
        {{"vec2", true, PinType::Vec2}},
        {{"float", false, PinType::Float}},
    },
    {
        NodeKind::FractalPerlin,
        "Fractal Perlin",
        {{"vec2", true, PinType::Vec2}},
        {{"float", false, PinType::Float}},
    },
    {
        NodeKind::Perlin,
        "Perlin Noise",
        {{"vec2", true, PinType::Vec2}},
        {{"float", false, PinType::Float}},
    },
    {
        NodeKind::Simplex,
        "Simplex Noise",
        {{"vec2", true, PinType::Vec2}},
        {{"float", false, PinType::Float}},
    },
    {
        NodeKind::TerrainSynthesis,
        "Terrain Synthesis",
        {
            {"continental",    true, PinType::Float},
            {"ridges",         true, PinType::Float},
            {"detail",         true, PinType::Float},
            {"rangeMask",      true, PinType::Float},
            {"basin",          true, PinType::Float},
            {"detailBand",     true, PinType::Float},
            {"rimMask",        true, PinType::Float},
            {"plainsBase",     true, PinType::Float},
            {"macroRelief",    true, PinType::Float},
            {"hilliness",      true, PinType::Float},
            {"basinNoise",     true, PinType::Float},
            {"plateauMask",    true, PinType::Float},
        },
        {/* no outputs — final sink */},
    },
    {
        NodeKind::Position,
        "Position",
        {/* inputs */},
        {{"vec2", false, PinType::Vec2}},
    },
    {
        NodeKind::CreateVec2,
        "Vec2",
        {
            {"x", true, PinType::Float},
            {"y", true, PinType::Float},
        },
        {{"vec2", false, PinType::Vec2}},
    },
    {
        NodeKind::Add2,
        "Add2",
        {
            {"a", true, PinType::Vec2},
            {"b", true, PinType::Vec2},
        },
        {{"vec2", false, PinType::Vec2}},
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
    case NodeKind::RidgedFbm:
    case NodeKind::FractalPerlin:
    case NodeKind::Perlin:
    case NodeKind::Simplex:
        return NoiseParams{};
    case NodeKind::TerrainSynthesis:
        return TerrainSynthesisParams{};
    case NodeKind::CreateVec2:
        return CreateVec2Params{};
    case NodeKind::Position:
    case NodeKind::Add2:
        return std::monostate{};
    }
    throw std::invalid_argument("Unknown node kind");
}

// === Default Graph ===
//
//  Domain-warp subgraph (Position → 2×Fbm → CreateVec2 → Add2)
//    ↓
//  12 source noise nodes (each samples at warped coord)
//    ↓
//  TerrainSynthesis
//
EditorGraph defaultGraph() {
    EditorGraph g;

    const float baseFreq = 0.007f;
    const float warpFreq = 0.003f;

    // ── Terrain noise generators ──────────────────────────────────────────
    const float nx = 850.0f;
    g.nodes.push_back({0,  NodeKind::Fbm,           nx,  50.0f, NoiseParams{baseFreq, 6, 2.0f, 0.50f, 2.0f, 0.0f,    0.0f}});      // continental
    g.nodes.push_back({1,  NodeKind::RidgedFbm,     nx, 200.0f, NoiseParams{baseFreq, 6, 2.0f, 0.50f, 2.0f, 0.0f,    0.0f}});      // ridges
    g.nodes.push_back({2,  NodeKind::Simplex,       nx, 350.0f, NoiseParams{baseFreq * 2.70f, 1, 2.0f, 0.50f, 2.0f, 0.0f,    0.0f}});      // detail
    g.nodes.push_back({3,  NodeKind::Perlin,        nx, 500.0f, NoiseParams{baseFreq * 0.30f, 1, 2.0f, 0.45f, 2.0f, 400.0f,  -250.0f}});   // rangeMask
    g.nodes.push_back({4,  NodeKind::Fbm,           nx, 650.0f, NoiseParams{baseFreq * 0.28f, 3, 2.0f, 0.52f, 2.0f, -191.7f, 83.4f}});    // basin
    g.nodes.push_back({5,  NodeKind::Fbm,           nx, 800.0f, NoiseParams{baseFreq * 1.90f, 4, 2.0f, 0.50f, 2.0f, 52.3f,   -61.8f}});   // detailBand
    g.nodes.push_back({6,  NodeKind::Perlin,        nx, 950.0f, NoiseParams{baseFreq * 0.17f, 1, 2.0f, 0.45f, 2.0f, 420.0f,  -301.0f}});   // rimMask
    g.nodes.push_back({7,  NodeKind::Fbm,           nx, 1100.0f, NoiseParams{baseFreq, 4, 2.0f, 0.50f, 2.0f, -63.2f,  41.8f}});    // plainsBase
    g.nodes.push_back({8,  NodeKind::FractalPerlin, nx, 1250.0f, NoiseParams{baseFreq * 0.30f, 3, 2.0f, 0.48f, 2.0f, 219.4f,  -174.6f}});   // macroRelief
    g.nodes.push_back({9,  NodeKind::Fbm,           nx, 1400.0f, NoiseParams{baseFreq * 0.82f, 5, 2.0f, 0.50f, 2.0f, -141.5f, 96.8f}});    // hilliness
    g.nodes.push_back({10, NodeKind::Simplex,       nx, 1550.0f, NoiseParams{baseFreq * 0.18f, 1, 2.0f, 0.55f, 2.0f, -331.7f, 271.4f}});   // basinNoise
    g.nodes.push_back({11, NodeKind::Perlin,        nx, 1700.0f, NoiseParams{0.028f * baseFreq, 1, 2.0f, 0.52f, 2.0f, 0.0f,    0.0f}});     // plateauMask

    // Sink
    g.nodes.push_back({12, NodeKind::TerrainSynthesis, 1450.0f, 875.0f, TerrainSynthesisParams{}});

    // ── Domain-warp subgraph ──────────────────────────────────────────────
    g.nodes.push_back({13, NodeKind::Position,       50.0f,   50.0f, std::monostate{}});
    g.nodes.push_back({14, NodeKind::Fbm,            50.0f,  250.0f, NoiseParams{warpFreq, 3, 2.0f, 0.50f, 2.0f, 0.0f, 0.0f}});  // warpX
    g.nodes.push_back({15, NodeKind::Fbm,            50.0f,  450.0f, NoiseParams{warpFreq, 3, 2.0f, 0.50f, 2.0f, 0.0f, 0.0f}});  // warpZ
    g.nodes.push_back({16, NodeKind::CreateVec2,    350.0f,  350.0f, CreateVec2Params{-0.5f, -0.5f}});
    g.nodes.push_back({17, NodeKind::Add2,          600.0f,   50.0f, std::monostate{}});  // warpedPos

    // ── Links: noise outputs → TerrainSynthesis ───────────────────────────
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

    // ── Links: domain-warp composition ────────────────────────────────────
    // Position feeds both warp FBMs
    g.links.push_back({12, {13, 0}, {14, 0}});
    g.links.push_back({13, {13, 0}, {15, 0}});
    // FBM outputs → CreateVec2
    g.links.push_back({14, {14, 0}, {16, 0}});
    g.links.push_back({15, {15, 0}, {16, 1}});
    // Position + warpVec → warpedPos
    g.links.push_back({16, {13, 0}, {17, 0}});
    g.links.push_back({17, {16, 0}, {17, 1}});
    // warpedPos feeds all noise nodes as their coord input
    g.links.push_back({18, {17, 0}, {0,  0}});
    g.links.push_back({19, {17, 0}, {1,  0}});
    g.links.push_back({20, {17, 0}, {2,  0}});
    g.links.push_back({21, {17, 0}, {3,  0}});
    g.links.push_back({22, {17, 0}, {4,  0}});
    g.links.push_back({23, {17, 0}, {5,  0}});
    g.links.push_back({24, {17, 0}, {6,  0}});
    g.links.push_back({25, {17, 0}, {7,  0}});
    g.links.push_back({26, {17, 0}, {8,  0}});
    g.links.push_back({27, {17, 0}, {9,  0}});
    g.links.push_back({28, {17, 0}, {10, 0}});
    g.links.push_back({29, {17, 0}, {11, 0}});

    return g;
}

} // namespace graph
