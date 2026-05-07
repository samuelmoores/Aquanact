#include <iostream>
#include <chrono>
#include <Engine.h>
#include "PlayerController.h"
#include "MainMenu.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static PlayerController* g_playerController = nullptr;
static MainMenu*         g_mainMenu         = nullptr;

static void mainLoop()
{
#ifdef __EMSCRIPTEN__
    static bool s_firstFrame = true;
    if (s_firstFrame) {
        s_firstFrame = false;
        printf("[timing] First frame (MainMenu visible): %.1f ms\n", emscripten_get_now());
    }
#endif

    Engine::Tick();

    if (g_mainMenu && g_mainMenu->IsVisible()) {
        Engine::Renderer->RenderMenuOnly();

        if (g_mainMenu->WantsHideCursor()) {
            g_mainMenu->ConsumeHideCursor();
            glfwSetInputMode(Engine::Window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }

        if (g_mainMenu->WantsPlay()) {
            g_mainMenu->Hide();
            g_playerController = new PlayerController(Engine::Level->Objects());

            // void_01 = harry, void_02 = kratos, void_03 = tony
            g_playerController->SetVoidMessage(1,
                "[ VOID SECTOR - HARRY ]\n\n"
                "A presence lingers here.\n"
                "Something watches from between the walls.\n\n"
                "Harry was here.");
            g_playerController->SetVoidMessage(2,
                "[ VOID SECTOR - KRATOS ]\n\n"
                "The weight of this place is unbearable.\n"
                "A warrior walked these halls and left\n"
                "only silence behind.\n\n"
                "What he sought, he never found.");
            g_playerController->SetVoidMessage(3,
                "[ VOID SECTOR - TONY ]\n\n"
                "The signal is strongest here.\n"
                "Someone brilliant passed through\n"
                "and rewired everything.\n\n"
                "The algorithm remembers him.");

            Engine::Camera->SetObjects();
            glfwSetInputMode(Engine::Window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            Engine::Input->ResetMouseLook();
#ifdef __EMSCRIPTEN__
            EM_ASM({ Module.canvas.focus(); });
#endif
        }
        if (g_mainMenu->WantsQuit()) {
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#else
            glfwSetWindowShouldClose(Engine::Window->GLFW(), GLFW_TRUE);
#endif
        }
    } else {
        Engine::Input->Loop();
        g_playerController->Update();
        Engine::Renderer->Loop();
    }

    glfwSwapBuffers(Engine::Window->GLFW());
    glfwPollEvents();
}

int main()
{
#ifdef __EMSCRIPTEN__
    double tMainEntry = emscripten_get_now();

#endif

    Engine::Init();

#ifdef __EMSCRIPTEN__
    double tInitDone = emscripten_get_now();
    printf("[timing] Engine::Init: %.1f ms\n", tInitDone - tMainEntry);
    printf("[timing] WASM load + assets (before main): %.1f ms\n", tMainEntry);
#endif

    static MainMenu menu;
    g_mainMenu = &menu;
    Engine::UI->SetMainMenu(&menu);
    glfwSetInputMode(Engine::Window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (Engine::Running())
        mainLoop();
    Engine::Shutdown();
    glfwTerminate();
#endif
    return 0;
}
