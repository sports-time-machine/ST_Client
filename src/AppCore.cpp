// Win32Ç‚OpenGLÇ…ä÷Ç∑ÇÈÉRÉAÇ»Ç±Ç∆

#include "St3dData.h"
#include "gl_funcs.h"
#include <GL/glfw.h>
#pragma comment(lib,"GLFW_x32.lib")

using namespace stclient;
using namespace mgl;

static HWND GetGlfwHwnd(void)
{
	const int buffer_size = 1024;
	static char new_title[buffer_size];

	wsprintf(new_title,"%d/%d", GetTickCount(), GetCurrentProcessId());
	glfwSetWindowTitle(new_title);
	Sleep(40);
	return FindWindow(NULL, new_title);
}

static void FullScreen()
{
	const HWND hwnd = GetGlfwHwnd();
	SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
	const int dispx = GetSystemMetrics(SM_CXSCREEN);
	const int dispy = GetSystemMetrics(SM_CYSCREEN);
	glfwSetWindowSize(dispx, dispy);
}

static void init_open_gl_params()
{
	glEnable(GL_TEXTURE_2D);
	glHint(GL_LINE_SMOOTH_HINT,            GL_NICEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint(GL_POLYGON_SMOOTH_HINT,         GL_NICEST);
	gl::AlphaBlending(true);
}

bool AppCore::initGraphics(bool full_screen, const std::string& title)
{
	auto open_window = [&]()->int{
		printf("Initial Fullscreen: %d\n", full_screen);
		return glfwOpenWindow(
			640,
			480,
			0, 0, 0,
			0, 0, 0,
			GLFW_WINDOW);
	};

	if (glfwInit()==GL_FALSE)
	{
		return false;
	}
	if (open_window()==GL_FALSE)
	{
		return false;
	}

	if (full_screen)
	{
		FullScreen();
	}
	else
	{
		glfwSetWindowTitle(title.c_str());
	}

	init_open_gl_params();
	return true;
}

void AppCore::MyGetKeyboardState(BYTE* kbd)
{
	if (glfwGetWindowParam(GLFW_ACTIVE))
	{
		GetKeyboardState(kbd);
	}
	else
	{
		memset(kbd, 0, 256);
	}
}

