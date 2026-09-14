#include <Engine/Core/Root.h>

#if defined(AQUANACT_GAME) && !defined(AQUANACT_WEB)
#include <Windows.h>
#endif

static int RunApplication(int argc, char** argv)
{
    Root root;
    root.startUp(argc, argv);
    root.run();
    root.shutDown();
    return 0;
}

#if defined(AQUANACT_GAME) && !defined(AQUANACT_WEB)
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


