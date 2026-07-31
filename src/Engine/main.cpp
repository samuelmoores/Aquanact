#include <Engine/Root.h>

#ifdef AQUANACT_GAME
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


