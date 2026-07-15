#include <Globals.h>
#include <RenderManager.h>
#include <FrontEndManager.h>
#include <Object3D.h>
#include <SceneManager.h>
#include <ProjectManager.h>
#include <Window.h>
#include <Debug.h>
#include <FileManager.h>
#include <FileSystem.h>
#include <Input.h>

Window gWindow;
RenderManager gRenderManager;
FrontEndManager gFrontEndManager;
Debug gDebug;
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
    gFrontEndManager.startUp(gWindow);
    gInput.startUp(gWindow);
    gInput.AttachCamera(gRenderManager.GetEngineCamera());
    gDebug.startUp();
    gFileManager.startUp();
    gProjectManager.LoadProject("C:/dev/Aquanact/assets/projects/project.aqua", gSceneManager);

    while (!gWindow.ShouldClose())
        mainLoop();

    gDebug.shutDown();
    gFrontEndManager.shutDown();
    gFileManager.shutDown();
    gSceneManager.Clear();
    gInput.shutDown();
    gRenderManager.shutDown();
    gWindow.shutDown();
    return 0;
}
