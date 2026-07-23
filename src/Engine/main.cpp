#include <Engine/Globals.h>
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
#include <filesystem>
#include <string_view>

#ifdef AQUANACT_GAME
#include <Windows.h>
#endif

Window gWindow;
RenderManager gRenderManager;
FrontEndManager gFrontEndManager;
Debug gDebug;
FileSystem gFileSystem;
FileManager gFileManager(gFileSystem);
LevelManager gLevelManager;
ProjectManager gProjectManager(gFileSystem);
GameplayManager gGameplayManager;
Input gInput;
EngineState gEngineState;

static void mainLoop()
{
    gInput.Update();
    if (gEngineState.IsGameMode())
    {
        gGameplayManager.Update(gInput.DeltaTime());
    }

    gRenderManager.Loop();
}

namespace {
	std::filesystem::path DefaultProjectPath()
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

	enum class LaunchMode {
		Editor,
		Game
	};

	struct LaunchConfig {
		LaunchMode mode = LaunchMode::Editor;
		std::filesystem::path projectPath = DefaultProjectPath();
	};

	LaunchConfig ParseLaunchConfig(int argc, char** argv)
	{
		LaunchConfig config;
		const std::filesystem::path executableName = (argc > 0 && argv && argv[0]) ? std::filesystem::path(argv[0]).filename() : std::filesystem::path();
		if (executableName == "game.exe")
		{
			config.mode = LaunchMode::Game;
		}

		for (int i = 1; i < argc; ++i)
		{
			const std::string_view arg = argv[i];
			if (arg == "--game")
			{
				config.mode = LaunchMode::Game;
			}
			else if (arg == "--editor")
			{
				config.mode = LaunchMode::Editor;
			}
			else if (arg.rfind("--project=", 0) == 0)
			{
				config.projectPath = std::filesystem::path(arg.substr(std::string_view("--project=").size()));
			}
		}
		return config;
	}
}

static int RunApplication(int argc, char** argv)
{
    const LaunchConfig launchConfig = ParseLaunchConfig(argc, argv);
    gEngineState.SetMode(launchConfig.mode == LaunchMode::Editor ? EngineMode::Editor : EngineMode::Game);

    gWindow.startUp();
    gRenderManager.startUp(gWindow);
    gFrontEndManager.startUp(gWindow);
    gInput.startUp(gWindow);
    gDebug.startUp();
    gFileManager.startUp();
    gProjectManager.LoadProject(launchConfig.projectPath, gLevelManager);
    Level* activeLevel = gLevelManager.startUp();
    gGameplayManager.startUp(gLevelManager, activeLevel);

    while (!gWindow.ShouldClose())
        mainLoop();

    gDebug.LogMessage("Main loop exiting because the window close flag was set.");

    gDebug.shutDown();
    gFileManager.shutDown();
    gFrontEndManager.shutDown();
    gGameplayManager.shutDown();
    gInput.shutDown();
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


