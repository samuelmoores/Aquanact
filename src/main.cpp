#include <iostream>
#include <Globals.h>
#include <RenderManager.h>
#include <Window.h>
#include <EngineCamera.h>
#include <OpenGLGraphicsDevice.h>
#include <Debug.h>
#include <Input.h>

Window gWindow;
EngineCamera gEngineCamera;
RenderManager gRenderManager;
OpenGLGraphicsDevice gGraphicsDevice;
Debug gDebug;
Input gInput;

static void mainLoop()
{
    gInput.Update();

    gRenderManager.Loop();
}

int main()
{
    gWindow.startUp();
    gGraphicsDevice.startUp(gWindow);
    gEngineCamera.startUp();
    gRenderManager.startUp(gGraphicsDevice);
    gInput.startUp(gWindow);
    gInput.AttachCamera(gEngineCamera);
    gDebug.startUp();
    gDebug.SetLoggingEnabled(true);

    while (!gWindow.ShouldClose())
        mainLoop();

    gDebug.shutDown();
    gInput.shutDown();
    gRenderManager.shutDown();
    gEngineCamera.shutDown();
    gGraphicsDevice.shutDown();
    gWindow.shutDown();
    return 0;
}
