cmake_minimum_required(VERSION 3.20)
project(raymarcher C CXX)

set(CMAKE_CXX_STANDARD 20) # C++20: inicializadores designados nos presets
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # para o autocomplete do editor (clangd)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# GLFW: janela, contexto OpenGL e entrada (teclado/mouse). Vem do sistema.
find_package(glfw3 3.3 REQUIRED)

# glad: carregador das funções do OpenGL 3.3 core (gerado e commitado em external/).
add_library(glad STATIC external/glad/src/gl.c)
target_include_directories(glad PUBLIC external/glad/include)

# Dear ImGui: interface gráfica em "modo imediato". Baixado na configuração do CMake;
# compilamos o núcleo + os backends de GLFW (entrada) e OpenGL 3 (desenho).
include(FetchContent)
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.9b
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui)
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)

add_executable(raymarcher src/main.cpp src/shader.cpp src/scene.cpp)
target_link_libraries(raymarcher PRIVATE glad glfw imgui)
target_compile_options(raymarcher PRIVATE -Wall -Wextra)
# Shaders são lidos da pasta-fonte em tempo de execução, o que permite recarregar com R.
target_compile_definitions(raymarcher PRIVATE SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders/")
