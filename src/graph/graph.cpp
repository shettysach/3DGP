#include <stdexcept>

#include "graph/types.h"

namespace graph {

namespace {

    const NodeDef kNodeDefs[] = {
        {
            NodeKind::Fbm,
            "FBm Noise",
            {{"vec2", PinType::Vec2}},
            {{"float", PinType::Float}},
        },
        {
            NodeKind::RidgedFbm,
            "Ridged Noise",
            {{"vec2", PinType::Vec2}},
            {{"float", PinType::Float}},
        },
        {
            NodeKind::FractalPerlin,
            "Fractal Perlin",
            {{"vec2", PinType::Vec2}},
            {{"float", PinType::Float}},
        },
        {
            NodeKind::Perlin,
            "Perlin Noise",
            {{"vec2", PinType::Vec2}},
            {{"float", PinType::Float}},
        },
        {
            NodeKind::Simplex,
            "Simplex Noise",
            {{"vec2", PinType::Vec2}},
            {{"float", PinType::Float}},
        },
        {
            NodeKind::Mountain,
            "Mountain",
            {
                {"continental", PinType::Float},
                {"ridges", PinType::Float},
                {"rangeMask", PinType::Float},
            },
            {{"vec2", PinType::Vec2}},
        },
        {
            NodeKind::Valley,
            "Valley",
            {
                {"continental", PinType::Float},
                {"basin", PinType::Float},
                {"rimMask", PinType::Float},
            },
            {{"vec2", PinType::Vec2}},
        },
        {
            NodeKind::Plains,
            "Plains",
            {
                {"continental", PinType::Float},
                {"plainsBase", PinType::Float},
            },
            {{"height", PinType::Float}},
        },
        {
            NodeKind::Plateau,
            "Plateau",
            {
                {"continental", PinType::Float},
                {"plateauFeature", PinType::Float},
                {"plateauMask", PinType::Float},
            },
            {{"vec2", PinType::Vec2}},
        },
        {
            NodeKind::Terrace,
            "Terrace",
            {
                {"input", PinType::Float},
            },
            {{"float", PinType::Float}},
        },
        {
            NodeKind::Smoothstep,
            "Smoothstep",
            {
                {"x", PinType::Float},
            },
            {{"float", PinType::Float}},
        },
        {
            NodeKind::Lerp,
            "Lerp",
            {
                {"a", PinType::Float},
                {"b", PinType::Float},
                {"t", PinType::Float},
            },
            {{"float", PinType::Float}},
        },
        {
            NodeKind::Blend,
            "Blend",
            {
                {"mountain", PinType::Vec2},
                {"valley", PinType::Vec2},
                {"plains", PinType::Float},
                {"plateau", PinType::Vec2},
            },
            {/* no outputs — final sink */},
        },
        {
            NodeKind::Position,
            "Position",
            {},
            {{"vec2", PinType::Vec2}},
        },
        {
            NodeKind::CreateVec2,
            "Vec2",
            {
                {"x", PinType::Float},
                {"y", PinType::Float},
            },
            {{"vec2", PinType::Vec2}},
        },
        {
            NodeKind::Add2,
            "Add2",
            {
                {"a", PinType::Vec2},
                {"b", PinType::Vec2},
            },
            {{"vec2", PinType::Vec2}},
        },
        {
            NodeKind::Scale2,
            "Scale2",
            {
                {"vec2", PinType::Vec2},
            },
            {{"vec2", PinType::Vec2}},
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
            return NoiseParams {};
        case NodeKind::Mountain:
            return MountainParams {};
        case NodeKind::Valley:
            return ValleyParams {};
        case NodeKind::Plains:
            return PlainsParams {};
        case NodeKind::Plateau:
            return PlateauParams {};
        case NodeKind::Terrace:
            return TerraceParams {};
        case NodeKind::Smoothstep:
            return SmoothstepParams {};
        case NodeKind::Lerp:
            return LerpParams {};
        case NodeKind::Blend:
            return BlendParams {};
        case NodeKind::CreateVec2:
            return CreateVec2Params {};
        case NodeKind::Scale2:
            return Scale2Params {};
        case NodeKind::Position:
        case NodeKind::Add2:
            return std::monostate {};
    }
    throw std::invalid_argument("Unknown node kind");
}

EditorGraph defaultGraph() {
    EditorGraph g;

    const float baseFreq = 0.007f;
    const float warpFreq = 0.003f;

    g.nodes.push_back(
        {0,
         NodeKind::Fbm,
         1050.0f,
         50.0f,
         NoiseParams {baseFreq, 6, 2.0f, 0.50f, 2.0f, 0.0f, 0.0f}}
    );
    g.nodes.push_back(
        {1,
         NodeKind::RidgedFbm,
         1050.0f,
         250.0f,
         NoiseParams {baseFreq, 6, 2.0f, 0.50f, 2.0f, 0.0f, 0.0f}}
    );
    g.nodes.push_back(
        {2,
         NodeKind::Fbm,
         1050.0f,
         450.0f,
         NoiseParams {baseFreq * 0.26f, 3, 2.0f, 0.52f, 2.0f, -191.7f, 83.4f}}
    );
    g.nodes.push_back(
        {3,
         NodeKind::Fbm,
         1050.0f,
         650.0f,
         NoiseParams {baseFreq * 0.70f, 4, 2.0f, 0.50f, 2.0f, 130.0f, -50.0f}}
    );
    g.nodes.push_back(
        {4,
         NodeKind::Fbm,
         1050.0f,
         850.0f,
         NoiseParams {baseFreq * 0.60f, 3, 2.0f, 0.50f, 2.0f, 0.0f, 0.0f}}
    );

    g.nodes.push_back(
        {5,
         NodeKind::Perlin,
         1050.0f,
         1050.0f,
         NoiseParams {baseFreq * 0.30f, 1, 2.0f, 0.50f, 2.0f, 500.0f, -250.0f}}
    );
    g.nodes.push_back(
        {6,
         NodeKind::Perlin,
         1050.0f,
         1250.0f,
         NoiseParams {baseFreq * 0.17f, 1, 2.0f, 0.50f, 2.0f, 420.0f, -301.0f}}
    );
    g.nodes.push_back(
        {7,
         NodeKind::Perlin,
         1050.0f,
         1450.0f,
         NoiseParams {baseFreq * 0.40f, 1, 2.0f, 0.50f, 2.0f, 0.0f, 0.0f}}
    );

    g.nodes.push_back(
        {8, NodeKind::Mountain, 1450.0f, 250.0f, MountainParams {}}
    );
    g.nodes.push_back({9, NodeKind::Valley, 1450.0f, 450.0f, ValleyParams {}});
    g.nodes.push_back({10, NodeKind::Plains, 1450.0f, 750.0f, PlainsParams {}});
    g.nodes.push_back(
        {11, NodeKind::Plateau, 1450.0f, 1050.0f, PlateauParams {}}
    );

    g.nodes.push_back(
        {19,
         NodeKind::Smoothstep,
         1250.0f,
         1050.0f,
         SmoothstepParams {0.42f, 0.72f}}
    );
    g.nodes.push_back(
        {20,
         NodeKind::Smoothstep,
         1250.0f,
         1250.0f,
         SmoothstepParams {0.38f, 0.74f}}
    );

    g.nodes.push_back({12, NodeKind::Blend, 1850.0f, 600.0f, BlendParams {}});

    g.nodes.push_back(
        {13, NodeKind::Position, 50.0f, 50.0f, std::monostate {}}
    );
    g.nodes.push_back(
        {14,
         NodeKind::Fbm,
         250.0f,
         250.0f,
         NoiseParams {warpFreq, 3, 2.0f, 0.50f, 2.0f, 0.0f, 0.0f}}
    );
    g.nodes.push_back(
        {15,
         NodeKind::Fbm,
         250.0f,
         450.0f,
         NoiseParams {warpFreq, 3, 2.0f, 0.50f, 2.0f, 317.4f, -271.8f}}
    );
    g.nodes.push_back(
        {16,
         NodeKind::CreateVec2,
         500.0f,
         350.0f,
         CreateVec2Params {-0.5f, -0.5f}}
    );
    g.nodes.push_back(
        {17, NodeKind::Scale2, 650.0f, 350.0f, Scale2Params {8.0f}}
    );
    g.nodes.push_back({18, NodeKind::Add2, 800.0f, 50.0f, std::monostate {}});

    g.links.push_back({0, {8, 0}, {12, 0}});
    g.links.push_back({1, {9, 0}, {12, 1}});
    g.links.push_back({2, {10, 0}, {12, 2}});
    g.links.push_back({3, {11, 0}, {12, 3}});

    g.links.push_back({4, {0, 0}, {8, 0}});
    g.links.push_back({5, {1, 0}, {8, 1}});
    g.links.push_back({6, {5, 0}, {19, 0}});
    g.links.push_back({30, {19, 0}, {8, 2}});
    g.links.push_back({7, {0, 0}, {9, 0}});
    g.links.push_back({8, {2, 0}, {9, 1}});
    g.links.push_back({9, {6, 0}, {20, 0}});
    g.links.push_back({31, {20, 0}, {9, 2}});
    g.links.push_back({10, {0, 0}, {10, 0}});
    g.links.push_back({11, {3, 0}, {10, 1}});
    g.links.push_back({12, {0, 0}, {11, 0}});
    g.links.push_back({13, {4, 0}, {11, 1}});
    g.links.push_back({14, {7, 0}, {11, 2}});

    g.links.push_back({15, {13, 0}, {14, 0}});
    g.links.push_back({16, {13, 0}, {15, 0}});
    g.links.push_back({17, {14, 0}, {16, 0}});
    g.links.push_back({18, {15, 0}, {16, 1}});
    g.links.push_back({19, {16, 0}, {17, 0}});
    g.links.push_back({20, {13, 0}, {18, 0}});
    g.links.push_back({21, {17, 0}, {18, 1}});

    g.links.push_back({22, {18, 0}, {0, 0}});
    g.links.push_back({23, {18, 0}, {1, 0}});
    g.links.push_back({24, {18, 0}, {2, 0}});
    g.links.push_back({25, {18, 0}, {3, 0}});
    g.links.push_back({26, {18, 0}, {4, 0}});
    g.links.push_back({27, {18, 0}, {5, 0}});
    g.links.push_back({28, {18, 0}, {6, 0}});
    g.links.push_back({29, {18, 0}, {7, 0}});

    return g;
}

EditorGraph preset1Graph() {
    EditorGraph g;

    const float baseFreq = 0.007f;
    const float blendFreq = 0.0015f;

    g.nodes.push_back({0, NodeKind::Position, 50.0f, 50.0f, std::monostate {}});

    g.nodes.push_back(
        {1,
         NodeKind::Fbm,
         300.0f,
         50.0f,
         NoiseParams {baseFreq, 5, 2.0f, 0.50f, 2.0f, 0.0f, 0.0f}}
    );
    g.nodes.push_back(
        {2,
         NodeKind::RidgedFbm,
         300.0f,
         250.0f,
         NoiseParams {baseFreq, 5, 2.0f, 0.50f, 2.0f, 317.4f, -271.8f}}
    );
    g.nodes.push_back(
        {3,
         NodeKind::FractalPerlin,
         300.0f,
         450.0f,
         NoiseParams {baseFreq, 5, 2.0f, 0.50f, 2.0f, -191.7f, 83.4f}}
    );
    g.nodes.push_back(
        {4,
         NodeKind::Fbm,
         300.0f,
         650.0f,
         NoiseParams {baseFreq * 0.8f, 5, 2.0f, 0.52f, 2.0f, 130.0f, -50.0f}}
    );

    g.nodes.push_back(
        {5,
         NodeKind::Fbm,
         50.0f,
         300.0f,
         NoiseParams {blendFreq, 3, 2.0f, 0.50f, 2.0f, 500.0f, -250.0f}}
    );
    g.nodes.push_back(
        {6,
         NodeKind::Fbm,
         50.0f,
         500.0f,
         NoiseParams {blendFreq, 3, 2.0f, 0.50f, 2.0f, 420.0f, -301.0f}}
    );

    g.nodes.push_back(
        {7, NodeKind::Lerp, 550.0f, 150.0f, LerpParams {0.0f, 1.0f, 0.5f}}
    );
    g.nodes.push_back(
        {8, NodeKind::Lerp, 550.0f, 350.0f, LerpParams {0.0f, 1.0f, 0.5f}}
    );
    g.nodes.push_back(
        {9, NodeKind::Lerp, 800.0f, 250.0f, LerpParams {0.0f, 1.0f, 0.5f}}
    );

    g.nodes.push_back(
        {10,
         NodeKind::Perlin,
         1050.0f,
         50.0f,
         NoiseParams {baseFreq * 0.30f, 1, 2.0f, 0.50f, 2.0f, 0.0f, 0.0f}}
    );
    g.nodes.push_back(
        {11,
         NodeKind::Perlin,
         1050.0f,
         250.0f,
         NoiseParams {baseFreq * 0.17f, 1, 2.0f, 0.50f, 2.0f, 0.0f, 0.0f}}
    );

    g.nodes.push_back(
        {12, NodeKind::Mountain, 1350.0f, 50.0f, MountainParams {}}
    );
    g.nodes.push_back({13, NodeKind::Valley, 1350.0f, 250.0f, ValleyParams {}});
    g.nodes.push_back({14, NodeKind::Plains, 1350.0f, 450.0f, PlainsParams {}});

    g.nodes.push_back({15, NodeKind::Blend, 1650.0f, 250.0f, BlendParams {}});

    g.links.push_back({0, {0, 0}, {1, 0}});
    g.links.push_back({1, {0, 0}, {2, 0}});
    g.links.push_back({2, {0, 0}, {3, 0}});
    g.links.push_back({3, {0, 0}, {4, 0}});
    g.links.push_back({4, {0, 0}, {5, 0}});
    g.links.push_back({5, {0, 0}, {6, 0}});
    g.links.push_back({6, {0, 0}, {10, 0}});
    g.links.push_back({7, {0, 0}, {11, 0}});

    g.links.push_back({8, {1, 0}, {7, 0}});
    g.links.push_back({9, {2, 0}, {7, 1}});
    g.links.push_back({10, {5, 0}, {7, 2}});

    g.links.push_back({11, {3, 0}, {8, 0}});
    g.links.push_back({12, {4, 0}, {8, 1}});
    g.links.push_back({13, {5, 0}, {8, 2}});

    g.links.push_back({14, {7, 0}, {9, 0}});
    g.links.push_back({15, {8, 0}, {9, 1}});
    g.links.push_back({16, {6, 0}, {9, 2}});

    g.links.push_back({17, {9, 0}, {12, 0}});
    g.links.push_back({18, {9, 0}, {12, 1}});
    g.links.push_back({19, {10, 0}, {12, 2}});

    g.links.push_back({20, {9, 0}, {13, 0}});
    g.links.push_back({21, {9, 0}, {13, 1}});
    g.links.push_back({22, {11, 0}, {13, 2}});

    g.links.push_back({23, {9, 0}, {14, 0}});
    g.links.push_back({24, {9, 0}, {14, 1}});

    g.links.push_back({25, {12, 0}, {15, 0}});
    g.links.push_back({26, {13, 0}, {15, 1}});
    g.links.push_back({27, {14, 0}, {15, 2}});

    return g;
}

} // namespace graph
