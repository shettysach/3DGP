#ifndef GRAPH_TYPES_H
#define GRAPH_TYPES_H

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace graph {

// === Ids ===

using NodeId = int32_t;
using LinkId = int32_t;

// === Data Types ===

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

enum class PinType : uint8_t { Float, Vec2 };

// === Node Kinds ===

enum class NodeKind : uint8_t {
    Fbm,
    RidgedFbm,
    FractalPerlin,
    Perlin,
    Simplex,
    Mountain,
    Valley,
    Plains,
    Plateau,
    Blend,
    Position,
    CreateVec2,
    Add2,
};

// === Pin References ===

struct PinRef {
    NodeId nodeId = 0;
    uint8_t slot = 0;
};

// === Params ===

struct NoiseParams {
    float frequency = 0.007f;
    int octaves = 6;
    float lacunarity = 2.0f;
    float gain = 0.5f;
    float sharpness = 2.0f;
    float xOffset = 0.0f;
    float zOffset = 0.0f;
};

struct MountainParams {
    float heightScale = 0.95f;
    float coverage = 0.48f;
    float sharpness = 1.35f;
};

struct ValleyParams {
    float depthScale = 0.50f;
    float coverage = 0.58f;
};

struct PlainsParams {
    float heightScale = 0.78f;
    float relief = 0.36f;
};

struct PlateauParams {
    float heightScale = 0.42f;
    float coverage = 0.53f;
    float cliffness = 1.0f;
};

struct BlendParams {
    // no tunables currently
};

struct CreateVec2Params {
    float x = 0.0f;
    float y = 0.0f;
};

using NodeParams = std::variant<
    NoiseParams,
    MountainParams,
    ValleyParams,
    PlainsParams,
    PlateauParams,
    BlendParams,
    CreateVec2Params,
    std::monostate>;

// === Editor Graph Model ===

struct EditorNode {
    NodeId id = 0;
    NodeKind kind = NodeKind::Fbm;
    float posX = 0.0f;
    float posY = 0.0f;
    NodeParams params;
};

struct EditorLink {
    LinkId id = 0;
    PinRef from;
    PinRef to;
};

struct EditorGraph {
    std::vector<EditorNode> nodes;
    std::vector<EditorLink> links;
};

// === Compiled Graph ===

struct CompiledNode {
    NodeKind kind;
    NodeParams params;
    std::vector<std::optional<uint16_t>> inputs;
};

struct CompiledGraph {
    std::vector<CompiledNode> nodes;
};

// === Node Definition Table ===

struct PinDef {
    const char* label;
    bool isInput = false;
    PinType type = PinType::Float;
};

struct NodeDef {
    NodeKind kind;
    const char* name;
    std::vector<PinDef> inputs;
    std::vector<PinDef> outputs;
};

// Returns static pin layout for a given node kind.
const NodeDef& nodeDefinition(NodeKind kind);

// Returns default params for a given node kind.
NodeParams defaultParams(NodeKind kind);

// Builds the default graph.
EditorGraph defaultGraph();

} // namespace graph

#endif // GRAPH_TYPES_H
