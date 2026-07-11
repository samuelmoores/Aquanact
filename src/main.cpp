#include <iostream>
#include <Globals.h>
#include <RenderManager.h>
#include <Window.h>
#include <Camera.h>

Window* gWindow = nullptr;
Camera* gCamera = nullptr;
RenderManager gRenderManager;

static void mainLoop()
{
    gRenderManager.Loop();

    glfwSwapBuffers(gWindow->GLFW());
    glfwPollEvents();
}

int main()
{
    gWindow = new Window();
    gladLoadGL();
    gCamera = new Camera();
    gRenderManager.Init();

    while (!glfwWindowShouldClose(gWindow->GLFW()))
        mainLoop();
    delete gCamera; gCamera = nullptr;
    delete gWindow; gWindow = nullptr;
    glfwTerminate();
    return 0;
}
