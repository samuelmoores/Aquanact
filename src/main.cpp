#include <Globals.h>
#include <RenderManager.h>
#include <Object3D.h>
#include <SceneManager.h>
#include <ProjectManager.h>
#include <Window.h>
#include <Debug.h>
#include <FileManager.h>
#include <FileSystem.h>
#include <Input.h>
#include <FrontEndManager.h>
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
SceneManager gSceneManager;
ProjectManager gProjectManager(gFileSystem);
Input gInput;

static void mainLoop()
{
    gInput.Update();

    gRenderManager.Loop();
}

namespace {
	enum class LaunchMode {
		Editor,
		Game
	};

	struct LaunchConfig {
		LaunchMode mode = LaunchMode::Editor;
		std::filesystem::path projectPath = std::filesystem::current_path() / "project.aqua";
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

	gWindow.startUp();
    gRenderManager.startUp(gWindow);
    gFrontEndManager.startUp(gWindow);
    gInput.startUp(gWindow);
#ifdef AQUANACT_EDITOR
    if (launchConfig.mode == LaunchMode::Editor)
    {
        gFrontEndManager.SetMode(FrontEndMode::Editor);
        gInput.AttachCamera(gRenderManager.GetEngineCamera());
        gDebug.startUp();
        gFileManager.startUp();
    }
    else
    {
        gFrontEndManager.SetMode(FrontEndMode::Game);
    }
#else
    gFrontEndManager.SetMode(launchConfig.mode == LaunchMode::Editor ? FrontEndMode::Editor : FrontEndMode::Game);
#endif
    gProjectManager.LoadProject(launchConfig.projectPath, gSceneManager);

    while (!gWindow.ShouldClose())
        mainLoop();

#ifdef AQUANACT_EDITOR
    if (launchConfig.mode == LaunchMode::Editor)
    {
        gDebug.shutDown();
        gFileManager.shutDown();
    }
#endif
    gFrontEndManager.shutDown();
    gSceneManager.Clear();
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
