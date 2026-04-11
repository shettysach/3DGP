#include "terrain.h"

#include <iomanip>
#include <iostream>

int main()
{
    terrain::NoiseSettings settings;
    settings.frequency = 0.06f;
    settings.octaves = 4;
    settings.heightScale = 10.0f;
    settings.lacunarity = 2.0f;
    settings.gain = 0.5f;
    terrain::configureNoise(settings, 2026u);

    terrain::ScalarField3D field(8, 8, 4);
    terrain::populateTerrain(field, 1.0f);

    // Temp, will render using OpenGL later
    for (int z = 0; z < field.depth(); ++z)
    {
        std::cout << "z = " << z << '\n';
        for (int y = 0; y < field.height(); ++y)
        {
            for (int x = 0; x < field.width(); ++x)
            {
                std::cout << std::setw(8) << std::fixed << std::setprecision(3)
                          << field.get(x, y, z) << ' ';
            }
            std::cout << '\n';
        }
        std::cout << '\n';
    }

    return 0;
}
