#include <Globals.h>
#include <RenderManager.h>
#include <Object3D.h>
#include <SceneManager.h>
#include <ProjectManager.h>
#include <Window.h>
#include <EngineCamera.h>
#include <OpenGLGraphicsDevice.h>
#include <Debug.h>
#include <EngineGUI.h>
#include <FileManager.h>
#include <FileSystem.h>
#include <Input.h>

Window gWindow;
EngineCamera gEngineCamera;
RenderManager gRenderManager;
OpenGLGraphicsDevice gGraphicsDevice;
Debug gDebug;
EngineGUI gEngineGUI;
FileSystem gFileSystem;
FileManager gFileManager(gFileSystem);
SceneManager gSceneManager;
ProjectManager gProjectManager(gFileSystem);
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
    gProjectManager.LoadProject("C:/dev/Aquanact/assets/projects/project.aqua", gSceneManager);

    while (!gWindow.ShouldClose())
        mainLoop();

    gDebug.shutDown();
    gEngineGUI.shutDown();
    gFileManager.shutDown();
    gSceneManager.Clear();
    gInput.shutDown();
    gRenderManager.shutDown();
    gEngineCamera.shutDown();
    gGraphicsDevice.shutDown();
    gWindow.shutDown();
    return 0;
}
