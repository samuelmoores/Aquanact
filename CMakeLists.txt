
# CMakeList.txt : CMake project for Graphics, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("AquanactEngine")

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
    "include/RenderCommand.h"
    "include/GLHeaders.h"
)

add_executable (AquanactEngine ${SOURCES})

if (EMSCRIPTEN)
    # ---- Web / WebGL 2 build ----
    set_target_properties(AquanactEngine PROPERTIES
        OUTPUT_NAME "index"
        SUFFIX ".html"
    )

    target_compile_options(AquanactEngine PRIVATE
        -O2
    )

    # Preload assets and shaders into the virtual FS
    set(EM_LINK_FLAGS
        "-s USE_WEBGL2=1"
        "-s FULL_ES3=1"
        "-s USE_GLFW=3"
        "-s ALLOW_MEMORY_GROWTH=1"
        "-s INITIAL_MEMORY=134217728"   # 128 MB
        "-s MAXIMUM_MEMORY=536870912"   # 512 MB
        "-s ENVIRONMENT=web"
        "--preload-file ${CMAKE_BINARY_DIR}/assets@/assets"
        "--preload-file ${CMAKE_BINARY_DIR}/shaders@/shaders"
        "--shell-file ${CMAKE_SOURCE_DIR}/web/shell.html"
        "--use-preload-cache"
        "-s WASM=1"
        "-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
    )
    string(JOIN " " EM_LINK_FLAGS_STR ${EM_LINK_FLAGS})
    set_target_properties(AquanactEngine PROPERTIES LINK_FLAGS "${EM_LINK_FLAGS_STR}")

    # Emscripten ships GLFW and OpenGL ES — no vcpkg packages needed for those.
    # We still need assimp compiled for Emscripten (use emcmake + vcpkg or pre-built).
    find_package(assimp CONFIG REQUIRED)
    target_link_libraries(AquanactEngine PRIVATE assimp::assimp)

else()
    # ---- Native desktop build ----
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
endif()

target_include_directories(AquanactEngine PUBLIC "./include")

if (NOT EMSCRIPTEN)
    set_target_properties(AquanactEngine
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR})
endif()

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
