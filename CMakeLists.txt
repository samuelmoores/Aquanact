
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

add_executable (AquanactEngine "src/main.cpp" "include/ShaderProgram.h" "src/ShaderProgram.cpp" "include/StbImage.h" "src/StbImage.cpp" "include/Mesh.h" "src/Mesh.cpp"
"include/Camera.h" "src/Camera.cpp" "include/Renderer.h" "src/Renderer.cpp" "include/Object3D.h" "src/Object3D.cpp" "include/Axis.h" "src/Axis.cpp" "include/Texture.h" "include/Line.h" "src/Line.cpp" "include/Animation.h" "src/Animation.cpp" "include/Animator.h" "src/Animator.cpp" "include/Window.h" "src/Window.cpp" "include/Engine.h" "src/Engine.cpp" "include/UI.h" "src/UI.cpp")


# Find and link external libraries, like SFML.
# This only works if Vcpkg has been configured correctly.
find_package(SFML COMPONENTS system window graphics CONFIG REQUIRED)
target_link_libraries(AquanactEngine PRIVATE sfml-system sfml-network sfml-graphics sfml-window)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(AquanactEngine PRIVATE glfw)

find_package(assimp CONFIG REQUIRED)
target_link_libraries(AquanactEngine PRIVATE assimp::assimp)

find_package(glad CONFIG REQUIRED)
target_link_libraries(AquanactEngine PRIVATE glad::glad)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(AquanactEngine PRIVATE imgui::imgui)

target_include_directories(AquanactEngine PUBLIC "./include")


set_target_properties(AquanactEngine
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR})
# Configure CMake to copy the contents of the /shaders and /models directories to the 
# output (.exe) directory, so the application can reference those folders using relative paths
# instead of absolute paths.
add_custom_target(copyshaders
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders ${CMAKE_BINARY_DIR}/shaders
        COMMENT "copying ${CMAKE_SOURCE_DIR}/shaders to ${CMAKE_BINARY_DIR}/shaders"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_custom_target(copymodels
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/models
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/models ${CMAKE_BINARY_DIR}/models
        COMMENT "copying ${CMAKE_SOURCE_DIR}/models to ${CMAKE_CURRENT_BINARY_DIR}/models"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_dependencies(AquanactEngine copyshaders copymodels)


if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET AquanactEngine PROPERTY CXX_STANDARD 20)
endif()

