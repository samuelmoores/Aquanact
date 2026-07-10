#include <iostream>
#include <chrono>
#include <Engine.h>

static void mainLoop()
{
    Engine::Tick();

    Engine::Input->Loop();
    Engine::Renderer->Loop();

    glfwSwapBuffers(Engine::Window->GLFW());
    glfwPollEvents();
}

int main()
{
    Engine::Init();

    glfwSetInputMode(Engine::Window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    Engine::Input->ResetMouseLook();

    while (Engine::Running())
        mainLoop();
    Engine::Shutdown();
    glfwTerminate();
    return 0;
}
