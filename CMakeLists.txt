cmake_minimum_required(VERSION 3.16)
project(TerrainDemo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

if(WIN32)
  find_package(SDL2 REQUIRED)
  find_package(OpenGL REQUIRED)
  find_package(nlohmann_json CONFIG REQUIRED)
  include_directories(${SDL2_INCLUDE_DIRS})
  set(SDL2_LIBRARIES ${SDL2_LIBRARIES})
  set(OPENGL_LIBRARIES ${OPENGL_LIBRARIES})
  set(GRAPH_VIEW_SRC src/graph/graph_view_stub.cpp)
  set(THIRD_PARTY_LIBS nlohmann_json::nlohmann_json)
elseif(APPLE)
  find_package(SDL2 REQUIRED)
  find_package(OpenGL REQUIRED)

  include(FetchContent)
  if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
  endif()

  # Fetch imgui
  FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.4)
  FetchContent_Populate(imgui)
  set(IMGUI_SRC_DIR ${imgui_SOURCE_DIR})
  add_library(imgui STATIC
    ${IMGUI_SRC_DIR}/imgui.cpp
    ${IMGUI_SRC_DIR}/imgui_draw.cpp
    ${IMGUI_SRC_DIR}/imgui_tables.cpp
    ${IMGUI_SRC_DIR}/imgui_widgets.cpp
    ${IMGUI_SRC_DIR}/imgui_demo.cpp
  )
  target_include_directories(imgui PUBLIC ${IMGUI_SRC_DIR})
  add_library(imgui::imgui ALIAS imgui)

  # Fetch imnodes
  FetchContent_Declare(imnodes
    GIT_REPOSITORY https://github.com/Nelarius/imnodes.git
    GIT_TAG master)
  FetchContent_Populate(imnodes)
  add_library(imnodes STATIC
    ${imnodes_SOURCE_DIR}/imnodes.cpp
  )
  target_include_directories(imnodes PUBLIC ${imnodes_SOURCE_DIR})
  target_link_libraries(imnodes PUBLIC imgui)
  add_library(imnodes::imnodes ALIAS imnodes)

  # Fetch nlohmann_json
  FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3)
  FetchContent_MakeAvailable(nlohmann_json)

  include_directories(${SDL2_INCLUDE_DIRS})
  set(SDL2_LIBRARIES ${SDL2_LIBRARIES})
  set(OPENGL_LIBRARIES OpenGL::GL)
  set(THIRD_PARTY_LIBS imgui::imgui imnodes::imnodes nlohmann_json::nlohmann_json)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED sdl2)
  pkg_check_modules(GL REQUIRED gl)
  pkg_check_modules(GLU REQUIRED glu)
  include_directories(${SDL2_INCLUDE_DIRS} ${GL_INCLUDE_DIRS} ${GLU_INCLUDE_DIRS})
  set(SDL2_LIBRARIES ${SDL2_LIBRARIES})
  set(OPENGL_LIBRARIES ${GL_LIBRARIES} ${GLU_LIBRARIES})

  find_package(imgui CONFIG REQUIRED)
  find_package(imnodes CONFIG REQUIRED)
  find_package(nlohmann_json CONFIG REQUIRED)

  set(IMGUI_SRC_DIR "$ENV{IMGUI_SRC}")
  if(NOT IMGUI_SRC_DIR)
    message(FATAL_ERROR "IMGUI_SRC environment variable not set. Run nix develop first.")
  endif()
  set(THIRD_PARTY_LIBS imgui::imgui imnodes::imnodes nlohmann_json::nlohmann_json)
endif()

if(NOT GRAPH_VIEW_SRC)
  set(GRAPH_VIEW_SRC src/graph/graph_view.cpp)
endif()

set(IMGUI_BACKEND_SOURCES)
set(IMGUI_BACKEND_INCLUDES)
if(IMGUI_SRC_DIR)
  list(APPEND IMGUI_BACKEND_SOURCES
    ${IMGUI_SRC_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_SRC_DIR}/backends/imgui_impl_opengl3.cpp
  )
  list(APPEND IMGUI_BACKEND_INCLUDES ${IMGUI_SRC_DIR}/backends)
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
  src/terrain/fields.h
  src/terrain/landforms.cpp
  src/terrain/landforms.h
  src/terrain/util.h
  src/terrain/terrain_noise.cpp
  src/terrain/terrain_noise.h
  src/terrain/rivers.cpp
  src/terrain/rivers.h
  src/terrain/voronoi.cpp
  src/terrain/voronoi.h
  src/terrain/wfc.cpp
  src/terrain/wfc.h
  src/renderer/core.cpp
  src/renderer/core.h
  src/renderer/demo.cpp
  src/renderer/support.cpp
  src/renderer/internal.h
  src/renderer/shaders.h
  ${GRAPH_VIEW_SRC}
  src/graph/graph.cpp
  src/graph/graph_compile.cpp
  src/graph/graph_execute.cpp
  src/graph/graph_serialize.cpp
  ${IMGUI_BACKEND_SOURCES}
)

target_include_directories(terrain_demo PRIVATE src ${IMGUI_BACKEND_INCLUDES})

target_link_libraries(terrain_demo
  ${SDL2_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${THIRD_PARTY_LIBS}
)
