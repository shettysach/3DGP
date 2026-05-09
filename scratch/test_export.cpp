#include "terrain.h"
#include "terrain/biomes.h"
#include <iostream>

int main() {
    terrain::TerrainSettings settings;
    settings.width = 128;
    settings.depth = 128;
    settings.exportImages = true;
    settings.seed = 42;

    terrain::TerrainGenerator generator(settings);
    try {
        generator.generateMesh();
        std::cout << "Successfully generated terrain and exported images." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
