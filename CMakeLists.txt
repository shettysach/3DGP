cmake_minimum_required(VERSION 3.16)
project(TerrainDemo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

if(WIN32)
  find_package(SDL2 REQUIRED)
  find_package(OpenGL REQUIRED)
  include_directories(${SDL2_INCLUDE_DIRS})
  set(SDL2_LIBRARIES ${SDL2_LIBRARIES})
  set(OPENGL_LIBRARIES ${OPENGL_LIBRARIES})
elseif(APPLE)
    find_package(SDL2 REQUIRED)
    find_package(OpenGL REQUIRED)

    include_directories(${SDL2_INCLUDE_DIRS})

    set(SDL2_LIBS ${SDL2_LIBRARIES})
    set(OPENGL_LIBS OpenGL::GL)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED sdl2)
  pkg_check_modules(GL REQUIRED gl)
  pkg_check_modules(GLU REQUIRED glu)
  include_directories(${SDL2_INCLUDE_DIRS} ${GL_INCLUDE_DIRS} ${GLU_INCLUDE_DIRS})
  set(SDL2_LIBRARIES ${SDL2_LIBRARIES})
  set(OPENGL_LIBRARIES ${GL_LIBRARIES} ${GLU_LIBRARIES})
endif()

add_executable(terrain_demo
    src/main.cpp
    src/terrain.cpp
    src/terrain.h
    src/terrain/plains.cpp
    src/terrain/plains.h
    src/terrain/plateaus.cpp
    src/terrain/plateaus.h
    src/terrain/mountains.cpp
    src/terrain/mountains.h
    src/terrain/valleys.cpp
    src/terrain/valleys.h
    src/terrain/blending.cpp
    src/terrain/blending.h
    src/terrain/biomes.cpp
    src/terrain/biomes.h
    src/terrain/climate.cpp
    src/terrain/climate.h
    src/terrain/fields.h
    src/terrain/landforms.cpp
    src/terrain/landforms.h
    src/terrain/util.h
    src/terrain/terrain_noise.cpp
    src/terrain/terrain_noise.h
    src/terrain/rivers.cpp
    src/terrain/rivers.h
    src/renderer/core.cpp
    src/renderer/core.h
    src/renderer/demo.cpp
    src/renderer/support.cpp
    src/renderer/internal.h
    src/renderer/shaders.h
 )

target_include_directories(terrain_demo PRIVATE src)

target_link_libraries(terrain_demo
    ${SDL2_LIBRARIES}
    ${OPENGL_LIBRARIES}
)
