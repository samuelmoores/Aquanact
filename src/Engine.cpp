#include <Engine.h>
#include <stdexcept>

Window* Engine::Window = nullptr;
Renderer* Engine::Renderer = nullptr;
Camera* Engine::Camera = nullptr;
UI* Engine::UI = nullptr;
Level* Engine::Level = nullptr;
float Engine::m_deltaFrameTime = 0.0f;
float Engine::m_timeElapsed = 0.0f;
std::chrono::steady_clock::time_point Engine::m_prevFrameTime = std::chrono::high_resolution_clock::now();

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

float Engine::DeltaFrameTime()
{
    auto currTime = std::chrono::high_resolution_clock::now();
    auto diffTime = std::chrono::duration<float>(currTime - m_prevFrameTime);
    float diffTimeSec = diffTime.count();
    m_prevFrameTime = currTime;
    return diffTimeSec;
}

float Engine::TimeElapsed()
{
    return m_timeElapsed;
}

void Engine::Tick()
{
    m_timeElapsed += DeltaFrameTime();
}
