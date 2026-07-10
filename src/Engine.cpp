#include <Engine.h>
#include <stdexcept>

Window* Engine::Window = nullptr;
Renderer* Engine::Renderer = nullptr;
Camera* Engine::Camera = nullptr;
UI* Engine::UI = nullptr;
Input* Engine::Input = nullptr;
float Engine::m_deltaFrameTime = 0.0f;
float Engine::m_timeElapsed = 0.0f;
std::chrono::steady_clock::time_point Engine::m_prevFrameTime = std::chrono::high_resolution_clock::now();

Engine::Engine()
{
    Window = new ::Window();
    gladLoadGL();
    Camera = new ::Camera();
    Renderer = new ::Renderer();
    UI = new ::UI();
    Input = new ::Input();
    Renderer->Init();
    glfwSwapInterval(1);
    glEnable(GL_DEPTH_TEST);
}

bool Engine::Running()
{
    return !glfwWindowShouldClose(Window->GLFW());
}

float Engine::DeltaFrameTime()
{
    return m_deltaFrameTime;
}

float Engine::TimeElapsed()
{
    return m_timeElapsed;
}

void Engine::Tick()
{
    m_timeElapsed += DeltaFrameTime();
    auto currTime = std::chrono::high_resolution_clock::now();
    auto diffTime = std::chrono::duration<float>(currTime - m_prevFrameTime);
    float diffTimeSec = diffTime.count();
    m_prevFrameTime = currTime;
    m_deltaFrameTime = diffTimeSec;
    //std::cout << "fps: " << 1 / diffTimeSec << std::endl;
}

void Engine::ToggleAxis()
{

}

void Engine::Shutdown()
{
    delete UI;      UI = nullptr;
    delete Input;   Input = nullptr;
    delete Renderer; Renderer = nullptr;
    delete Camera;  Camera = nullptr;
    delete Window;  Window = nullptr;
}


