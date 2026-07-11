#include <iostream>
#include <Globals.h>
#include <RenderManager.h>
#include <Window.h>
#include <Camera.h>
#include <OpenGLGraphicsDevice.h>

Window gWindow;
Camera gCamera;
RenderManager gRenderManager;
OpenGLGraphicsDevice gGraphicsDevice;

static void mainLoop()
{
    gRenderManager.Loop();
    glfwPollEvents();
}

int main()
{
    gWindow.startUp();
    gGraphicsDevice.startUp(gWindow);
    gCamera.startUp();
    gRenderManager.startUp(gGraphicsDevice);
    gRenderManager.Init();

    while (!glfwWindowShouldClose(gWindow.GLFW()))
        mainLoop();

    glfwTerminate();
    return 0;
}
