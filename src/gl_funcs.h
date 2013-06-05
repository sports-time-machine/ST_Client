#pragma once
#include "miCore.h"
#include <windows.h>		//(the GL headers need it)
#include <GL/gl.h>
#include <GL/glu.h>

struct glRGBA
{
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




extern void glRectangle(int x, int y, int w, int h);
extern void glRectangleFill(int x, int y, int w, int h);
extern void glLine3D(vec p1, vec p2);
