#include <Engine.h>
#include <stdexcept>

Window* Engine::Window = nullptr;
Renderer* Engine::Renderer = nullptr;
Camera* Engine::Camera = nullptr;
UI* Engine::UI = nullptr;
Level* Engine::Level = nullptr;

Engine::Engine()
{
    Window = new ::Window();
    Camera = new ::Camera();
    Renderer = new ::Renderer();
    UI = new ::UI();
    Level = new ::Level();
    gladLoadGL();
    glEnable(GL_DEPTH_TEST);
    Level->Load();
}

bool Engine::Running()
{
    return !glfwWindowShouldClose(Window->GLFW());
}