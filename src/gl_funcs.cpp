// OpenGL関係の細々したツール

#include "gl_funcs.h"
#include <gl/glu.h>

using namespace mgl;

glRGBA
	glRGBA::white(255,255,255),
	glRGBA::black(0,0,0);

void glRGBA::glColorUpdate() const 
{
	glColor4ub(r,g,b,a);
}

void glRGBA::glColorUpdate(int alpha) const 
{
	glColor4ub(r,g,b,(uint8)(a*alpha>>8));
}

void glRGBA::glColorUpdate(float alpha) const 
{
	glColor4ub(r,g,b,(uint8)(a*alpha));
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

void gl::AlphaBlending(bool state)
{
	if (state)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		glDisable(GL_BLEND);
	}
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

void gl::DrawSphere(float x, float y, float z, float r, float ra, float rx, float ry, float rz)
{
	static GLUquadricObj *sphere = nullptr;
	if (sphere==nullptr)
	{
		sphere = gluNewQuadric();
	}

	const int SLICES = 16;
	const int STACKS = 16;
	gluQuadricDrawStyle(sphere, GLU_LINE);
	glPushMatrix();
		glLoadIdentity();
		glTranslatef(x,y,z);
		glScalef(1.0f, 1.0f, 1.0f);
		glRotatef(ra,rx,ry,rz);
		gluSphere(sphere, r, SLICES, STACKS);
	glPopMatrix();
}

void gl::DrawSphere(float x, float y, float z, float r)
{
	DrawSphere(x,y,z,r, 0.0f, 0.0f, 0.0f, 0.0f);
}


// @gcls
void gl::ClearGraphics(int r, int g, int b)
{
	glClearColor(
		r / 255.0f,
		g / 255.0f,
		b / 255.0f,
		1.00f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
