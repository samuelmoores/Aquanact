
# CMakeList.txt : CMake project for Graphics, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8...3.30)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("AquanactEngine")

set(RMLUI_BACKEND_DIR "${CMAKE_SOURCE_DIR}/src/rmlui_backends")

set(SOURCES
    "src/main.cpp"
    "include/ShaderProgram.h" "src/ShaderProgram.cpp"
    "include/StbImage.h" "src/StbImage.cpp"
    "include/Mesh.h" "src/Mesh.cpp"
    "include/Camera.h" "src/Camera.cpp"
    "include/Renderer.h" "src/Renderer.cpp"
    "include/Object3D.h" "src/Object3D.cpp"
    "include/Axis.h" "src/Axis.cpp"
    "include/Texture.h"
    "include/Line.h" "src/Line.cpp"
    "include/Animation.h" "src/Animation.cpp"
    "include/Animator.h" "src/Animator.cpp"
    "include/Window.h" "src/Window.cpp"
    "include/Engine.h" "src/Engine.cpp"
    "include/UI.h" "src/UI.cpp"
    "include/Level.h" "src/Level.cpp"
    "include/Input.h" "src/Input.cpp"
    "include/PlayerController.h" "src/PlayerController.cpp"
    "include/Physics.h" "src/Physics.cpp"
    "include/MathUtils.h" "src/MathUtils.cpp"
    "include/Audio.h" "src/Audio.cpp"
    "include/ShadowMap.h" "src/ShadowMap.cpp"
    "include/MainMenu.h" "src/MainMenu.cpp"
    "include/RenderCommand.h"
    "include/GLHeaders.h"
)

add_executable (AquanactEngine ${SOURCES})

target_sources(AquanactEngine PRIVATE
    "${RMLUI_BACKEND_DIR}/RmlUi_Platform_GLFW.cpp"
    "${RMLUI_BACKEND_DIR}/RmlUi_Renderer_GL3.cpp"
)

find_package(SFML COMPONENTS system window graphics audio CONFIG REQUIRED)
target_link_libraries(AquanactEngine PRIVATE sfml-system sfml-network sfml-graphics sfml-window sfml-audio)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(AquanactEngine PRIVATE glfw)

find_package(assimp CONFIG REQUIRED)
target_link_libraries(AquanactEngine PRIVATE assimp::assimp)

find_package(glad CONFIG REQUIRED)
target_link_libraries(AquanactEngine PRIVATE glad::glad)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(AquanactEngine PRIVATE imgui::imgui)

find_package(RmlUi CONFIG REQUIRED)
target_link_libraries(AquanactEngine PRIVATE RmlCore RmlDebugger)

target_include_directories(AquanactEngine PUBLIC "./include" "${RMLUI_BACKEND_DIR}")

# Use our existing glad instead of RmlUi's bundled GLAD loader
target_compile_definitions(AquanactEngine PRIVATE "RMLUI_GL3_CUSTOM_LOADER=<glad/glad.h>")

set_target_properties(AquanactEngine
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR})

# Copy shaders and assets to the build output directory
add_custom_target(copyshaders
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders ${CMAKE_BINARY_DIR}/shaders
        COMMENT "syncing ${CMAKE_SOURCE_DIR}/shaders to ${CMAKE_BINARY_DIR}/shaders"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_custom_target(copymodels
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets
        COMMENT "syncing ${CMAKE_SOURCE_DIR}/assets to ${CMAKE_BINARY_DIR}/assets"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_dependencies(AquanactEngine copyshaders copymodels)


if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET AquanactEngine PROPERTY CXX_STANDARD 20)
endif()
