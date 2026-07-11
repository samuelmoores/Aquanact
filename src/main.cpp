#include <iostream>
#include <Globals.h>
#include <RenderManager.h>
#include <Window.h>
#include <Camera.h>
#include <OpenGLGraphicsDevice.h>

Window* gWindow = nullptr;
Camera* gCamera = nullptr;
RenderManager* gRenderManager = nullptr;
OpenGLGraphicsDevice* gGraphicsDevice = nullptr;

static void mainLoop()
{
    gRenderManager->Loop();
    glfwPollEvents();
}

int main()
{
    gWindow = new Window();
    gGraphicsDevice = new OpenGLGraphicsDevice(*gWindow);
    gGraphicsDevice->Initialize();
    gCamera = new Camera();
    gRenderManager = new RenderManager(*gGraphicsDevice);
    gRenderManager->Init();

    while (!glfwWindowShouldClose(gWindow->GLFW()))
        mainLoop();
    delete gRenderManager; gRenderManager = nullptr;
    delete gGraphicsDevice; gGraphicsDevice = nullptr;
    delete gCamera; gCamera = nullptr;
    delete gWindow; gWindow = nullptr;
    glfwTerminate();
    return 0;
}
