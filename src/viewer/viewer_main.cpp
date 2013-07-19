#define THIS_IS_MAIN
#include "../St3dData.h"

#pragma comment(lib,"opengl32.lib")
#pragma comment(lib,"glu32.lib")

using namespace mi;
using namespace stclient;

static bool run_app()
{
	AppCore::initGraphics(false, "ST 3D Viewer");


	return true;
}

int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int)
{
	return run_app() ? EXIT_SUCCESS : EXIT_FAILURE;
}
