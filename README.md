Build

Linux/macOS:
```bash
./build.sh
./build/terrain_demo
```

Windows (x64):
```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\terrain_demo.exe
```

`build.sh` uses CMake (`build/` directory), so repeated builds are incremental.
