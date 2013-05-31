#include "miImage.h"
#include <Windows.h>
#include "FreeImage.h"
#pragma comment(lib, "FreeImage.lib")
#include <vector>
#include <GL/glut.h>


static inline int getBestTexSize(int value)
{
	int x = 1;
	while (x<4096 && x<value)
	{
		x <<= 1;
	}
	return x;
}




bool miImage::createFromImageA(const char* filename)
{
	this->_tex_h = 0;
	this->_tex_w = 0;
	this->_gl_tex = 0u;

	auto format = FreeImage_GetFIFFromFilename(filename);
	if (format==FIF_UNKNOWN)
	{
		fprintf(stderr, "createFromImage format error: path('%s')\n", filename);
		return false;
	}

	FIBITMAP* bitmap = FreeImage_Load(format, filename);
	if (bitmap==nullptr)
	{
		fprintf(stderr, "createFromImage load error: path('%s')\n", filename);
		return false;
	}

	this->_img_w = static_cast<int>(FreeImage_GetWidth(bitmap));
	this->_img_h = static_cast<int>(FreeImage_GetHeight(bitmap));
	this->_bpp   = static_cast<int>(FreeImage_GetBPP(bitmap));
	this->_has_alpha = (_bpp==32);
	this->_linear = true;

	// SIZE Check
	const int OV = 4096;
	if (_img_w==0 || _img_h==0) throw "Image size is zero";
	if (_img_w>OV || _img_h>OV) throw "Image size too large";

	// BPP Check
	switch (_bpp)
	{
	case 8:   // Palette 256
	case 15:  // R5 G5 B5
	case 16:  // R5 G6 B5
	case 24:  // R8 G8 B8
	case 32:  // R8 G8 B8 A8
		break;
	default:
		throw "Illegal bpp";
	}

	_DibToPicture(bitmap);
	FreeImage_Unload(bitmap);
	return true;
}

void miImage::_DibToPicture(void* void_dib)
{
	auto* dib = static_cast<FIBITMAP*>(void_dib);

	const RGBQUAD* palette = FreeImage_GetPalette(dib);
	const int colorkey = FreeImage_HasBackgroundColor(dib) ? FreeImage_GetTransparentIndex(dib) : 0u;


	this->_tex_w = getBestTexSize(_img_w);
	this->_tex_h = getBestTexSize(_img_h);


	const int total_size = this->_tex_w * this->_tex_h;
	std::vector<RGBA_raw> pixels;
	pixels.resize(total_size);
	RGBA_raw black = {0,0,0,0};
	int i=0;
	for (int y=0; y<_tex_h; ++y)
	{
		for (int x=0; x<_tex_w; ++x)
		{
			//pixels[i] = black;
			pixels[i].r = x * 255 / _tex_w;
			pixels[i].g = y * 255 / _tex_h;
			pixels[i].b = 200;
			pixels[i].a = 220;
			++i;
		}
	}

	auto conv_index = [&](RGBA_raw& px, int x, int y) {
			BYTE index;
			FreeImage_GetPixelIndex(dib,static_cast<uint>(x),static_cast<uint>(y),&index);
			px.r = palette[index].rgbRed;
			px.g = palette[index].rgbGreen;
			px.b = palette[index].rgbBlue;
			px.a = colorkey==index ? 0 : 255;
		};
	auto conv_rgba = [&](RGBA_raw& px, int x, int y, bool alpha) {
			RGBQUAD colour;
			FreeImage_GetPixelColor(dib,static_cast<uint>(x),static_cast<uint>(y),&colour);
			px.r = colour.rgbRed;
			px.g = colour.rgbGreen;
			px.b = colour.rgbBlue;
			px.a = alpha ? colour.rgbReserved : 255;
		};
#if 1
	for (int y=0; y<_img_h; ++y)
	{
		auto* px = &pixels[(_img_h-1-y) * _tex_w];
		for (int x=0; x<_img_w; ++x)
		{
			switch (_bpp)
			{
			case 8:   conv_index(*px,x,y);       break;
			case 15:  conv_rgba(*px,x,y,false);  break;
			case 16:  conv_rgba(*px,x,y,false);  break;
			case 24:  conv_rgba(*px,x,y,false);  break;
			case 32:  conv_rgba(*px,x,y,true);   break;
			}
			
			px->a = y * 255 / _img_h;
			px->a = 150;

			++px;
		}
	}
#endif

	glEnable(GL_TEXTURE_2D);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glGenTextures(1, &this->_gl_tex);
	glBindTexture(GL_TEXTURE_2D, this->_gl_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, _linear ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, _linear ? GL_LINEAR : GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		static_cast<GLsizei>(_tex_w),
		static_cast<GLsizei>(_tex_h),
		0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

	glBindTexture(GL_TEXTURE_2D, 0);

	this->_tex_x_ratio = (float)_img_w / _tex_w;
	this->_tex_y_ratio = (float)_img_h / _tex_h;
}

void miImage::draw(int x, int y, int w, int h, int alpha)
{
	glBindTexture(GL_TEXTURE_2D, this->_gl_tex);
	glColor4f(1, 1, 1, alpha/255.0f);

	const int x1 = x;
	const int y1 = y;
	const int x2 = x + w;
	const int y2 = y + h;
	const float u = this->_tex_x_ratio;
	const float v = this->_tex_y_ratio;

	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex2f(x1, y1);
	glTexCoord2f(u, 0); glVertex2f(x2, y1);
	glTexCoord2f(u, v); glVertex2f(x2, y2);
	glTexCoord2f(0, v); glVertex2f(x1, y2);
	glEnd();
}

void miImage::drawRotated(int x, int y, int w, int h, float rot, int alpha)
{
	glBindTexture(GL_TEXTURE_2D, this->_gl_tex);
	glColor4f(1, 1, 1, alpha/255.0f);

	glPushMatrix();
	glTranslatef(x, y, 0.0f);
	glRotatef(rot, 0.0f, 0.0f, 1.0f);

	const int x1 = -w/2;
	const int y1 = -h/2;
	const int x2 = w/2;
	const int y2 = h/2;
	const float u = this->_tex_x_ratio;
	const float v = this->_tex_y_ratio;

	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex2f(x1, y1);
	glTexCoord2f(u, 0); glVertex2f(x2, y1);
	glTexCoord2f(u, v); glVertex2f(x2, y2);
	glTexCoord2f(0, v); glVertex2f(x1, y2);
	glEnd();
	glPopMatrix();
}
