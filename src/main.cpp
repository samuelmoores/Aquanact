#include <iostream>
#include <chrono>
#include <Engine.h>

//declare subsystem managers

//

static void mainLoop()
{
    Engine::Tick();

    Engine::Renderer->Loop();

    glfwSwapBuffers(Engine::Window->GLFW());
    glfwPollEvents();
}

int main()
{
    //call startup functions for subsystem managers
    Engine::Init();
    //

    while (Engine::Running())
        mainLoop();
    Engine::Shutdown();
    glfwTerminate();
    return 0;
}
