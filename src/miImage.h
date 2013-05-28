#pragma once
#include "miCore.h"


class miImage
{
public:
	void createFromImageA(const char* filename);

	void draw(int x, int y, int w, int h, int alpha=255);
	void drawRotated(int x, int y, int w, int h, float rot, int alpha=255);


	// miImage is GLuint compatible
	operator uint() const { return _gl_tex; }
	uint getTexture() const { return _gl_tex; }


private:
	void miImage::_DibToPicture(void* void_dib);

private:
	uint _gl_tex;
	int _img_w, _img_h, _bpp;
	int _tex_w, _tex_h;
	float _tex_x_ratio, _tex_y_ratio;
	bool _has_alpha, _linear;
};
