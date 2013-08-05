#include "MovieLib.h"

using namespace stclient;


void MovieLib::createDots(Dots& dest, const Dots& src)
{
	for (int i=0; i<src.size(); ++i)
	{
		const Point3D& po = src[i];
		const float x = po.x;
		const float y = po.y;
		const float z = po.z;
		const bool in_x = (x>=GROUND_LEFT && x<=GROUND_RIGHT);
		const bool in_y = (y>=GROUND_BOTTOM && y<=GROUND_TOP);
		const bool in_z = (z>=GROUND_XNEAR && z<=GROUND_XFAR);

		// out of box
		if (!(in_x && in_y && in_z))
			continue;

		dest.push(po);
	}
}

const string MovieLib::getPictureFilename(int frame_number)
{
	char num[100];
	sprintf_s(num, "%05d", frame_number);

	string foldername;
	foldername += mi::Core::getDesktopFolder();
	foldername += "/Pictures";
	_mkdir(foldername.c_str());

	string filename;
	filename += foldername;
	filename += "/picture";
	filename += num;
	filename += ".png";
	return filename;
}

static void saveScreenShot(const string& filename)
{
	int w,h;
	glfwGetWindowSize(&w, &h);

	FIBITMAP* bmp = FreeImage_Allocate(w,h,24);

	// バックバッファを読む
	glReadBuffer(GL_BACK);

	std::vector<RGBQUAD> vram;
	vram.resize(w * h);
		
	// バッファの内容を
	// bmpオブジェクトのピクセルデータが格納されている領域に直接コピーする。
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, vram.data());

	int addr = 0;
	for (int y=0; y<h; ++y)
	{
		for (int x=0; x<w; ++x)
		{
			RGBQUAD color;
			color.rgbRed   = vram[addr].rgbRed;
			color.rgbGreen = vram[addr].rgbGreen;
			color.rgbBlue  = vram[addr].rgbBlue;
			FreeImage_SetPixelColor(bmp, x, y, &color);
			++addr;
		}
	}

	FreeImage_Save(FIF_PNG, bmp, filename.c_str());
	FreeImage_Unload(bmp);
}

static void output(float output_dot_size, mi::File& f, const Dots& dots)
{
	char line[1000];
	const float SZ = 0.0025f * output_dot_size;
	for (int i=0; i<dots.size(); ++i)
	{
		const auto& dot = dots[i];

		auto wr_vec = [&](const int x, const int y, const int z){
			const int len = sprintf_s(line, "v %f %f %f\n", dot.x + SZ*x, dot.y + SZ*y, dot.z + SZ*z);
			f.write(line, len);
		};

		// 8*i+0 から 8*i+7
		wr_vec(-1, -1, -1);
		wr_vec(+1, -1, -1);
		wr_vec(-1, +1, -1);
		wr_vec(+1, +1, -1);
		wr_vec(-1, -1, +1);
		wr_vec(+1, -1, +1);
		wr_vec(-1, +1, +1);
		wr_vec(+1, +1, +1);

		const int N = 8*i;
		auto wr_face = [&](int a, int b, int c, int d){
			const int len = sprintf_s(line, "f %d %d %d %d\n", N+a, N+b, N+c, N+d);
			f.write(line, len);
		};

		wr_face(1,3,4,2);
		wr_face(1,5,7,3);
		wr_face(2,4,8,6);
		wr_face(1,2,6,5);
		wr_face(3,7,8,4);
		wr_face(5,6,8,7);
	}
}

void ObjWriter::create(float output_dot_size, mi::File& f, const Dots& dots_org)
{
	static Dots dots;
	dots.init();
	MovieLib::createDots(dots, dots_org);
	output(output_dot_size, f, dots);
}
