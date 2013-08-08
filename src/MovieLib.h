#pragma once
#define THIS_IS_MAIN
#include "St3dData.h"
#include "gl_funcs.h"
#include "mi/Timer.h"
#include "mi/Libs.h"
#include <FreeImage.h>
#include <direct.h>

namespace stclient{

using namespace mi;
using namespace mgl;

class MovieLib
{
public:
	static void createDots(Dots& dest, const Dots& src, float add_x);
	static const string getPictureFilename(int frame_number);
	static void saveScreenShot(const string& filename);
};

class ObjWriter
{
public:
	static void create(float output_dot_size, mi::File& f, const Dots& dots);
	static void create(float output_dot_size, mi::File& f, const std::vector<Dots>& dots);
	static int  outputCube(float output_dot_size, mi::File& f, const Dots& dots, float add_x, int face_base);
	static int  outputTetra(float output_dot_size, mi::File& f, const Dots& dots, float add_x, int face_base);
};

}//namespace stclient
