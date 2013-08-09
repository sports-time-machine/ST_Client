#pragma warning(disable:4244)
#include "../St3dData.h"
#include "../gl_funcs.h"
#include "../FreeType.h"
#include "../mi/Timer.h"
#include "../mi/Libs.h"
#include <FreeImage.h>
#include <direct.h>
#include "../MovieLib.h"
#include "../psl_if.h"

static const float PI = 3.1415f;

using namespace mi;
using namespace stclient;
using namespace mgl;

struct Point2D
{
	int x,y;
	Point2D() {}
	Point2D(int x, int y)            { set(x,y); }
	void set(int x, int y)           { this->x = x; this->y = y; }
	bool operator==(Point2D a) const { return x==a.x && y==a.y; }
	bool operator!=(Point2D a) const { return !operator==(a); }
};

struct CamUnit
{
	MovieData   mov;
	Dots        dots;
	Point3D     center;
	int         valid_dots;
};


struct Config
{
	glRGBA
		ground_rgba,
		sky_rgba,
		box_rgba,
		body1_rgba,
		body2_rgba,
		body3_rgba,
		body4_rgba,
		body5_rgba,
		body6_rgba;
	float
		body_dot_size,
		obj_dot_size,
		diff_2movies[2],
		diff_3movies[3],
		diff_4movies[4];
	string
		folder_format,
		png_format,
		obj_format;

	Config()
	{
		body_dot_size = 1.0f;
		obj_dot_size  = 1.0f;
	}

	void from(PSL::PSLVM& vm);
};

void Config::from(PSL::PSLVM& vm)
{
	auto PslvToRgb = [](PSL::variable v)->glRGBA
	{
		return glRGBA(v[0].toInt(), v[1].toInt(), v[2].toInt(), v[3].toInt());
	};
	auto makeDiff = [](float* dest, int n, PSL::variable v)->void
	{
		for (int i=0; i<n; ++i)
		{
			dest[i] = v[i].toDouble();
		}
	};

#define apply(NAME)       this->NAME = PSL::variable(vm.get(#NAME))
#define applyStr(NAME)    this->NAME = PSL::variable(vm.get(#NAME)).toString().c_str()
#define applyRGB(NAME)    this->NAME = PslvToRgb(PSL::variable(vm.get(#NAME)))
#define applyDiff(NAME,N) makeDiff(this->NAME,N,PSL::variable(vm.get(#NAME)))
	apply   (body_dot_size);
	apply   (obj_dot_size);
	applyStr(folder_format);
	applyStr(png_format);
	applyStr(obj_format);
	applyRGB(ground_rgba);
	applyRGB(box_rgba);
	applyRGB(sky_rgba);
	applyRGB(body1_rgba);
	applyRGB(body2_rgba);
	applyRGB(body3_rgba);
	applyRGB(body4_rgba);
	applyRGB(body5_rgba);
	applyRGB(body6_rgba);
	applyDiff(diff_2movies,2);
	applyDiff(diff_3movies,3);
	applyDiff(diff_4movies,4);
#undef apply
#undef applyRGB
}


struct CamSystem
{
	CamUnit  cams[6];
};

struct Snap3D
{
	bool  exist;
	Dots  dots;
	int   frame_index;

	Snap3D()
	{
		exist = false;
		frame_index = 0;
		dots.init();
	}
};

class ViewerAppBase
{
public:
	std::map<int,CamSystem> camsys;
	freetype::font_data font;
	EyeCore    eye;
	int        frame;
	float      output_dot_size;
	int        view2d_width;
	int        picture_interval;
	bool       debug_show;
	Config     config;
	string     game_id;
	bool       quit_requested;
	std::vector<Snap3D> snap3d;

	virtual bool onInit()             { return true; }
	virtual void onProcessMouse()     {}
	virtual void onProcessKeyboard()  {}
	virtual void onFrameBegin()       {}
	virtual void onFrameEnd()         {}
	virtual void onDisplay2D()        {}

	void quitRequest()
	{
		quit_requested = true;
	}

	bool isQuitRequested() const
	{
		return quit_requested;
	}

	
	static Point3D InvalidPoint3D()
	{
		return Point3D(-9999.0f, 0.0f, 1.0f);
	}

	ViewerAppBase()
	{
		frame            = 0;
		output_dot_size  = 1.0f;
		view2d_width     = 0;
		picture_interval = 1;
		debug_show       = true;
		quit_requested   = false;
		snap3d.resize(6);
	}

	void setDefaultCam()
	{
		view2d_width = 0;
		eye3d(-4.2f, 1.5f, 5.4f, -1.0f, -0.2f);
	}

	bool init()
	{
		font.init("C:/Windows/Fonts/Cour.ttf", 12);
		eye.setTransitionTime(80);
		eye.fast_set = false;
		setDefaultCam();
		view2d_width = 0;
		return onInit();
	}

	static void DenugPrintln(const char* s)
	{
		OutputDebugString(s);
		OutputDebugString("\n");
	}

	bool load(const string& basename)
	{
		this->game_id = basename.substr(basename.rfind('\\')+1);

		DenugPrintln("load");
		DenugPrintln(basename.c_str());
		
		auto& cams = camsys[camsys.size()].cams;
		for (int i=0; i<6; ++i)
		{
			if (!cams[i].mov.load(basename+"-"+mi::Lib::to_s(1+i)+".stmov"))
			{
				DenugPrintln("load failed");
				return false;
			}
		}
		return true;
	}

	bool loadConfigPsl()
	{
		PSL::PSLVM vm;
		string filename = mi::Core::getAppFolder()+"snapshot-config.psl";
		switch (vm.loadScript(filename.c_str()))
		{
		case PSL::PSLVM::FOPEN_ERROR:
			Msg::ErrorMessage("Cannot load config file.");
			return false;
		case PSL::PSLVM::PARSE_ERROR:
			Msg::ErrorMessage("Parse error in config file.");
			return false;
		}
		vm.run();
		config.from(vm);
		return true;
	}

	// カメラ番号0～5
	void setEyeCamUnit(int n)
	{
		eye2d(4.0f*n, -0.40f, 40.0f, -PI/2, 0.0f, 40);
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

	void replace(string& str, const string& from, const string& to)
	{
		for (;;)
		{
			string::size_type pos = str.find(from);
			if (pos==str.npos) return;
			str.replace(pos, from.length(), to, 0, to.length());
		}
	}

	void replaceVars(string& s, int intnum)
	{
		time_t timeval;
		time(&timeval);
		tm t;
		localtime_s(&t, &timeval);
		
		replace(s, "{desktop}",  mi::Core::getDesktopFolder());
		replace(s, "{gameid}",   game_id);
		replace(s, "{num}",      mi::Lib::to_s(intnum));
		replace(s, "{num00000}", mi::Lib::to_s_num0(intnum, 5));
		replace(s, "{yyyy}",     mi::Lib::to_s_num0(t.tm_year+1900, 4));
		replace(s, "{mm}",       mi::Lib::to_s_num0(t.tm_mon+1,     2));
		replace(s, "{dd}",       mi::Lib::to_s_num0(t.tm_mday,      2));
		replace(s, "{hh}",       mi::Lib::to_s_num0(t.tm_hour,      2));
	}

	void saveScreenShot(int frame_number)
	{
		int w,h;
		glfwGetWindowSize(&w, &h);

		FIBITMAP* bmp = FreeImage_Allocate(w,h,24);

		// バックバッファを読む
		glReadBuffer(GL_BACK);

		std::vector<RGBQUAD> vram;
		vram.resize(w*h);
		
		// バッファの内容を
		// bmpオブジェクトのピクセルデータが格納されている領域に直接コピーする。
		glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, vram.data());

		int addr = 0;
		for (int y=0; y<h; ++y)
		{
			for (int x=0; x<w; ++x)
			{
				RGBQUAD color;
				color.rgbRed   = vram[addr].rgbBlue;
				color.rgbGreen = vram[addr].rgbGreen;
				color.rgbBlue  = vram[addr].rgbRed;
				FreeImage_SetPixelColor(bmp, x, y, &color);
				++addr;
			}
		}

		string name = config.folder_format + config.png_format;
		replaceVars(name, frame_number);
		mi::Folder::createFolder(name, true);
		FreeImage_Save(FIF_PNG, bmp, (name).c_str());

		FreeImage_Unload(bmp);
	}

	void snap(const Dots& dots, int unit_index, int png_index)
	{
		saveScreenShot(png_index);
		Snap3D& sn = snap3d[unit_index];
		sn.exist = true;
		sn.frame_index = this->data.frame_index;
		sn.dots.copyValidDots(dots);
	}

	void saveObj(int save_file_num)
	{
		string obj_filename = config.folder_format + config.obj_format;
		replaceVars(obj_filename, save_file_num);

		File f;
		f.openForWrite(obj_filename);

		int face_base = 0;
		for (int i=0; i<6; ++i)
		{
			face_base += ObjWriter::outputTetra(
				config.obj_dot_size,
				f,
				snap3d[i].dots,
				i*4.0f,
				face_base);
		}
	}

	void runFrame()
	{
		onFrameBegin();
		onProcessMouse();
		onProcessKeyboard();
		processGraphics();
		if (data.output_picture)
		{
			if (data.frame_index % picture_interval==0)
			{
				saveScreenShot(data.frame_index);
			}
		}
		eye.updateCameraMove();
		processFrameIncrement();
		onFrameEnd();
	}

	void createObj(const Dots& dots)
	{
		const string desktop = mi::Core::getDesktopFolder();
		mi::File f;
		if (!f.openForWrite(desktop+"/st.obj"))
		{
			return;
		}

		static Dots inner_dots;
		inner_dots.init();
		MovieLib::createDots(inner_dots, dots, 0.0f);
		ObjWriter::create(output_dot_size, f, inner_dots);
	}

	void eye3d(float x, float y, float z, float h, float v)
	{
		eye.fast_set = (view2d_width==0);
		view2d_width = 0;
		eye.set(x,y,z,h,v);
	}

	void eye2d(float x, float y, float z, float h, float v, int w)
	{
		eye.fast_set = true;
		view2d_width = w;
		eye.set(x,y,z,h,v);
	}

	void incdec(bool dec_key, bool inc_key, int& val, int min, int max)
	{
		if (dec_key && val>min)
			--val;
		if (inc_key && val<max)
			++val;
	}

	void processGraphics()
	{
		VoxGrafix::global.dot_count   = 0;
		VoxGrafix::global.atari_count = 0;

		const auto x = config.sky_rgba;
		gl::ClearGraphics(x.r, x.g, x.b);
		display3dSectionPrepare();
		drawFieldGrid();
		display3d();
		display2dSectionPrepare();
		if (debug_show) display2d();
		onDisplay2D();
	}

	void drawFieldCube(float x_base)
	{
		// delta
		const float D = 0.0001f;

		glRGBA  gline_color(0.0f, 0.1f, 0.3f);
#if 0
		glRGBA   wall_color(0.92f, 0.85f, 0.88f, 0.25f);
		// 「向こう側」の壁
		glBegin(GL_QUADS);
			wall_color();
			glVertex3f(GROUND_LEFT +x_base+D,               D, -GROUND_FAR-D);
			glVertex3f(GROUND_RIGHT+x_base-D,               D, -GROUND_FAR-D);
			glVertex3f(GROUND_RIGHT+x_base-D, GROUND_HEIGHT-D, -GROUND_FAR-D);
			glVertex3f(GROUND_LEFT +x_base+D, GROUND_HEIGHT-D, -GROUND_FAR-D);
		glEnd();
#endif
		// 床の格子
		glLineWidth(0.5f);
		glBegin(GL_LINES);
			config.box_rgba();

			// 前後のライン
			for (double i=GROUND_LEFT; i<=GROUND_RIGHT; i+=0.5)
			{
				glVertex3d(i+x_base, 0, -GROUND_DEPTH);
				glVertex3d(i+x_base, 0,  0);
			}

			// 左右のライン
			for (double i=0; i<=GROUND_DEPTH; i+=GROUND_DEPTH/4)
			{
				glVertex3d(GROUND_LEFT +x_base, 0, -i);
				glVertex3d(GROUND_RIGHT+x_base, 0, -i);
			}
		glEnd();

		// Left and right box
		glLineWidth(1.0f);
		for (int i=0; i<2; ++i)
		{
			const float x = (i==0) ? GROUND_LEFT : GROUND_RIGHT;
			glBegin(GL_LINE_LOOP);
				glTranslatef(x_base, 0.0f, 0.0f);
				glVertex3f(x+x_base, GROUND_HEIGHT, -GROUND_NEAR);
				glVertex3f(x+x_base, GROUND_HEIGHT, -GROUND_FAR);
				glVertex3f(x+x_base,          0.0f, -GROUND_FAR);
				glVertex3f(x+x_base,          0.0f, -GROUND_NEAR);
			glEnd();
		}

		// Ceil wires
		glBegin(GL_LINES);
			glVertex3f(GROUND_LEFT +x_base, GROUND_HEIGHT, -GROUND_NEAR);
			glVertex3f(GROUND_RIGHT+x_base, GROUND_HEIGHT, -GROUND_NEAR);
			glVertex3f(GROUND_LEFT +x_base, GROUND_HEIGHT, -GROUND_FAR);
			glVertex3f(GROUND_RIGHT+x_base, GROUND_HEIGHT, -GROUND_FAR);
		glEnd();

		glBegin(GL_QUADS);
			glRGBA(0.25f, 1.00f, 0.25f, 0.25f)();
			glVertex3f(GROUND_LEFT +x_base, 0, -GROUND_NEAR);
			glVertex3f(GROUND_LEFT +x_base, 0, -GROUND_FAR);
			glVertex3f(GROUND_RIGHT+x_base, 0, -GROUND_FAR);
			glVertex3f(GROUND_RIGHT+x_base, 0, -GROUND_NEAR);
		glEnd();
	}

	void drawFieldGroundLarge()
	{
		const float D = 999.0f;
		const float x1 = -D;
		const float x2 = +D;
		const float z1 = -D;
		const float z2 = +D;
		const float y  = -0.002f;
		glBegin(GL_QUADS);
			config.ground_rgba();
			glVertex3f(x1, y, z1);
			glVertex3f(x2, y, z1);
			glVertex3f(x2, y, z2);
			glVertex3f(x1, y, z2);
		glEnd();
		glBegin(GL_QUADS);
			config.ground_rgba();
			glVertex3f(x1, y, 0);
			glVertex3f(x2, y, 0);
			glVertex3f(x2, -999.0f, 0);
			glVertex3f(x1, -999.0f, 0);
		glEnd();
	}

	void drawFieldGroundSmall()
	{
		const float x1 = -2.0f - 1;
		const float x2 = -2.0f + 1 + 4*6;
		const float z1 = -1.0f - GROUND_FAR;
		const float z2 = +1.0f;
		const float y  = -0.001f;
		glBegin(GL_QUADS);
			glRGBA(0.3f, 0.4f, 0.3f)();
			glVertex3f(x1, y, z1);
			glVertex3f(x2, y, z1);
			glVertex3f(x2, y, z2);
			glVertex3f(x1, y, z2);
		glEnd();
	}

	void drawFarSprite()
	{
		float x = 1.2f;
		float y = 1.2f;
		float x1 = x-0.5f;
		float x2 = x+0.5f;
		float y1 = y-0.5f;
		float y2 = y+0.5f;
		float z = -GROUND_FAR;
		glBegin(GL_QUADS);
			glRGBA(0.8f, 0.2f, 0.1f)();
			glVertex3f(x1, y1, z);
			glVertex3f(x2, y1, z);
			glVertex3f(x2, y2, z);
			glVertex3f(x1, y2, z);
		glEnd();
	}

	void drawFieldGrid()
	{
		drawFieldGroundLarge();
		drawFieldGroundSmall();

		for (int i=0; i<6; ++i)
		{
			drawFieldCube(4.0f * i);
		}

		//drawFarSprite();
	}

	void display2d()
	{
		glEnable(GL_LINE_SMOOTH);
#if 0
		glBegin(GL_TRIANGLE_STRIP);
			glRGBA(200,100,50)();
			glVertex2f(0.25, 0.25);
			glVertex2f(0.5, 0.25);
			glVertex2f(0.25, 0.5);
		glEnd();
#endif
	}

	void drawMovie(CamUnit& cam, float add_x, float add_z, glRGBA color)
	{
		VoxGrafix::DrawParam param;
		param.movie_inc  = 16;
		param.person_inc = 16;
		cam.mov.dot_size = 1.6f;
		const bool movie_is_end = VoxGrafix::DrawMovieFrame(
			cam.mov,
			param,
			data.frame_index,
			//mov.player_color_rgba,
			color,
			glRGBA(50,200,0, 80),
			"replay",
			VoxGrafix::DRAW_VOXELS_PERSON,
			cam.dots,
			add_x,
			add_z);
		if (!movie_is_end)
		{
			data.frame_index = 0;
			data.frame_auto = Data::NO_DIR;
			quitRequest();
		}

		float avg_x = 0.0f;
		float avg_y = 0.0f;
		float avg_z = 0.0f;
		int count = 0;
		const auto& dots = cam.dots;
		for (int i=0; i<dots.length(); ++i)
		{
			Point3D p = dots[i];
			if (p.x>=ATARI_LEFT   && p.x<=ATARI_RIGHT
			 && p.y>=ATARI_BOTTOM && p.y<=ATARI_TOP
			 && p.z>=ATARI_NEAR   && p.z<=ATARI_FAR)
			{
				++count;
				avg_x += p.x;
				avg_y += p.y;
				avg_z += p.z;
			}
		}

		// "bar noise" dots are 1000-1500 voxels
		cam.valid_dots = count;
		if (count>=1500)
		{
			cam.center = Point3D(avg_x/count, avg_y/count, avg_z/count);
		}
		else
		{
			cam.center = InvalidPoint3D();
		}
	}

	void display3d()
	{
		glRGBA colors[6]={
			config.body1_rgba,
			config.body2_rgba,
			config.body3_rgba,
			config.body4_rgba,
			config.body5_rgba,
			config.body6_rgba,
			};
		for (size_t j=0; j<camsys.size(); ++j)
		{
			float add_z = 0.0f;
			switch (camsys.size())
			{
			case 2:  add_z=config.diff_2movies[j];  break;
			case 3:  add_z=config.diff_3movies[j];  break;
			case 4:  add_z=config.diff_4movies[j];  break;
			}
			for (int i=0; i<6; ++i)
			{
				drawMovie(camsys[j].cams[i], i*4, add_z, colors[i]);
			}
		}
	}

	void display2dSectionPrepare()
	{
		gl::Projection();
		gl::LoadIdentity();
		glOrtho(0, 640, 480, 0, -50.0, 50.0);

		gl::Texture(false);
		gl::DepthTest(false);
	}

	void display3dSectionPrepare()
	{
		// PROJECTION
		gl::Projection();
		gl::LoadIdentity();

		if (view2d_width==0)
		{
			gluPerspective(30.0f, 4.0f/3.0f, 1.0f, 100.0f);
		}
		else
		{
			const double w = view2d_width/10.0f;
			glOrtho(-w/2, +w/2, 0, (w*3/4), -3.0, +120.0);
		}
		eye.gluLookAt();

		// MODEL
		gl::Texture(false);
		gl::DepthTest(true);
		gl::ModelView();
		gl::LoadIdentity();
	}

	// from: "C:/Folder/AAAAAA-1.stmov"
	//   to: "C:/Folder/AAAAAA"
	static string getBaseName(const char* _s)
	{
		string s(_s);

		// erase "-1.stmov$"
		return s.substr(0, s.length()-8);
	}
};
