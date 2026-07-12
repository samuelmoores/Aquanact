#include <iostream>
#include <Globals.h>
#include <RenderManager.h>
#include <Object3D.h>
#include <Window.h>
#include <EngineCamera.h>
#include <OpenGLGraphicsDevice.h>
#include <Debug.h>
#include <EngineGUI.h>
#include <FileManager.h>
#include <Input.h>

Window gWindow;
EngineCamera gEngineCamera;
RenderManager gRenderManager;
OpenGLGraphicsDevice gGraphicsDevice;
Debug gDebug;
EngineGUI gEngineGUI;
FileManager gFileManager;
std::vector<std::unique_ptr<Object3D>> gSceneObjects;
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
    gEngineGUI.startUp(gWindow);
    gFileManager.startUp();
    gDebug.SetLoggingEnabled(true);

    while (!gWindow.ShouldClose())
        mainLoop();

    gDebug.shutDown();
    gEngineGUI.shutDown();
    gFileManager.shutDown();
    gSceneObjects.clear();
    gInput.shutDown();
    gRenderManager.shutDown();
    gEngineCamera.shutDown();
    gGraphicsDevice.shutDown();
    gWindow.shutDown();
    return 0;
}
