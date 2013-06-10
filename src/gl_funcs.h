#pragma once
#include "mi/Core.h"
#include <windows.h>		//(the GL headers need it)
#include <GL/gl.h>
#include <GL/glut.h>


struct glRGBA
{
	static glRGBA white,black;
	
	uint8 r,g,b,a;

	glRGBA() : r(255),g(255),b(255),a(255) { }

	glRGBA(int r, int g, int b)
	{
		set(r,g,b,255);
	}

	glRGBA(int r, int g, int b, int a)
	{
		set(r,g,b,a);
	}

	void set(int r, int g, int b, int a=255)
	{
		this->r = (uint8)r;
		this->g = (uint8)g;
		this->b = (uint8)b;
		this->a = (uint8)a;
	}

	void glColorUpdate();
};

struct vec
{
	float x,y,z;

	vec() { x=y=z=0.0f; }
	vec(float x, float y, float z) { this->x=x; this->y=y; this->z=z; }

	void glVertexUpdate();
};

struct Point2i
{
	int x,y;
	Point2i() : x(0),y(0) { }
	Point2i(int x, int y) { this->x=x; this->y=y; }
	void glVertex2() const;
};

struct Point2f
{
	float x,y;
	Point2f() : x(0),y(0) { }
	Point2f(float x, float y) { this->x=x; this->y=y; }
	void glVertex2() const;
};



class gl
{
public:
	// glMatrixMode and Identity
	static void ModelView();
	static void Projection();
	static void LoadIdentity();

	// CapState
	static void DepthTest(bool state)        { CapState(GL_DEPTH_TEST, state); }
	static void Texture(bool state)          { CapState(GL_TEXTURE_2D, state); }
	static void CapState(int cap, bool state);

	static void AlphaBlending();

	// Full Screen
	static void ToggleFullScreen();
	static bool IsFullScreen();

	// Drawing functions
	static void Rectangle(int x, int y, int w, int h);
	static void RectangleFill(int x, int y, int w, int h);
	static void Line3D(vec p1, vec p2);
	static void Line2D(Point2i p1, Point2i p2);

private:
	struct Data
	{
		bool fullscreen;
	};

private:
	static Data& data() { static Data d; return d; }
};

class ModelViewObject
{
public:
	ModelViewObject()
	{
		gl::ModelView();
		glPushMatrix();
		gl::LoadIdentity();
	}
	~ModelViewObject()
	{
		glPopMatrix();
		gl::Projection();
	}
};
