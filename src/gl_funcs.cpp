#include "gl_funcs.h"


void glRGBA::glColorUpdate()
{
	glColor4ub(r,g,b,a);
}

void glRectangle(glRGBA rgba, int x, int y, int w, int h)
{
	glDisable(GL_TEXTURE_2D);
	rgba.glColorUpdate();

	const int x1 = x;
	const int y1 = y;
	const int x2 = x+w;
	const int y2 = y+h;
	glBegin(GL_LINE_LOOP);
	glVertex2i(x1, y1);
	glVertex2i(x2, y1);
	glVertex2i(x2, y2);
	glVertex2i(x1, y2);
	glEnd();
}

void glRectangleFill(glRGBA rgba, int x, int y, int w, int h)
{
	glDisable(GL_TEXTURE_2D);
	rgba.glColorUpdate();

	const int x1 = x;
	const int y1 = y;
	const int x2 = x+w;
	const int y2 = y+h;
	glBegin(GL_QUADS);
	glVertex2i(x1, y1);
	glVertex2i(x2, y1);
	glVertex2i(x2, y2);
	glVertex2i(x1, y2);
	glEnd();
}
