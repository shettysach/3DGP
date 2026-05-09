Build & Run

Linux/macOS:
```bash
./build.sh
./build/terrain_demo graph # for graph view
./build/terrain_demo view  # for render view
```

Windows (x64):
```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\terrain_demo.exe
.\build\Release\terrain_demo.exe graph # for graph view
.\build\Release\terrain_demo.exe view  # for render view
```

`build.sh` uses CMake (`build/` directory), so repeated builds are incremental.

Dependencies

- **CMake** (3.16+)
- **SDL2**
- **OpenGL** (GL + GLU)
- **C++ compiler** (GCC/Clang with C++17 support)
- **pkg-config** (Linux only)
- **imgui**, **imnodes**, **nlohmann_json** — fetched automatically by CMake on Linux/macOS
