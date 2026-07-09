#include <iostream>
#include <chrono>
#include <Engine.h>
#include "PlayerController.h"

static PlayerController* g_playerController = nullptr;

static void mainLoop()
{
    Engine::Tick();

    Engine::Input->Loop();
    g_playerController->Update();
    Engine::Renderer->Loop();

    glfwSwapBuffers(Engine::Window->GLFW());
    glfwPollEvents();
}

int main()
{
    Engine::Init();

    Engine::Level->PrepareLoad();
    while (!Engine::Level->StepLoad());

    g_playerController = new PlayerController(Engine::Level->Objects());

    // void_01 = harry, void_02 = kratos, void_03 = tony
    g_playerController->SetVoidMessage(1,
        "[ VOID SECTOR - HARRY ]\n\n"
        "I TOLD you that, do you not remember?\n"
        "Something got into our logic and spoiled it.\n\n"
        "Do you understand?\nHello?\n");
    g_playerController->SetVoidMessage(2,
        "[ VOID SECTOR - KRATOS ]\n\n"
        "Brute force is no match for me.\n"
        "Don't even think about it.\n"
        "Move along, plebeian.\n\n"
        "What you seek, you will not find.");
    g_playerController->SetVoidMessage(3,
        "[ VOID SECTOR - TONY ]\n\n"
        "You made it.\nYou found the end.\n"
        "Go and be free at last.\n"
        "The algorithm will remember you.");

    Engine::Camera->SetObjects();
    glfwSetInputMode(Engine::Window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    Engine::Input->ResetMouseLook();

    while (Engine::Running())
        mainLoop();
    Engine::Shutdown();
    glfwTerminate();
    return 0;
}
