#include <Engine/Core/Root.h>
#include <Engine/Core/BootLog.h>

#include <exception>

#if defined(AQUANACT_GAME) && !defined(AQUANACT_WEB) && defined(_WIN32)
#include <Windows.h>
#endif

static int RunApplication(int argc, char** argv)
{
	BootLog::Initialize(argc > 0 ? argv[0] : nullptr);
	try
	{
		BootLog::SetStage("Root construction");
		Root root;
		root.startUp(argc, argv);
		BootLog::SetStage("application running");
		root.run();
		BootLog::SetStage("application shutdown");
		root.shutDown();
		BootLog::SetStage("shutdown complete");
		return 0;
	}
	catch (const std::exception& exception)
	{
		BootLog::WriteException("unhandled std::exception", exception.what());
		return 1;
	}
	catch (...)
	{
		BootLog::WriteException("unhandled non-standard exception", "unknown exception");
		return 1;
	}
}

#if defined(AQUANACT_GAME) && !defined(AQUANACT_WEB) && defined(_WIN32)
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


