#include <iostream>
#include <chrono>
#include <Engine.h>
#include "PlayerController.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static PlayerController* g_playerController = nullptr;

static void mainLoop()
{
    Engine::Tick();
    Engine::Input->Loop();
    g_playerController->Update();
    Engine::Renderer->Loop();
}

int main()
{
    Engine::Init();
    static PlayerController pc(Engine::Level->Objects());
    g_playerController = &pc;
    Engine::Camera->SetObjects();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (Engine::Running())
        mainLoop();
    glfwTerminate();
#endif
    return 0;
}
