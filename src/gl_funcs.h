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


extern void glRectangle(glRGBA rgba, int x, int y, int w, int h);
extern void glRectangleFill(glRGBA rgba, int x, int y, int w, int h);
