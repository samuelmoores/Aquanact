
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
    "src/Engine/Core/main.cpp"
    "include/Engine/Core/ShaderProgram.h" "src/Engine/Core/ShaderProgram.cpp"
    "include/Engine/Core/StbImage.h" "src/Engine/Core/StbImage.cpp"
    "include/Engine/Core/Mesh.h" "src/Engine/Core/Mesh.cpp"
    "include/Engine/Core/ModelImporter.h" "src/Engine/Core/ModelImporter.cpp"
    "include/Engine/Core/Camera.h"
    "include/Engine/Core/CameraController.h"
    "include/Engine/Core/CameraManager.h" "src/Engine/Core/CameraManager.cpp"
    "include/Engine/Core/EngineCamera.h" "src/Engine/Core/EngineCamera.cpp"
    "include/Engine/Core/EditorCameraController.h" "src/Engine/Core/EditorCameraController.cpp"
    "include/Engine/Core/GameCamera.h" "src/Engine/Core/GameCamera.cpp"
    "include/Engine/Core/Renderer.h"
    "include/Engine/Core/Axis.h" "src/Engine/Core/Axis.cpp"
    "include/Engine/Core/Grid.h" "src/Engine/Core/Grid.cpp"
    "include/Engine/Core/Texture.h"
    "include/Engine/Core/Line.h" "src/Engine/Core/Line.cpp"
    "include/Engine/Core/Debug.h" "src/Engine/Core/Debug.cpp"
    "include/Engine/UI/GameGUIAsset.h"
    "include/Engine/UI/EngineGUI.h" "src/Engine/UI/EngineGUI.cpp"
    "include/Engine/UI/GameGUI.h" "src/Engine/UI/GameGUI.cpp"
    "include/Engine/UI/GameGUIManager.h" "src/Engine/UI/GameGUIManager.cpp"
    "include/Engine/UI/GameGUICreatorHelpers.h" "src/Engine/UI/GameGUICreatorHelpers.cpp"
    "include/Engine/UI/GameGUICreatorModel.h" "src/Engine/UI/GameGUICreatorModel.cpp"
    "include/Engine/UI/GameGUICreatorView.h" "src/Engine/UI/GameGUICreatorView.cpp"
    "include/Engine/UI/GameGUICreator.h" "src/Engine/UI/GameGUICreator.cpp"
    "src/Engine/UI/GameGUICreator_Persistence.cpp"
    "src/Engine/UI/GameGUICreator_WidgetOps.cpp"
    "src/Engine/UI/GameGUICreator_Panels.cpp"
    "src/Engine/UI/GameGUICreator_CreatePopup.cpp"
    "src/Engine/UI/GameGUICreator_BindingPopup.cpp"
    "src/Engine/UI/GameGUICreator_TexturePopup.cpp"
    "src/Engine/UI/GameGUICreator_PanelsList.cpp"
    "src/Engine/UI/GameGUICreator_PanelsDetails.cpp"
    "include/Engine/Core/FrontEndManager.h" "src/Engine/Core/FrontEndManager.cpp"
    "include/Engine/Core/FileManager.h" "src/Engine/Core/FileManager.cpp"
    "include/Engine/Core/FileSystem.h" "src/Engine/Core/FileSystem.cpp"
    "include/Engine/Core/Level.h" "src/Engine/Core/Level.cpp"
    "include/Engine/Core/LevelManager.h" "src/Engine/Core/LevelManager.cpp"
    "include/Engine/Core/ProjectManager.h" "src/Engine/Core/ProjectManager.cpp"
    "include/Engine/Core/ProjectStateData.h"
    "include/Engine/Core/ProjectStateFormat.h" "src/Engine/Core/ProjectStateFormat.cpp"
    "include/Engine/Core/ProjectStateSerializer.h" "src/Engine/Core/ProjectStateSerializer.cpp"
    "include/Engine/Core/Animation.h" "src/Engine/Core/Animation.cpp"
    "include/Engine/Core/Animator.h" "src/Engine/Core/Animator.cpp"
    "include/Engine/Core/AnimatorComponent.h" "src/Engine/Core/AnimatorComponent.cpp"
    "include/Engine/Core/Controller.h" "src/Engine/Core/Controller.cpp"
    "include/Engine/Core/PlayerController.h" "src/Engine/Core/PlayerController.cpp"
    "include/Engine/Core/Component.h" "src/Engine/Core/Component.cpp"
    "include/Engine/Core/Entity.h" "src/Engine/Core/Entity.cpp"
    "include/Engine/Core/EntityRegistry.h" "src/Engine/Core/EntityRegistry.cpp"
    "include/Engine/Core/EventManager.h" "src/Engine/Core/EventManager.cpp"
    "include/Game/Enemy.h" "src/Game/Enemy.cpp"
    "include/Game/PlayerHealth.h" "src/Game/PlayerHealth.cpp"
    "include/Engine/Core/Window.h" "src/Engine/Core/Window.cpp"
    "include/Engine/Core/Input.h" "src/Engine/Core/Input.cpp"
    "include/Engine/Core/GameplayManager.h" "src/Engine/Core/GameplayManager.cpp"
    "include/Engine/Core/SimulationManager.h" "src/Engine/Core/SimulationManager.cpp"
    "include/Engine/Core/Root.h" "src/Engine/Core/Root.cpp"
    "include/Engine/Core/RenderManager.h" "src/Engine/Core/RenderManager.cpp"
    "include/Engine/Core/GraphicsDevice.h"
    "include/Engine/Core/OpenGLGraphicsDevice.h" "src/Engine/Core/OpenGLGraphicsDevice.cpp"
    "include/Engine/Core/OpenGLGraphics.h" "src/Engine/Core/OpenGLGraphics.cpp"
    "include/Engine/Core/Physics.h" "src/Engine/Core/Physics.cpp"
    "include/Engine/Core/MathUtils.h" "src/Engine/Core/MathUtils.cpp"
    "include/Engine/Core/Audio.h" "src/Engine/Core/Audio.cpp"
    "include/Engine/Core/RenderCommand.h"
    "include/Engine/Core/FrameAllocator.h"
    "include/Engine/Core/GLHeaders.h"
    "include/Engine/Core/AquanactBuildSystem.h" "src/Engine/Core/AquanactBuildSystem.cpp"
)

add_executable (AquanactEngine ${SOURCES} "include/Engine/Core/LightingManager.h" "src/Engine/Core/LightingManager.cpp")
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_EDITOR)
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
target_compile_definitions(AquanactEngine PRIVATE MYGUI_STATIC)
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_VCPKG_TARGET_TRIPLET="${AQUANACT_VCPKG_TARGET_TRIPLET}")
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_MYGUI_ENGINE_LIBRARY="${MYGUI_ENGINE_LIBRARY_DEBUG}")
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_MYGUI_OPENGL_LIBRARY="${MYGUI_OPENGL_LIBRARY_DEBUG}")
target_compile_definitions(AquanactEngine PRIVATE AQUANACT_FREETYPE_LIBRARY="freetype")
target_compile_features(AquanactEngine PRIVATE cxx_std_20)

add_executable (AquanactGame WIN32 ${SOURCES} "include/Engine/Core/LightingManager.h" "src/Engine/Core/LightingManager.cpp")
target_compile_definitions(AquanactGame PRIVATE AQUANACT_GAME)
target_compile_definitions(AquanactGame PRIVATE AQUANACT_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
target_compile_definitions(AquanactGame PRIVATE MYGUI_STATIC)
target_compile_definitions(AquanactGame PRIVATE AQUANACT_VCPKG_TARGET_TRIPLET="${AQUANACT_VCPKG_TARGET_TRIPLET}")
target_compile_definitions(AquanactGame PRIVATE AQUANACT_MYGUI_ENGINE_LIBRARY="${MYGUI_ENGINE_LIBRARY_DEBUG}")
target_compile_definitions(AquanactGame PRIVATE AQUANACT_MYGUI_OPENGL_LIBRARY="${MYGUI_OPENGL_LIBRARY_DEBUG}")
target_compile_definitions(AquanactGame PRIVATE AQUANACT_FREETYPE_LIBRARY="freetype")
target_compile_features(AquanactGame PRIVATE cxx_std_20)

add_executable (AquanactBuildTool
    "src/Engine/Core/BuildToolMain.cpp"
    "include/Engine/Core/AquanactBuildSystem.h" "src/Engine/Core/AquanactBuildSystem.cpp")
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




