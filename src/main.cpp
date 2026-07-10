#include <iostream>
#include <chrono>
#include <Engine.h>

static void mainLoop()
{
    Engine::Tick();

    Engine::Renderer->Loop();

    glfwSwapBuffers(Engine::Window->GLFW());
    glfwPollEvents();
}

int main()
{
    Engine::Init();

    while (Engine::Running())
        mainLoop();
    Engine::Shutdown();
    glfwTerminate();
    return 0;
}
