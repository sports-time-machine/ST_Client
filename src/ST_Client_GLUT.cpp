#include "ST_Client.h"
#include "gl_funcs.h"

using namespace stclient;


void StClient::glutIdle()
{
	glutPostRedisplay();
}
void StClient::glutDisplay()
{
	StClient::ms_self->display();
}
void StClient::glutKeyboard(unsigned char key, int x, int y)
{
	StClient::ms_self->onKey(key, x, y);
}
void StClient::glutKeyboardSpecial(int key, int x, int y)
{
	StClient::ms_self->onKey(key+1000, x, y);
}
void StClient::glutMouse(int button, int state, int x, int y)
{
	StClient::ms_self->onMouse(button, state, x, y);
}
void StClient::glutMouseMove(int x, int y)
{
	StClient::ms_self->onMouseMove(x, y);
}
void StClient::glutReshape(int width, int height)
{
	global.window_w = width;
	global.window_h = height;
	glViewport(0, 0, width, height);
}
