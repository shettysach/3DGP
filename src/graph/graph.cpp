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
        },
        {{"vec2", false, PinType::Vec2}},
    },
    {
        NodeKind::Valley,
        "Valley",
        {
            {"continental", true, PinType::Float},
            {"basin",       true, PinType::Float},
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
        {
            {"height",         false, PinType::Float},
            {"mountainWeight", false, PinType::Float},
        },
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
//  5 source noise nodes (each samples at warped coord)
//    ↓
//  4 terrain nodes (Mountain, Valley, Plains, Plateau)
//    ↓
//  Blend (final sink)
//
EditorGraph defaultGraph() {
    EditorGraph g;

    const float baseFreq = 0.007f;
    const float warpFreq = 0.003f;

    // ── Terrain noise generators (5 nodes) ─────────────────────────────────
    g.nodes.push_back({0, NodeKind::Fbm,        850.0f,   50.0f, NoiseParams{baseFreq,           6, 2.0f, 0.50f, 2.0f, 0.0f,    0.0f}});   // continental
    g.nodes.push_back({1, NodeKind::RidgedFbm,  850.0f,  200.0f, NoiseParams{baseFreq,           6, 2.0f, 0.50f, 2.0f, 0.0f,    0.0f}});   // ridges
    g.nodes.push_back({2, NodeKind::Fbm,        850.0f,  350.0f, NoiseParams{baseFreq * 0.26f,  3, 2.0f, 0.52f, 2.0f, -191.7f, 83.4f}});  // basin
    g.nodes.push_back({3, NodeKind::Fbm,        850.0f,  500.0f, NoiseParams{baseFreq * 0.70f,  4, 2.0f, 0.50f, 2.0f, 130.0f,  -50.0f}});  // plainsBase
    g.nodes.push_back({4, NodeKind::RidgedFbm,  850.0f,  650.0f, NoiseParams{baseFreq * 1.70f,  4, 2.0f, 0.55f, 2.0f, -80.0f,  120.0f}});  // plateauFeature

    // ── Terrain nodes ──────────────────────────────────────────────────────
    g.nodes.push_back({5, NodeKind::Mountain,  1250.0f,  125.0f, MountainParams{}});
    g.nodes.push_back({6, NodeKind::Valley,    1250.0f,  350.0f, ValleyParams{}});
    g.nodes.push_back({7, NodeKind::Plains,    1250.0f,  575.0f, PlainsParams{}});
    g.nodes.push_back({8, NodeKind::Plateau,   1250.0f,  800.0f, PlateauParams{}});

    // ── Blend sink ─────────────────────────────────────────────────────────
    g.nodes.push_back({9, NodeKind::Blend,     1650.0f,  460.0f, BlendParams{}});

    // ── Domain-warp subgraph ────────────────────────────────────────────────
    g.nodes.push_back({10, NodeKind::Position,      50.0f,   50.0f, std::monostate{}});
    g.nodes.push_back({11, NodeKind::Fbm,           50.0f,  250.0f, NoiseParams{warpFreq, 3, 2.0f, 0.50f, 2.0f, 0.0f,    0.0f}});   // warpX
    g.nodes.push_back({12, NodeKind::Fbm,           50.0f,  450.0f, NoiseParams{warpFreq, 3, 2.0f, 0.50f, 2.0f, 317.4f, -271.8f}}); // warpZ
    g.nodes.push_back({13, NodeKind::CreateVec2,   350.0f,  350.0f, CreateVec2Params{-0.5f, -0.5f}});
    g.nodes.push_back({14, NodeKind::Add2,         600.0f,   50.0f, std::monostate{}});  // warpedPos

    // ── Links: terrain nodes → Blend ───────────────────────────────────────
    g.links.push_back({0,  {5, 0}, {9, 0}});   // Mountain → Blend:mountain
    g.links.push_back({1,  {6, 0}, {9, 1}});   // Valley   → Blend:valley
    g.links.push_back({2,  {7, 0}, {9, 2}});   // Plains   → Blend:plains
    g.links.push_back({3,  {8, 0}, {9, 3}});   // Plateau  → Blend:plateau

    // ── Links: noise → terrain nodes ───────────────────────────────────────
    g.links.push_back({4,  {0, 0},  {5, 0}});  // continental → Mountain
    g.links.push_back({5,  {1, 0},  {5, 1}});  // ridges      → Mountain
    g.links.push_back({6,  {0, 0},  {6, 0}});  // continental → Valley
    g.links.push_back({7,  {2, 0},  {6, 1}});  // basin       → Valley
    g.links.push_back({8,  {0, 0},  {7, 0}});  // continental → Plains
    g.links.push_back({9,  {3, 0},  {7, 1}});  // plainsBase  → Plains
    g.links.push_back({10, {0, 0},  {8, 0}});  // continental   → Plateau
    g.links.push_back({11, {4, 0},  {8, 1}});  // plateauFeature → Plateau

    // ── Links: domain-warp composition ─────────────────────────────────────
    // Position feeds both warp FBMs
    g.links.push_back({12, {10, 0}, {11, 0}});
    g.links.push_back({13, {10, 0}, {12, 0}});
    // FBM outputs → CreateVec2
    g.links.push_back({14, {11, 0}, {13, 0}});
    g.links.push_back({15, {12, 0}, {13, 1}});
    // Position + warpVec → warpedPos
    g.links.push_back({16, {10, 0}, {14, 0}});
    g.links.push_back({17, {13, 0}, {14, 1}});
    // warpedPos feeds all 5 noise nodes as their coord input
    g.links.push_back({18, {14, 0}, {0,  0}});
    g.links.push_back({19, {14, 0}, {1,  0}});
    g.links.push_back({20, {14, 0}, {2,  0}});
    g.links.push_back({21, {14, 0}, {3,  0}});
    g.links.push_back({22, {14, 0}, {4,  0}});

    return g;
}

} // namespace graph
