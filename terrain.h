#ifndef TERRAIN_H
#define TERRAIN_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace terrain {

struct NoiseSettings {
  float frequency = 0.05f;
  int octaves = 5;
  float heightScale = 16.0f;
  float lacunarity = 2.0f;
  float gain = 0.5f;
};

class ScalarField3D {
 public:
  ScalarField3D(int width, int height, int depth);

  float get(int x, int y, int z) const;
  void set(int x, int y, int z, float value);
  int width() const { return width_; }
  int height() const { return height_; }
  int depth() const { return depth_; }

 private:
  int width_;
  int height_;
  int depth_;
  std::vector<float> data_;

  size_t indexOf(int x, int y, int z) const;
};

void configureNoise(const NoiseSettings& settings, uint32_t seed);
float perlinNoise(float x, float y, float z);
float fbm(float x, float y, float z);
float density(float x, float y, float z);
void populateTerrain(ScalarField3D& field, float yScale = 1.0f);

}  // namespace terrain

#endif  // TERRAIN_H
