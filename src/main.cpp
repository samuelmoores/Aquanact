#include <iostream>
#include <Globals.h>
#include <RenderManager.h>
#include <Window.h>
#include <Camera.h>
#include <OpenGLGraphicsDevice.h>
#include <Debug.h>

Window gWindow;
Camera gCamera;
RenderManager gRenderManager;
OpenGLGraphicsDevice gGraphicsDevice;
Debug gDebug;

static void mainLoop()
{
    gRenderManager.Loop();
    gDebug.draw(gCamera);
    gWindow.SwapBuffers();
    gWindow.PollEvents();
}

int main()
{
    gWindow.startUp();
    gGraphicsDevice.startUp(gWindow);
    gCamera.startUp();
    gRenderManager.startUp(gGraphicsDevice);
    gDebug.startUp();

    while (!gWindow.ShouldClose())
        mainLoop();

    gDebug.shutDown();
    gRenderManager.shutDown();
    gCamera.shutDown();
    gGraphicsDevice.shutDown();
    gWindow.shutDown();
    return 0;
}
