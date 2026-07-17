
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
    "src/main.cpp"
    "include/ShaderProgram.h" "src/ShaderProgram.cpp"
    "include/StbImage.h" "src/StbImage.cpp"
    "include/Mesh.h" "src/Mesh.cpp"
    "include/ModelImporter.h" "src/ModelImporter.cpp"
    "include/Camera.h"
    "include/EngineCamera.h" "src/EngineCamera.cpp"
    "include/GameCamera.h" "src/GameCamera.cpp"
    "include/Renderer.h"
    "include/Object3D.h" "src/Object3D.cpp"
    "include/Axis.h" "src/Axis.cpp"
    "include/Grid.h" "src/Grid.cpp"
    "include/Texture.h"
    "include/Line.h" "src/Line.cpp"
    "include/Debug.h" "src/Debug.cpp"
    "include/GameGUIAsset.h"
    "include/EngineGUI.h" "src/EngineGUI.cpp"
    "include/GameGUI.h" "src/GameGUI.cpp"
    "include/GameGUICreator.h" "src/GameGUICreator.cpp"
    "include/FrontEndManager.h" "src/FrontEndManager.cpp"
    "include/FileManager.h" "src/FileManager.cpp"
    "include/FileSystem.h" "src/FileSystem.cpp"
    "include/SceneManager.h" "src/SceneManager.cpp"
    "include/ProjectManager.h" "src/ProjectManager.cpp"
    "include/Animation.h" "src/Animation.cpp"
    "include/Animator.h" "src/Animator.cpp"
    "include/AnimatorComponent.h" "src/AnimatorComponent.cpp"
    "include/Controller.h" "src/Controller.cpp"
    "include/Window.h" "src/Window.cpp"
    "include/Input.h" "src/Input.cpp"
    "include/GameplayManager.h" "src/GameplayManager.cpp"
    "include/RenderManager.h" "src/RenderManager.cpp"
    "include/GraphicsDevice.h"
    "include/OpenGLGraphicsDevice.h" "src/OpenGLGraphicsDevice.cpp"
    "include/OpenGLGraphics.h" "src/OpenGLGraphics.cpp"
    "include/Physics.h" "src/Physics.cpp"
    "include/MathUtils.h" "src/MathUtils.cpp"
    "include/Audio.h" "src/Audio.cpp"
    "include/RenderCommand.h"
    "include/FrameAllocator.h"
    "include/GLHeaders.h"
    "include/AquanactBuildSystem.h" "src/AquanactBuildSystem.cpp"
)

add_executable (AquanactEngine ${SOURCES})
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_EDITOR)
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
target_compile_definitions(AquanactEngine PRIVATE MYGUI_STATIC)
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_VCPKG_TARGET_TRIPLET="${AQUANACT_VCPKG_TARGET_TRIPLET}")
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_MYGUI_ENGINE_LIBRARY="${MYGUI_ENGINE_LIBRARY_DEBUG}")
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_MYGUI_OPENGL_LIBRARY="${MYGUI_OPENGL_LIBRARY_DEBUG}")
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_FREETYPE_LIBRARY="freetype")
target_compile_features(AquanactEngine PRIVATE cxx_std_20)

add_executable (AquanactGame WIN32 ${SOURCES})
target_compile_definitions(AquanactGame PRIVATE AQUANACT_GAME)
target_compile_definitions(AquanactGame PRIVATE AQUANACT_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
target_compile_definitions(AquanactGame PRIVATE MYGUI_STATIC)
target_compile_definitions(AquanactGame PRIVATE AQUANACT_VCPKG_TARGET_TRIPLET="${AQUANACT_VCPKG_TARGET_TRIPLET}")
target_compile_definitions(AquanactGame PRIVATE AQUANACT_MYGUI_ENGINE_LIBRARY="${MYGUI_ENGINE_LIBRARY_DEBUG}")
target_compile_definitions(AquanactGame PRIVATE AQUANACT_MYGUI_OPENGL_LIBRARY="${MYGUI_OPENGL_LIBRARY_DEBUG}")
target_compile_definitions(AquanactGame PRIVATE AQUANACT_FREETYPE_LIBRARY="freetype")
target_compile_features(AquanactGame PRIVATE cxx_std_20)

add_executable (AquanactBuildTool
    "src/BuildToolMain.cpp"
    "include/AquanactBuildSystem.h" "src/AquanactBuildSystem.cpp"
)
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


