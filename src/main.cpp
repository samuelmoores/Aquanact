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
    gWindow.SwapBuffers();
    gWindow.PollEvents();
}

int main()
{
    gWindow.startUp();
    gGraphicsDevice.startUp(gWindow);
    gCamera.startUp();
    gRenderManager.startUp(gGraphicsDevice);

    while (!gWindow.ShouldClose())
        mainLoop();

    gRenderManager.shutDown();
    gCamera.shutDown();
    gGraphicsDevice.shutDown();
    gWindow.shutDown();
    return 0;
}
