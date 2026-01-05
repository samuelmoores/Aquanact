#include <Engine.h>
#include <stdexcept>

Window* Engine::Window = nullptr;
Renderer* Engine::Renderer = nullptr;
Camera* Engine::Camera = nullptr;
UI* Engine::UI = nullptr;

Engine::Engine()
{
    Window = new ::Window();
    Camera = new ::Camera();
    Renderer = new ::Renderer();
    UI = new ::UI();
}