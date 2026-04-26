#include "graph/graph_serialize.h"
#include "graph/types.h"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

namespace graph {

using json = nlohmann::json;

// ---------- NodeKind ↔ string ----------

const char* kindToString(NodeKind kind) {
    switch (kind) {
    case NodeKind::Fbm:               return "Fbm";
    case NodeKind::RidgedFbm:         return "RidgedFbm";
    case NodeKind::Simplex:           return "Simplex";
    case NodeKind::Perlin:            return "Perlin";
    case NodeKind::Mountains:         return "Mountains";
    case NodeKind::Valleys:           return "Valleys";
    case NodeKind::Plains:            return "Plains";
    case NodeKind::Plateaus:          return "Plateaus";
    case NodeKind::TerrainSynthesis:  return "TerrainSynthesis";
    }
    throw std::runtime_error("Unknown NodeKind");
}

NodeKind kindFromString(const std::string& s) {
    if (s == "Fbm")               return NodeKind::Fbm;
    if (s == "RidgedFbm")         return NodeKind::RidgedFbm;
    if (s == "Simplex")           return NodeKind::Simplex;
    if (s == "Perlin")            return NodeKind::Perlin;
    if (s == "Mountains")         return NodeKind::Mountains;
    if (s == "Valleys")           return NodeKind::Valleys;
    if (s == "Plains")            return NodeKind::Plains;
    if (s == "Plateaus")          return NodeKind::Plateaus;
    if (s == "TerrainSynthesis")  return NodeKind::TerrainSynthesis;
    throw std::runtime_error("Unknown NodeKind string: " + s);
}

// ---------- Params ↔ JSON ----------

static json noiseParamsToJson(const NoiseParams& p) {
    return {
        {"frequency",  p.frequency},
        {"octaves",    p.octaves},
        {"lacunarity", p.lacunarity},
        {"gain",       p.gain},
        {"sharpness",  p.sharpness},
        {"xOffset",    p.xOffset},
        {"zOffset",    p.zOffset},
    };
}

static NoiseParams noiseParamsFromJson(const json& j) {
    NoiseParams p;
    if (j.contains("frequency"))  p.frequency  = j["frequency"].get<float>();
    if (j.contains("octaves"))    p.octaves    = j["octaves"].get<int>();
    if (j.contains("lacunarity")) p.lacunarity = j["lacunarity"].get<float>();
    if (j.contains("gain"))       p.gain       = j["gain"].get<float>();
    if (j.contains("sharpness"))  p.sharpness  = j["sharpness"].get<float>();
    if (j.contains("xOffset"))    p.xOffset    = j["xOffset"].get<float>();
    if (j.contains("zOffset"))    p.zOffset    = j["zOffset"].get<float>();
    return p;
}

static json verticalScaleParamsToJson(float verticalScale) {
    return {{"verticalScale", verticalScale}};
}

static float verticalScaleFromJson(const json& j) {
    if (j.contains("verticalScale"))
        return j["verticalScale"].get<float>();
    return 80.0f;
}

static json paramsToJson(NodeKind kind, const NodeParams& params) {
    switch (kind) {
    case NodeKind::Fbm:
    case NodeKind::RidgedFbm:
        return noiseParamsToJson(std::get<NoiseParams>(params));
    case NodeKind::Mountains:
        return verticalScaleParamsToJson(std::get<MountainParams>(params).verticalScale);
    case NodeKind::Valleys:
        return verticalScaleParamsToJson(std::get<ValleyParams>(params).verticalScale);
    case NodeKind::Plains:
        return verticalScaleParamsToJson(std::get<PlainsParams>(params).verticalScale);
    case NodeKind::Plateaus:
        return verticalScaleParamsToJson(std::get<PlateauParams>(params).verticalScale);
    case NodeKind::TerrainSynthesis:
        return verticalScaleParamsToJson(std::get<TerrainSynthesisParams>(params).verticalScale);
    }
    return json::object();
}

static NodeParams paramsFromJson(NodeKind kind, const json& j) {
    switch (kind) {
    case NodeKind::Fbm:
    case NodeKind::RidgedFbm:
        return noiseParamsFromJson(j);
    case NodeKind::Mountains: {
        MountainParams p;
        p.verticalScale = verticalScaleFromJson(j);
        return p;
    }
    case NodeKind::Valleys: {
        ValleyParams p;
        p.verticalScale = verticalScaleFromJson(j);
        return p;
    }
    case NodeKind::Plains: {
        PlainsParams p;
        p.verticalScale = verticalScaleFromJson(j);
        return p;
    }
    case NodeKind::Plateaus: {
        PlateauParams p;
        p.verticalScale = verticalScaleFromJson(j);
        return p;
    }
    case NodeKind::TerrainSynthesis: {
        TerrainSynthesisParams p;
        p.verticalScale = verticalScaleFromJson(j);
        return p;
    }
    }
    return NoiseParams{};
}

// ---------- Serialize ----------

std::string toJson(const EditorGraph& g) {
    json j;

    json nodesArr = json::array();
    for (const auto& node : g.nodes) {
        json nj;
        nj["id"]    = node.id;
        nj["kind"]  = kindToString(node.kind);
        nj["pos"]   = {node.posX, node.posY};
        nj["params"] = paramsToJson(node.kind, node.params);
        nodesArr.push_back(nj);
    }
    j["nodes"] = nodesArr;

    json linksArr = json::array();
    for (const auto& link : g.links) {
        json lj;
        lj["id"]   = link.id;
        lj["from"] = {{"nodeId", link.from.nodeId}, {"slot", link.from.slot}};
        lj["to"]   = {{"nodeId", link.to.nodeId},   {"slot", link.to.slot}};
        linksArr.push_back(lj);
    }
    j["links"] = linksArr;

    return j.dump(2);
}

// ---------- Deserialize ----------

EditorGraph fromJson(const std::string& text) {
    json j = json::parse(text);

    EditorGraph g;

    for (const auto& nj : j.at("nodes")) {
        EditorNode node;
        node.id    = nj.at("id").get<NodeId>();
        node.kind  = kindFromString(nj.at("kind").get<std::string>());
        if (nj.contains("pos") && nj["pos"].is_array() && nj["pos"].size() == 2) {
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
        link.id        = lj.at("id").get<LinkId>();
        link.from.nodeId = lj["from"].at("nodeId").get<NodeId>();
        link.from.slot   = lj["from"].at("slot").get<uint8_t>();
        link.to.nodeId   = lj["to"].at("nodeId").get<NodeId>();
        link.to.slot     = lj["to"].at("slot").get<uint8_t>();
        g.links.push_back(link);
    }

    return g;
}

} // namespace graph
