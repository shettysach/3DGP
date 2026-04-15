#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

struct Field {
    int width, depth;
    std::vector<float> data;

    float& at(int x, int z) {
        return data[z * width + x];
    }

    float at(int x, int z) const {
        return data[z * width + x];
    }
};

struct Context {
    Field height;
    Field river;
    Field delta;
    Field slope;
    Field waterDist;
};

// Base class
class ConstraintNode {
public:
    virtual void apply(Context& ctx) = 0;
    virtual ~ConstraintNode() {}
};
inline float safeAt(const Field& f, int x, int z) {
    x = std::max(0, std::min(f.width - 1, x));
    z = std::max(0, std::min(f.depth - 1, z));
    return f.at(x,z);
}
inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}

inline float computeSlope(const Field& h, int x, int z) {
    float dx = safeAt(h, x+1,z) - safeAt(h, x-1,z);
    float dz = safeAt(h, x,z+1) - safeAt(h, x,z-1);
    return std::sqrt(dx*dx + dz*dz);
}

inline float neighborAvg(const Field& h, int x, int z) {
    return (
        safeAt(h, x+1,z) +
        safeAt(h, x-1,z) +
        safeAt(h, x,z+1) +
        safeAt(h, x,z-1)
    ) * 0.25f;
}

class RiverNode : public ConstraintNode {
public:
    float weight = 4.0f;
    float depth = 8.0f;

    void apply(Context& ctx) override {
        for (int z = 1; z < ctx.height.depth - 1; z++) {
            for (int x = 1; x < ctx.height.width - 1; x++) {

                if (ctx.river.at(x,z) > 0.5f) {

                    float lowest = std::min({
                        ctx.height.at(x+1,z),
                        ctx.height.at(x-1,z),
                        ctx.height.at(x,z+1),
                        ctx.height.at(x,z-1)
                    });

                    float target = lowest - depth;

                    ctx.delta.at(x,z) += 
                        weight * (target - ctx.height.at(x,z));
                }
            }
        }
    }
};

class SmoothNode : public ConstraintNode {
public:
    float weight = 0.2f;

    void apply(Context& ctx) override {
        for (int z = 1; z < ctx.height.depth - 1; z++) {
            for (int x = 1; x < ctx.height.width - 1; x++) {

                float avg = neighborAvg(ctx.height, x, z);

                ctx.delta.at(x,z) += 
                    weight * (avg - ctx.height.at(x,z));
            }
        }
    }
};

class SlopeNode : public ConstraintNode {
public:
    void apply(Context& ctx) override {
        for (int z = 1; z < ctx.height.depth - 1; z++) {
            for (int x = 1; x < ctx.height.width - 1; x++) {

                ctx.slope.at(x,z) = computeSlope(ctx.height, x, z);
            }
        }
    }
};

class DistanceToWaterNode : public ConstraintNode {
public:
    void apply(Context& ctx) override {
        for (int z = 0; z < ctx.height.depth; z++) {
            for (int x = 0; x < ctx.height.width; x++) {

                float minDist = 9999.0f;

                for (int zz = 0; zz < ctx.height.depth; zz++) {
                    for (int xx = 0; xx < ctx.height.width; xx++) {

                        if (ctx.river.at(xx,zz) > 0.5f) {
                            float dx = float(x - xx);
                            float dz = float(z - zz);
                            float dist = std::sqrt(dx*dx + dz*dz);

                            if (dist < minDist)
                                minDist = dist;
                        }
                    }
                }

                ctx.waterDist.at(x,z) = minDist;
            }
        }
    }
};

class SettlementNode : public ConstraintNode {
public:
    float weight = 0.3f;

    void apply(Context& ctx) override {
        for (int z = 1; z < ctx.height.depth - 1; z++) {
            for (int x = 1; x < ctx.height.width - 1; x++) {

                float slope = ctx.slope.at(x,z);
                float dist  = ctx.waterDist.at(x,z);

                // suitability: flat + near water
                float suitability =
                    std::exp(-dist * 0.1f) *
                    std::exp(-slope * 5.0f);

                float avg = neighborAvg(ctx.height, x, z);

                ctx.delta.at(x,z) += 
                    weight * suitability * (avg - ctx.height.at(x,z));
            }
        }
    }
};

