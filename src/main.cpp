#include <Globals.h>
#include <RenderManager.h>
#include <Object3D.h>
#include <SceneManager.h>
#include <ProjectManager.h>
#include <Window.h>
#include <Debug.h>
#include <EngineGUI.h>
#include <FileManager.h>
#include <FileSystem.h>
#include <Input.h>

Window gWindow;
RenderManager gRenderManager;
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
    gRenderManager.startUp(gWindow);
    gInput.startUp(gWindow);
    gInput.AttachCamera(gRenderManager.GetCamera());
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
    gWindow.shutDown();
    return 0;
}
