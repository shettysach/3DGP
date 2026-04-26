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

//   Fbm(1)           RidgedFbm(2)
//   out[0]           out[0]
//     |                 |
//     v                 v
//   in[0] continental  in[1] ridges
//   TerrainSynthesis(3)
//   out[0..5] → TerrainFields
//
EditorGraph defaultGraph() {
    EditorGraph g;

    g.nodes.push_back({1, NodeKind::Fbm, 200.0f, 200.0f, NoiseParams{}});
    g.nodes.push_back({2, NodeKind::RidgedFbm, 600.0f, 200.0f, NoiseParams{}});
    g.nodes.push_back({3, NodeKind::TerrainSynthesis, 400.0f, 450.0f, TerrainSynthesisParams{}});

    // Fbm → TerrainSynthesis continental (input slot 0)
    g.links.push_back({1, {1, 0}, {3, 0}});
    // RidgedFbm → TerrainSynthesis ridges (input slot 1)
    g.links.push_back({2, {2, 0}, {3, 1}});

    return g;
}

} // namespace graph
