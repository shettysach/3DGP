cmake_minimum_required(VERSION 3.16)
project(TerrainDemo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
  find_package(SDL2 REQUIRED)
  find_package(OpenGL REQUIRED)
  include_directories(${SDL2_INCLUDE_DIRS})
  set(SDL2_LIBRARIES ${SDL2_LIBRARIES})
  set(OPENGL_LIBRARIES ${OPENGL_LIBRARIES})
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
    src/renderer.cpp
    src/renderer.h
)

target_link_libraries(terrain_demo
    ${SDL2_LIBRARIES}
    ${OPENGL_LIBRARIES}
)
