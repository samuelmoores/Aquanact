
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

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    # Match the MSVC runtime used by the vcpkg binaries so debug/release and iterator
    # settings line up with the external libraries we link against.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Record the vcpkg triplet so the build can log which package target it was compiled against.
set(AQUANACT_VCPKG_TARGET_TRIPLET "${VCPKG_TARGET_TRIPLET}")

set(SOURCES
    "src/Engine/main.cpp"
    "include/Engine/ShaderProgram.h" "src/Engine/ShaderProgram.cpp"
    "include/Engine/StbImage.h" "src/Engine/StbImage.cpp"
    "include/Engine/Mesh.h" "src/Engine/Mesh.cpp"
    "include/Engine/ModelImporter.h" "src/Engine/ModelImporter.cpp"
    "include/Engine/Camera.h"
    "include/Engine/CameraController.h"
    "include/Engine/CameraManager.h" "src/Engine/CameraManager.cpp"
    "include/Engine/EngineCamera.h" "src/Engine/EngineCamera.cpp"
    "include/Engine/EditorCameraController.h" "src/Engine/EditorCameraController.cpp"
    "include/Engine/GameCamera.h" "src/Engine/GameCamera.cpp"
    "include/Engine/Renderer.h"
    "include/Engine/Axis.h" "src/Engine/Axis.cpp"
    "include/Engine/Grid.h" "src/Engine/Grid.cpp"
    "include/Engine/Texture.h"
    "include/Engine/Line.h" "src/Engine/Line.cpp"
    "include/Engine/Debug.h" "src/Engine/Debug.cpp"
    "include/Engine/GameGUIAsset.h"
    "include/Engine/EngineGUI.h" "src/Engine/EngineGUI.cpp"
    "include/Engine/GameGUI.h" "src/Engine/GameGUI.cpp"
    "include/Engine/GameGUIManager.h" "src/Engine/GameGUIManager.cpp"
    "include/Engine/GameGUICreator.h" "src/Engine/GameGUICreator.cpp"
    "include/Engine/FrontEndManager.h" "src/Engine/FrontEndManager.cpp"
    "include/Engine/FileManager.h" "src/Engine/FileManager.cpp"
    "include/Engine/FileSystem.h" "src/Engine/FileSystem.cpp"
    "include/Engine/Level.h" "src/Engine/Level.cpp"
    "include/Engine/LevelManager.h" "src/Engine/LevelManager.cpp"
    "include/Engine/ProjectManager.h" "src/Engine/ProjectManager.cpp"
    "include/Engine/Animation.h" "src/Engine/Animation.cpp"
    "include/Engine/Animator.h" "src/Engine/Animator.cpp"
    "include/Engine/AnimatorComponent.h" "src/Engine/AnimatorComponent.cpp"
    "include/Engine/Controller.h" "src/Engine/Controller.cpp"
    "include/Engine/PlayerController.h" "src/Engine/PlayerController.cpp"
    "include/Engine/Component.h" "src/Engine/Component.cpp"
    "include/Engine/Entity.h" "src/Engine/Entity.cpp"
    "include/Engine/EntityRegistry.h" "src/Engine/EntityRegistry.cpp"
    "include/Engine/EventManager.h" "src/Engine/EventManager.cpp"
    "include/Game/Enemy.h" "src/Game/Enemy.cpp"
    "include/Game/PlayerHealth.h" "src/Game/PlayerHealth.cpp"
    "include/Engine/Window.h" "src/Engine/Window.cpp"
    "include/Engine/Input.h" "src/Engine/Input.cpp"
    "include/Engine/GameplayManager.h" "src/Engine/GameplayManager.cpp"
    "include/Engine/RenderManager.h" "src/Engine/RenderManager.cpp"
    "include/Engine/GraphicsDevice.h"
    "include/Engine/OpenGLGraphicsDevice.h" "src/Engine/OpenGLGraphicsDevice.cpp"
    "include/Engine/OpenGLGraphics.h" "src/Engine/OpenGLGraphics.cpp"
    "include/Engine/Physics.h" "src/Engine/Physics.cpp"
    "include/Engine/MathUtils.h" "src/Engine/MathUtils.cpp"
    "include/Engine/Audio.h" "src/Engine/Audio.cpp"
    "include/Engine/RenderCommand.h"
    "include/Engine/FrameAllocator.h"
    "include/Engine/GLHeaders.h"
    "include/Engine/AquanactBuildSystem.h" "src/Engine/AquanactBuildSystem.cpp"
)

add_executable (AquanactEngine ${SOURCES} "include/Engine/LightingManager.h" "src/Engine/LightingManager.cpp")
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_EDITOR)
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
target_compile_definitions(AquanactEngine PRIVATE MYGUI_STATIC)
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_VCPKG_TARGET_TRIPLET="${AQUANACT_VCPKG_TARGET_TRIPLET}")
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_MYGUI_ENGINE_LIBRARY="${MYGUI_ENGINE_LIBRARY_DEBUG}")
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_MYGUI_OPENGL_LIBRARY="${MYGUI_OPENGL_LIBRARY_DEBUG}")
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_FREETYPE_LIBRARY="freetype")
target_compile_features(AquanactEngine PRIVATE cxx_std_20)

add_executable (AquanactGame WIN32 ${SOURCES} "include/Engine/LightingManager.h" "src/Engine/LightingManager.cpp")
target_compile_definitions(AquanactGame PRIVATE AQUANACT_GAME)
target_compile_definitions(AquanactGame PRIVATE AQUANACT_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
target_compile_definitions(AquanactGame PRIVATE MYGUI_STATIC)
target_compile_definitions(AquanactGame PRIVATE AQUANACT_VCPKG_TARGET_TRIPLET="${AQUANACT_VCPKG_TARGET_TRIPLET}")
target_compile_definitions(AquanactGame PRIVATE AQUANACT_MYGUI_ENGINE_LIBRARY="${MYGUI_ENGINE_LIBRARY_DEBUG}")
target_compile_definitions(AquanactGame PRIVATE AQUANACT_MYGUI_OPENGL_LIBRARY="${MYGUI_OPENGL_LIBRARY_DEBUG}")
target_compile_definitions(AquanactGame PRIVATE AQUANACT_FREETYPE_LIBRARY="freetype")
target_compile_features(AquanactGame PRIVATE cxx_std_20)

add_executable (AquanactBuildTool
    "src/Engine/BuildToolMain.cpp"
    "include/Engine/AquanactBuildSystem.h" "src/Engine/AquanactBuildSystem.cpp")
target_compile_definitions(AquanactBuildTool PRIVATE AQUANACT_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

if (MSVC)
    target_link_options(AquanactEngine PRIVATE
        "/LIBPATH:C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/lib/x64"
        "/LIBPATH:C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/ATLMFC/lib/x64"
        "/LIBPATH:C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/ucrt/x64"
        "/LIBPATH:C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64"
    )
    target_link_options(AquanactGame PRIVATE
        "/LIBPATH:C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/lib/x64"
        "/LIBPATH:C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/ATLMFC/lib/x64"
        "/LIBPATH:C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/ucrt/x64"
        "/LIBPATH:C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64"
    )
    target_link_options(AquanactBuildTool PRIVATE
        "/LIBPATH:C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/lib/x64"
        "/LIBPATH:C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/ATLMFC/lib/x64"
        "/LIBPATH:C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/ucrt/x64"
        "/LIBPATH:C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64"
    )
endif()

find_package(SFML COMPONENTS system window graphics audio CONFIG REQUIRED)
target_link_libraries(AquanactEngine PRIVATE sfml-system sfml-network sfml-graphics sfml-window sfml-audio)
target_link_libraries(AquanactEngine PRIVATE Psapi)
target_link_libraries(AquanactGame PRIVATE sfml-system sfml-network sfml-graphics sfml-window sfml-audio)
target_link_libraries(AquanactGame PRIVATE Psapi)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(AquanactEngine PRIVATE glfw)
target_link_libraries(AquanactGame PRIVATE glfw)

find_package(assimp CONFIG REQUIRED)
target_link_libraries(AquanactEngine PRIVATE assimp::assimp)
target_link_libraries(AquanactGame PRIVATE assimp::assimp)

find_package(glad CONFIG REQUIRED)
target_link_libraries(AquanactEngine PRIVATE glad::glad)
target_link_libraries(AquanactGame PRIVATE glad::glad)

find_package(OpenGL REQUIRED)
target_link_libraries(AquanactEngine PRIVATE OpenGL::GL)
target_link_libraries(AquanactGame PRIVATE OpenGL::GL)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(AquanactEngine PRIVATE imgui::imgui)
target_link_libraries(AquanactGame PRIVATE imgui::imgui)

# MyGUI's vcpkg package did not provide a simple CONFIG target in this setup, so we
# locate the headers and libraries explicitly and choose the debug or release variant
# ourselves.
find_path(MYGUI_INCLUDE_DIR
    NAMES MyGUI.h
    PATH_SUFFIXES MYGUI
    REQUIRED
)

find_library(MYGUI_ENGINE_LIBRARY_DEBUG
    NAMES MyGUIEngineStatic_d
    REQUIRED
)
find_library(MYGUI_ENGINE_LIBRARY_RELEASE
    NAMES MyGUIEngineStatic
    REQUIRED
)

find_library(MYGUI_OPENGL_LIBRARY_DEBUG
    NAMES MyGUI.OpenGLPlatform_d
    REQUIRED
)
find_library(MYGUI_OPENGL_LIBRARY_RELEASE
    NAMES MyGUI.OpenGLPlatform
    REQUIRED
)

find_package(freetype CONFIG REQUIRED)

target_include_directories(AquanactEngine PRIVATE ${MYGUI_INCLUDE_DIR})
target_include_directories(AquanactGame PRIVATE ${MYGUI_INCLUDE_DIR})
target_link_libraries(AquanactEngine PRIVATE
    debug ${MYGUI_ENGINE_LIBRARY_DEBUG} optimized ${MYGUI_ENGINE_LIBRARY_RELEASE}
    debug ${MYGUI_OPENGL_LIBRARY_DEBUG} optimized ${MYGUI_OPENGL_LIBRARY_RELEASE}
    freetype
)
target_link_libraries(AquanactGame PRIVATE
    debug ${MYGUI_ENGINE_LIBRARY_DEBUG} optimized ${MYGUI_ENGINE_LIBRARY_RELEASE}
    debug ${MYGUI_OPENGL_LIBRARY_DEBUG} optimized ${MYGUI_OPENGL_LIBRARY_RELEASE}
    freetype
)

target_include_directories(AquanactEngine PUBLIC "./include")
target_include_directories(AquanactGame PUBLIC "./include")
target_include_directories(AquanactBuildTool PUBLIC "./include")
target_compile_features(AquanactBuildTool PRIVATE cxx_std_20)

set_target_properties(AquanactEngine
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(AquanactGame
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
add_custom_target(copymygui
        # MyGUI's skins, layouts, fonts, and XML descriptors must be present next to
        # the executable, otherwise startup succeeds but widget/resource loading fails.
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/MyGUI_Media
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/external/mygui-upstream/Media/MyGUI_Media ${CMAKE_BINARY_DIR}
        COMMENT "syncing MyGUI media to ${CMAKE_BINARY_DIR}"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_dependencies(AquanactEngine copyshaders copymodels)
add_dependencies(AquanactEngine copymygui)
add_dependencies(AquanactGame copyshaders copymodels copymygui)



