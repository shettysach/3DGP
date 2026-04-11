{
  description = "Terrain demo with OpenGL";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = {
    self,
    nixpkgs,
  }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs {inherit system;};
  in {
    devShells.${system}.default = pkgs.mkShell {
      packages = with pkgs; [
        # Python and package management
        pkgs.python312
        pkgs.uv
        pkgs.ruff
        pkgs.ty

        # C++
        pkgs.libgcc
        pkgs.cmake
        pkgs.pkg-config
        pkgs.clang-tools

        # OpenGL and windowing (cross-platform)
        pkgs.libGL
        pkgs.libGLU
        pkgs.SDL2
        pkgs.libX11
        pkgs.libXext

        # Utilities
        pkgs.glib
        pkgs.libjpeg
        pkgs.libpng
        pkgs.scons
      ];

      shellHook = ''
        # Python environment
        unset PYTHONPATH
        export UV_PYTHON_DOWNLOADS=never

        # OpenGL + SDL2 library paths
        export LD_LIBRARY_PATH=${pkgs.lib.makeLibraryPath [
          pkgs.libGL
          pkgs.libGLU
          pkgs.SDL2
        ]}:$LD_LIBRARY_PATH
      '';
    };

    formatter.${system} = pkgs.alejandra;
  };
}
