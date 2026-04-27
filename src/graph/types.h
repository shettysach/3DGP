#ifndef GRAPH_TYPES_H
#define GRAPH_TYPES_H

#include <cstdint>
#include <variant>
#include <vector>

namespace graph {

// === Ids ===

using NodeId = int32_t;
using LinkId = int32_t;

// === Data Types ===

struct Vec2 { float x = 0.0f; float y = 0.0f; };

enum class PinType : uint8_t { Float, Vec2 };

// === Node Kinds ===

enum class NodeKind : uint8_t {
    Fbm,
    RidgedFbm,
    FractalPerlin,
    Perlin,
    Simplex,
    TerrainSynthesis,
    Position,
    CreateVec2,
    Add2,
};

// === Pin References ===

struct PinRef {
    NodeId nodeId = 0;
    uint8_t slot  = 0;
};

// === Params ===

struct NoiseParams {
    float frequency   = 0.007f;
    int   octaves     = 6;
    float lacunarity  = 2.0f;
    float gain        = 0.5f;
    float sharpness   = 2.0f;
    float xOffset = 0.0f;
    float zOffset = 0.0f;
};

struct TerrainSynthesisParams {
    float verticalScale = 80.0f;
};

struct CreateVec2Params {
    float x = 0.0f;
    float y = 0.0f;
};

using NodeParams = std::variant<NoiseParams, TerrainSynthesisParams, CreateVec2Params, std::monostate>;

// === Editor Graph Model ===

struct EditorNode {
    NodeId   id    = 0;
    NodeKind kind  = NodeKind::Fbm;
    float    posX  = 0.0f;
    float    posY  = 0.0f;
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
    std::vector<uint16_t> inputs;       // source node index per input slot, UINT16_MAX = unconnected
    bool      hasCoordInput = false;    // for noise nodes: is the optional Vec2 coord connected?
};

struct CompiledGraph {
    std::vector<CompiledNode> nodes;
};

// === Node Definition Table ===

struct PinDef {
    const char* label;
    bool        isInput = false;
    PinType     type    = PinType::Float;
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

// Builds the default graph (Fbm + RidgedFbm → TerrainSynthesis).
EditorGraph defaultGraph();

} // namespace graph

#endif // GRAPH_TYPES_H
