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
        NodeKind::Mountain,
        "Mountain",
        {
            {"continental", true, PinType::Float},
            {"ridges",      true, PinType::Float},
            {"rangeMask",   true, PinType::Float},
        },
        {{"vec2", false, PinType::Vec2}},
    },
    {
        NodeKind::Valley,
        "Valley",
        {
            {"continental", true, PinType::Float},
            {"basin",       true, PinType::Float},
            {"rimMask",     true, PinType::Float},
        },
        {{"vec2", false, PinType::Vec2}},
    },
    {
        NodeKind::Plains,
        "Plains",
        {
            {"continental", true, PinType::Float},
            {"plainsBase",  true, PinType::Float},
        },
        {{"height", false, PinType::Float}},
    },
    {
        NodeKind::Plateau,
        "Plateau",
        {
            {"continental",    true, PinType::Float},
            {"plateauFeature", true, PinType::Float},
            {"plateauMask",    true, PinType::Float},
        },
        {{"vec2", false, PinType::Vec2}},
    },
    {
        NodeKind::Blend,
        "Blend",
        {
            {"mountain", true, PinType::Vec2},
            {"valley",   true, PinType::Vec2},
            {"plains",   true, PinType::Float},
            {"plateau",  true, PinType::Vec2},
        },
        {/* no outputs — final sink */},
    },
    {
        NodeKind::Position,
        "Position",
        {},
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
    case NodeKind::Mountain:
        return MountainParams{};
    case NodeKind::Valley:
        return ValleyParams{};
    case NodeKind::Plains:
        return PlainsParams{};
    case NodeKind::Plateau:
        return PlateauParams{};
    case NodeKind::Blend:
        return BlendParams{};
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
//  8 source noise nodes (each samples at warped coord)
//    ↓
//  4 terrain nodes (Mountain, Valley, Plains, Plateau)
//    ↓
//  Blend (final sink)
//
EditorGraph defaultGraph() {
    EditorGraph g;

    const float baseFreq = 0.007f;
    const float warpFreq = 0.003f;

    // ── Terrain noise generators (8 nodes) ──────────────────────────────────

    // Core shape signals
    g.nodes.push_back({0, NodeKind::Fbm,        850.0f,    50.0f, NoiseParams{baseFreq,           6, 2.0f, 0.50f, 2.0f, 0.0f,    0.0f}});   // continental
    g.nodes.push_back({1, NodeKind::RidgedFbm,  850.0f,   200.0f, NoiseParams{baseFreq,           6, 2.0f, 0.50f, 2.0f, 0.0f,    0.0f}});   // ridges
    g.nodes.push_back({2, NodeKind::Fbm,        850.0f,   350.0f, NoiseParams{baseFreq * 0.26f,  3, 2.0f, 0.52f, 2.0f, -191.7f, 83.4f}});  // basin
    g.nodes.push_back({3, NodeKind::Fbm,        850.0f,   500.0f, NoiseParams{baseFreq * 0.70f,  4, 2.0f, 0.50f, 2.0f, 130.0f,  -50.0f}});  // plainsBase
    g.nodes.push_back({4, NodeKind::Fbm,        850.0f,   650.0f, NoiseParams{baseFreq * 0.60f,  3, 2.0f, 0.50f, 2.0f, 0.0f,    0.0f}});   // plateauFeature

    // Mask signals — Perlin to match original internal generation
    g.nodes.push_back({5, NodeKind::Perlin,     850.0f,  800.0f, NoiseParams{baseFreq * 0.30f,  1, 2.0f, 0.50f, 2.0f, 400.0f, -250.0f}});  // rangeMask
    g.nodes.push_back({6, NodeKind::Perlin,     850.0f,  950.0f, NoiseParams{baseFreq * 0.17f,  1, 2.0f, 0.50f, 2.0f, 420.0f, -301.0f}});  // rimMask
    g.nodes.push_back({7, NodeKind::Perlin,     850.0f, 1100.0f, NoiseParams{baseFreq * 0.40f,  1, 2.0f, 0.50f, 2.0f, 0.0f,    0.0f}});   // plateauMask

    // ── Terrain nodes ───────────────────────────────────────────────────────
    g.nodes.push_back({8,  NodeKind::Mountain,  1250.0f,  125.0f, MountainParams{}});
    g.nodes.push_back({9,  NodeKind::Valley,    1250.0f,  350.0f, ValleyParams{}});
    g.nodes.push_back({10, NodeKind::Plains,    1250.0f,  575.0f, PlainsParams{}});
    g.nodes.push_back({11, NodeKind::Plateau,   1250.0f,  800.0f, PlateauParams{}});

    // ── Blend sink ──────────────────────────────────────────────────────────
    g.nodes.push_back({12, NodeKind::Blend,     1650.0f,  460.0f, BlendParams{}});

    // ── Domain-warp subgraph ─────────────────────────────────────────────────
    g.nodes.push_back({13, NodeKind::Position,      50.0f,   50.0f, std::monostate{}});
    g.nodes.push_back({14, NodeKind::Fbm,           50.0f,  250.0f, NoiseParams{warpFreq, 3, 2.0f, 0.50f, 2.0f, 0.0f,    0.0f}});   // warpX
    g.nodes.push_back({15, NodeKind::Fbm,           50.0f,  450.0f, NoiseParams{warpFreq, 3, 2.0f, 0.50f, 2.0f, 317.4f, -271.8f}}); // warpZ
    g.nodes.push_back({16, NodeKind::CreateVec2,   350.0f,  350.0f, CreateVec2Params{-0.5f, -0.5f}});
    g.nodes.push_back({17, NodeKind::Add2,         600.0f,   50.0f, std::monostate{}});  // warpedPos

    // ── Links: terrain nodes → Blend ────────────────────────────────────────
    g.links.push_back({0,  {8,  0}, {12, 0}});  // Mountain → Blend:mountain
    g.links.push_back({1,  {9,  0}, {12, 1}});  // Valley   → Blend:valley
    g.links.push_back({2,  {10, 0}, {12, 2}});  // Plains   → Blend:plains
    g.links.push_back({3,  {11, 0}, {12, 3}});  // Plateau  → Blend:plateau

    // ── Links: noise → terrain nodes ────────────────────────────────────────
    g.links.push_back({4,  {0,  0}, {8,  0}});  // continental   → Mountain
    g.links.push_back({5,  {1,  0}, {8,  1}});  // ridges        → Mountain
    g.links.push_back({6,  {5,  0}, {8,  2}});  // rangeMask     → Mountain
    g.links.push_back({7,  {0,  0}, {9,  0}});  // continental   → Valley
    g.links.push_back({8,  {2,  0}, {9,  1}});  // basin         → Valley
    g.links.push_back({9,  {6,  0}, {9,  2}});  // rimMask       → Valley
    g.links.push_back({10, {0,  0}, {10, 0}});  // continental   → Plains
    g.links.push_back({11, {3,  0}, {10, 1}});  // plainsBase    → Plains
    g.links.push_back({12, {0,  0}, {11, 0}});  // continental     → Plateau
    g.links.push_back({13, {4,  0}, {11, 1}});  // plateauFeature → Plateau
    g.links.push_back({14, {7,  0}, {11, 2}});  // plateauMask    → Plateau

    // ── Links: domain-warp composition ──────────────────────────────────────
    g.links.push_back({15, {13, 0}, {14, 0}});  // Position → warpX
    g.links.push_back({16, {13, 0}, {15, 0}});  // Position → warpZ
    g.links.push_back({17, {14, 0}, {16, 0}});  // warpX → CreateVec2.x
    g.links.push_back({18, {15, 0}, {16, 1}});  // warpZ → CreateVec2.y
    g.links.push_back({19, {13, 0}, {17, 0}});  // Position → Add2.a
    g.links.push_back({20, {16, 0}, {17, 1}});  // warpVec → Add2.b
    // warpedPos feeds all 8 noise nodes as their coord input
    g.links.push_back({21, {17, 0}, {0,  0}});
    g.links.push_back({22, {17, 0}, {1,  0}});
    g.links.push_back({23, {17, 0}, {2,  0}});
    g.links.push_back({24, {17, 0}, {3,  0}});
    g.links.push_back({25, {17, 0}, {4,  0}});
    g.links.push_back({26, {17, 0}, {5,  0}});
    g.links.push_back({27, {17, 0}, {6,  0}});
    g.links.push_back({28, {17, 0}, {7,  0}});

    return g;
}

} // namespace graph
