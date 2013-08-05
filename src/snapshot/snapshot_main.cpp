#define THIS_IS_MAIN
#include "../St3dData.h"
//#include "../gl_funcs.h"
//#include "../FreeType.h"
#include "../mi/Timer.h"
#include "../mi/Libs.h"
#include "../MovieLib.h"
#pragma warning(disable:4244)

template<typename T> T minmax(T val, T min, T max)
{
	return (val<min) ? min : (val>max) ? max : val;
}

static void DenugPrintln(const char* s)
{
	OutputDebugString(s);
	OutputDebugString("\n");
}


static const float PI = 3.1415f;

using namespace mi;
using namespace stclient;
using namespace mgl;

class SixMovies
{
public:
	void load(const string& basename)
	{
		for (int i=0; i<6; ++i)
		{
			if (!mov[i].load(basename+"-"+mi::Lib::to_s(1+i)+".stmov"))
			{
				DenugPrintln("load failed");
			}
		}
	}

private:
	std::map<int,MovieData> mov;
};

class SnapshotApp
{
public:
	SixMovies  mov;
	int        frame;
	Dots*      dots_original;
	float      output_dot_size;
	int        picture_interval;
	bool       debug_show;

	SnapshotApp()
	{
		frame = 0;
		output_dot_size = 1.0f;
		picture_interval = 1;
		debug_show = true;
	}

	void init()
	{
	}

	void load(const string& basename)
	{
		DenugPrintln("load");
		DenugPrintln(basename.c_str());
		mov.load(basename);
	}

	struct Data
	{
		enum FrameDir
		{
			NO_DIR,
			INCR,
			DECR,
		};
		FrameDir  frame_auto;
		int       frame_index;
		bool      output_picture;

		Data()
		{
			frame_auto = NO_DIR;
			frame_index = 0;
			output_picture = false;
		}
	} data;

	void processFrameIncrement()
	{
		switch (data.frame_auto)
		{
		case Data::INCR:
			++data.frame_index;
			break;
		case Data::DECR:
			--data.frame_index;
			if (data.frame_index<0)
			{
				data.frame_index = 0;
				data.frame_auto = Data::NO_DIR;
			}
			break;
		}
	}
};

// 簡易的にベース名を作る
// from: "C:/Folder/AAAAAA-1.stmov"
//   to: "C:/Folder/AAAAAA"
static string getBaseName(const char* _s)
{
	string s(_s);

	// erase "-1.stmov$"
	return s.substr(0, s.length()-8);
}

static bool run_app(string arg)
{
	SnapshotApp app;
	app.init();
	if (arg.length()>0)
	{
		if (arg[0]=='"')
		{
			arg = arg.substr(1, arg.length()-2);
		}

		app.load(getBaseName(arg.c_str()));
	}

	return true;
}

int main(int argc, const char* argv[])
{
	return run_app(argc>=2 ? argv[1] : "") ? EXIT_SUCCESS : EXIT_FAILURE;
}
