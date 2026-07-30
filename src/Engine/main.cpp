#include <Engine/Globals.h>
#include <Engine/EventManager.h>
#include <Engine/RenderManager.h>
#include <Engine/Entity.h>
#include <Engine/LevelManager.h>
#include <Engine/ProjectManager.h>
#include <Engine/GameplayManager.h>
#include <Engine/Window.h>
#include <Engine/Debug.h>
#include <Engine/FileManager.h>
#include <Engine/FileSystem.h>
#include <Engine/Input.h>
#include <Engine/FrontEndManager.h>
#include <Engine/SimulationManager.h>
#include <filesystem>

#ifdef AQUANACT_GAME
#include <Windows.h>
#endif

Window gWindow;
RenderManager gRenderManager;
FrontEndManager gFrontEndManager;
Debug gDebug;
EventManager gEventManager;
FileSystem gFileSystem;
FileManager gFileManager(gFileSystem);
LevelManager gLevelManager;
ProjectManager gProjectManager(gFileSystem);
GameplayManager gGameplayManager;
Input gInput;
EngineState gEngineState;

// Resolve the default project in priority order:
// 1. a project next to the executable
// 2. a project in the current working directory
// 3. the source-tree fallback when running from a dev build
static std::filesystem::path DefaultProjectPath()
{
	const std::filesystem::path executableProject = gFileSystem.ExecutableDirectory() / "project.aqua";
	if (std::filesystem::exists(executableProject))
	{
		return executableProject;
	}

	const std::filesystem::path packagedProject = std::filesystem::current_path() / "project.aqua";
	if (std::filesystem::exists(packagedProject))
	{
		return packagedProject;
	}

#ifdef AQUANACT_SOURCE_ROOT
	return std::filesystem::path(AQUANACT_SOURCE_ROOT) / "assets" / "projects" / "project.aqua";
#else
	return packagedProject;
#endif
}

static int RunApplication(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    SimulationManager simulationManager;
#ifdef AQUANACT_GAME
    gEngineState.SetMode(EngineMode::Game);
#else
    gEngineState.SetMode(EngineMode::Editor);
#endif

    gWindow.startUp();
    gRenderManager.startUp(gWindow);
    gFrontEndManager.startUp(gWindow);
    gInput.startUp(gWindow);
    gDebug.startUp();
    gFileManager.startUp();

    // not a subsystem, no stateful runtime
    // unless additions are made such as,
    // cached project load resources, open file handles, pending autosave state, project session state
    gProjectManager.LoadProject(DefaultProjectPath(), gLevelManager); 
    //therefore no shutdown yet

    gGameplayManager.startUp(gLevelManager);

    simulationManager.run(gWindow, gInput, gGameplayManager, gRenderManager, gEngineState);

    gDebug.LogMessage("Main loop exiting because the window close flag was set.");

    gGameplayManager.shutDown();
    gFileManager.shutDown();
    gDebug.shutDown();
    gInput.shutDown();
    gFrontEndManager.shutDown();
    gRenderManager.shutDown();
    gWindow.shutDown();
    return 0;
}

#ifdef AQUANACT_GAME
int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return RunApplication(__argc, __argv);
}
#else
int main(int argc, char** argv)
{
    return RunApplication(argc, argv);
}
#endif


