#include <cstring>
#include <iostream>

namespace renderer {
void runDemo();
void exportImages();
}

namespace graph {
void run();
}

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "view";

    if (std::strcmp(mode, "graph") == 0) {
        graph::run();
    } else if (std::strcmp(mode, "view") == 0) {
        renderer::runDemo();
    } else if (std::strcmp(mode, "export") == 0) {
        renderer::exportImages();
    } else {
        std::cerr << "Usage: terrain_demo [graph|view|export]\n";
        return 1;
    }

    return 0;
}
