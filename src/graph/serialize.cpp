#include "graph/serialize.h"

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

#include "graph/types.h"

namespace graph {

using json = nlohmann::json;

const char* kindToString(NodeKind kind) {
    switch (kind) {
        case NodeKind::Fbm:
            return "Fbm";
        case NodeKind::RidgedFbm:
            return "RidgedFbm";
        case NodeKind::FractalPerlin:
            return "FractalPerlin";
        case NodeKind::Perlin:
            return "Perlin";
        case NodeKind::Simplex:
            return "Simplex";
        case NodeKind::Mountain:
            return "Mountain";
        case NodeKind::Valley:
            return "Valley";
        case NodeKind::Plains:
            return "Plains";
        case NodeKind::Plateau:
            return "Plateau";
        case NodeKind::Blend:
            return "Blend";
        case NodeKind::Position:
            return "Position";
        case NodeKind::CreateVec2:
            return "CreateVec2";
        case NodeKind::Add2:
            return "Add2";
        case NodeKind::Scale2:
            return "Scale2";
    }
    throw std::runtime_error("Unknown NodeKind");
}

NodeKind kindFromString(const std::string& s) {
    if (s == "Fbm")
        return NodeKind::Fbm;
    if (s == "RidgedFbm")
        return NodeKind::RidgedFbm;
    if (s == "FractalPerlin")
        return NodeKind::FractalPerlin;
    if (s == "Perlin")
        return NodeKind::Perlin;
    if (s == "Simplex")
        return NodeKind::Simplex;
    if (s == "Mountain")
        return NodeKind::Mountain;
    if (s == "Valley")
        return NodeKind::Valley;
    if (s == "Plains")
        return NodeKind::Plains;
    if (s == "Plateau")
        return NodeKind::Plateau;
    if (s == "Blend")
        return NodeKind::Blend;
    if (s == "Position")
        return NodeKind::Position;
    if (s == "CreateVec2")
        return NodeKind::CreateVec2;
    if (s == "Add2")
        return NodeKind::Add2;
    if (s == "Scale2")
        return NodeKind::Scale2;
    throw std::runtime_error("Unknown NodeKind string: " + s);
}

static json noiseParamsToJson(const NoiseParams& p) {
    return {
        {"frequency", p.frequency},
        {"octaves", p.octaves},
        {"lacunarity", p.lacunarity},
        {"gain", p.gain},
        {"sharpness", p.sharpness},
        {"xOffset", p.xOffset},
        {"zOffset", p.zOffset},
    };
}

static NoiseParams noiseParamsFromJson(const json& j) {
    NoiseParams p;
    if (j.contains("frequency"))
        p.frequency = j["frequency"].get<float>();
    if (j.contains("octaves"))
        p.octaves = j["octaves"].get<int>();
    if (j.contains("lacunarity"))
        p.lacunarity = j["lacunarity"].get<float>();
    if (j.contains("gain"))
        p.gain = j["gain"].get<float>();
    if (j.contains("sharpness"))
        p.sharpness = j["sharpness"].get<float>();
    if (j.contains("xOffset"))
        p.xOffset = j["xOffset"].get<float>();
    if (j.contains("zOffset"))
        p.zOffset = j["zOffset"].get<float>();
    return p;
}

static json mountainParamsToJson(const MountainParams& p) {
    return {
        {"heightScale", p.heightScale},
        {"coverage", p.coverage},
        {"sharpness", p.sharpness},
    };
}

static MountainParams mountainParamsFromJson(const json& j) {
    MountainParams p;
    if (j.contains("heightScale"))
        p.heightScale = j["heightScale"].get<float>();
    if (j.contains("coverage"))
        p.coverage = j["coverage"].get<float>();
    if (j.contains("sharpness"))
        p.sharpness = j["sharpness"].get<float>();
    return p;
}

static json valleyParamsToJson(const ValleyParams& p) {
    return {
        {"depthScale", p.depthScale},
        {"coverage", p.coverage},
    };
}

static ValleyParams valleyParamsFromJson(const json& j) {
    ValleyParams p;
    if (j.contains("depthScale"))
        p.depthScale = j["depthScale"].get<float>();
    if (j.contains("coverage"))
        p.coverage = j["coverage"].get<float>();
    return p;
}

static json plainsParamsToJson(const PlainsParams& p) {
    return {
        {"heightScale", p.heightScale},
        {"relief", p.relief},
    };
}

static PlainsParams plainsParamsFromJson(const json& j) {
    PlainsParams p;
    if (j.contains("heightScale"))
        p.heightScale = j["heightScale"].get<float>();
    if (j.contains("relief"))
        p.relief = j["relief"].get<float>();
    return p;
}

static json plateauParamsToJson(const PlateauParams& p) {
    return {
        {"heightScale", p.heightScale},
        {"coverage", p.coverage},
        {"cliffness", p.cliffness},
    };
}

static PlateauParams plateauParamsFromJson(const json& j) {
    PlateauParams p;
    if (j.contains("heightScale"))
        p.heightScale = j["heightScale"].get<float>();
    if (j.contains("coverage"))
        p.coverage = j["coverage"].get<float>();
    if (j.contains("cliffness"))
        p.cliffness = j["cliffness"].get<float>();
    return p;
}

static json createVec2ParamsToJson(const CreateVec2Params& p) {
    return {{"x", p.x}, {"y", p.y}};
}

static CreateVec2Params createVec2ParamsFromJson(const json& j) {
    CreateVec2Params p;
    if (j.contains("x"))
        p.x = j["x"].get<float>();
    if (j.contains("y"))
        p.y = j["y"].get<float>();
    return p;
}

static json scale2ParamsToJson(const Scale2Params& p) {
    return {{"scale", p.scale}};
}

static Scale2Params scale2ParamsFromJson(const json& j) {
    Scale2Params p;
    if (j.contains("scale"))
        p.scale = j["scale"].get<float>();
    return p;
}

static json paramsToJson(NodeKind kind, const NodeParams& params) {
    switch (kind) {
        case NodeKind::Fbm:
        case NodeKind::RidgedFbm:
        case NodeKind::FractalPerlin:
        case NodeKind::Perlin:
        case NodeKind::Simplex:
            return noiseParamsToJson(std::get<NoiseParams>(params));
        case NodeKind::Mountain:
            return mountainParamsToJson(std::get<MountainParams>(params));
        case NodeKind::Valley:
            return valleyParamsToJson(std::get<ValleyParams>(params));
        case NodeKind::Plains:
            return plainsParamsToJson(std::get<PlainsParams>(params));
        case NodeKind::Plateau:
            return plateauParamsToJson(std::get<PlateauParams>(params));
        case NodeKind::Blend:
            return json::object();
        case NodeKind::CreateVec2:
            return createVec2ParamsToJson(std::get<CreateVec2Params>(params));
        case NodeKind::Scale2:
            return scale2ParamsToJson(std::get<Scale2Params>(params));
        default:
            break;
    }
    return json::object();
}

static NodeParams paramsFromJson(NodeKind kind, const json& j) {
    switch (kind) {
        case NodeKind::Fbm:
        case NodeKind::RidgedFbm:
        case NodeKind::FractalPerlin:
        case NodeKind::Perlin:
        case NodeKind::Simplex:
            return noiseParamsFromJson(j);
        case NodeKind::Mountain:
            return mountainParamsFromJson(j);
        case NodeKind::Valley:
            return valleyParamsFromJson(j);
        case NodeKind::Plains:
            return plainsParamsFromJson(j);
        case NodeKind::Plateau:
            return plateauParamsFromJson(j);
        case NodeKind::Blend:
            return BlendParams{};
        case NodeKind::CreateVec2:
            return createVec2ParamsFromJson(j);
        case NodeKind::Scale2:
            return scale2ParamsFromJson(j);
        default:
            break;
    }
    return std::monostate{};
}

// Serialize

std::string toJson(const EditorGraph& g) {
    json j;

    json nodesArr = json::array();
    for (const auto& node : g.nodes) {
        json nj;
        nj["id"] = node.id;
        nj["kind"] = kindToString(node.kind);
        nj["pos"] = {node.posX, node.posY};
        nj["params"] = paramsToJson(node.kind, node.params);
        nodesArr.push_back(nj);
    }
    j["nodes"] = nodesArr;

    json linksArr = json::array();
    for (const auto& link : g.links) {
        json lj;
        lj["id"] = link.id;
        lj["from"] = {{"nodeId", link.from.nodeId}, {"slot", link.from.slot}};
        lj["to"] = {{"nodeId", link.to.nodeId}, {"slot", link.to.slot}};
        linksArr.push_back(lj);
    }
    j["links"] = linksArr;

    return j.dump(2);
}

// Deserialize

EditorGraph fromJson(const std::string& text) {
    json j = json::parse(text);

    EditorGraph g;

    for (const auto& nj : j.at("nodes")) {
        EditorNode node;
        node.id = nj.at("id").get<NodeId>();
        node.kind = kindFromString(nj.at("kind").get<std::string>());
        if (nj.contains("pos") && nj["pos"].is_array()
            && nj["pos"].size() == 2) {
            node.posX = nj["pos"][0].get<float>();
            node.posY = nj["pos"][1].get<float>();
        }
        if (nj.contains("params")) {
            node.params = paramsFromJson(node.kind, nj["params"]);
        } else {
            node.params = defaultParams(node.kind);
        }
        g.nodes.push_back(std::move(node));
    }

    for (const auto& lj : j.at("links")) {
        EditorLink link;
        link.id = lj.at("id").get<LinkId>();
        link.from.nodeId = lj["from"].at("nodeId").get<NodeId>();
        link.from.slot = lj["from"].at("slot").get<uint8_t>();
        link.to.nodeId = lj["to"].at("nodeId").get<NodeId>();
        link.to.slot = lj["to"].at("slot").get<uint8_t>();
        g.links.push_back(link);
    }

    return g;
}

} // namespace graph
