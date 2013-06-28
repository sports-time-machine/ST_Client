#pragma once
#include "Core.h"

namespace mi{

class Image
{
public:
	bool createFromImageA(const std::string& filename);

	void draw(int x, int y, int w, int h, int alpha=255) const;
	void drawDepth(int x, int y, int w, int h, float z, int alpha=255) const;
	void drawRotated(int x, int y, int w, int h, float rot, int alpha=255) const;


	// miImage is GLuint compatible
	operator uint()   const { return _gl_tex; }
	uint getTexture() const { return _gl_tex; }
	bool enabled()    const { return _gl_tex!=0u; }

	float getTextureWidth()  const { return _tex_x_ratio; }
	float getTextureHeight() const { return _tex_y_ratio; }

private:
	void _DibToPicture(void* void_dib);

private:
	uint _gl_tex;
	int _img_w, _img_h, _bpp;
	int _tex_w, _tex_h;
	float _tex_x_ratio, _tex_y_ratio;
	bool _has_alpha, _linear;
};

}//namespace mi
