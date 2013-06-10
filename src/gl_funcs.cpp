#include "gl_funcs.h"


glRGBA
	glRGBA::white(255,255,255),
	glRGBA::black(0,0,0);

void glRGBA::glColorUpdate()
{
	glColor4ub(r,g,b,a);
}

void vec::glVertexUpdate()
{
	glVertex3f(x, y, z);
}


void Point2i::glVertex2() const
{
	::glVertex2i(x, y);
}

void Point2f::glVertex2() const
{
	::glVertex2f(x, y);
}


void gl::Rectangle(int x, int y, int w, int h)
{
	glDisable(GL_TEXTURE_2D);

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

void gl::RectangleFill(int x, int y, int w, int h)
{
	glDisable(GL_TEXTURE_2D);

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

void gl::Line2D(Point2i p1, Point2i p2)
{
	glDisable(GL_TEXTURE_2D);

	glBegin(GL_LINE_STRIP);
	p1.glVertex2();
	p2.glVertex2();
	glEnd();
}

void gl::Line3D(vec p1, vec p2)
{
	glDisable(GL_TEXTURE_2D);

	glBegin(GL_LINE_STRIP);
	p1.glVertexUpdate();
	p2.glVertexUpdate();
	glEnd();
}

void gl::CapState(int cap, bool state)
{
	(state ? glEnable : glDisable)(cap);
}

void gl::AlphaBlending()
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void gl::Projection()
{
	glMatrixMode(GL_PROJECTION);
}

void gl::ModelView()
{
	glMatrixMode(GL_MODELVIEW);
}

void gl::LoadIdentity()
{
	glLoadIdentity();
}

void gl::ToggleFullScreen()
{
	bool& is_fullscreen = data().fullscreen;
	if (is_fullscreen)
	{
		glutReshapeWindow(640,480);
	}
	else
	{
		glutFullScreen();
	}
	is_fullscreen = !is_fullscreen;
}
