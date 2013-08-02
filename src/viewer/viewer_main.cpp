#define THIS_IS_MAIN
#include "../St3dData.h"
#include "../gl_funcs.h"
#include "../FreeType.h"
#include "../mi/Timer.h"
#include "../mi/Libs.h"
#include <FreeImage.h>
#include <direct.h>
#pragma warning(disable:4244)

template<typename T> T minmax(T val, T min, T max)
{
	return (val<min) ? min : (val>max) ? max : val;
}

static const float PI = 3.1415f;

using namespace mi;
using namespace stclient;
using namespace mgl;

class ViewerApp
{
public:
	std::map<int,MovieData> mov;
	freetype::font_data font;
	EyeCore    eye;
	int        frame;
	Dots*      dots_original;
	float      output_dot_size;
	int        view2d_width;
	int        picture_interval;
	bool       debug_show;

	ViewerApp()
	{
		frame = 0;
		output_dot_size = 1.0f;
		view2d_width = 0;
		picture_interval = 1;
		debug_show = true;
	}

	void init()
	{
		font.init("C:/Windows/Fonts/Cour.ttf", 12);
		eye.set(-4.2f, 1.5f, 5.4f, -1.0f, -0.2f);
		eye.setTransitionTime(80);
		eye.fast_set = false;
		view2d_width = 0;
	}

	static void DenugPrintln(const char* s)
	{
		OutputDebugString(s);
		OutputDebugString("\n");
	}

	void load(const string& basename)
	{
		DenugPrintln("load");
		DenugPrintln(basename.c_str());
		for (int i=0; i<6; ++i)
		{
			if (!mov[i].load(basename+"-"+mi::Lib::to_s(1+i)+".stmov"))
			{
				DenugPrintln("load failed");
			}
		}
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

	void saveScreenShot(int frame_number)
	{
		int w,h;
		glfwGetWindowSize(&w, &h);

		FIBITMAP* bmp = FreeImage_Allocate(w,h,24);

		// バックバッファを読む
		glReadBuffer(GL_BACK);

		std::vector<RGBQUAD> vram;
		vram.resize(640*480);
		
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

		FreeImage_Save(FIF_PNG, bmp, filename.c_str());

		FreeImage_Unload(bmp);
	}

	void runFrame()
	{
		processUserInput();
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
	}

	void createDots(Dots& dest, const Dots& src)
	{
		for (int i=0; i<src.size(); ++i)
		{
			const Point3D& po = src[i];
			const float x = po.x;
			const float y = po.y;
			const float z = po.z;
			const bool in_x = (x>=GROUND_LEFT && x<=GROUND_RIGHT);
			const bool in_y = (y>=0.0f && y<=GROUND_HEIGHT);
			const bool in_z = (z>=0.0f && z<=GROUND_DEPTH);

			// out of box
			if (!(in_x && in_y && in_z))
				continue;

			dest.push(po);
		}
	}

	void outputDots(mi::File& f, const Dots& dots)
	{
		char buffer[1000];
		const float SZ = 0.0025f * output_dot_size;
		for (int i=0; i<dots.size(); ++i)
		{
			const auto& dot = dots[i];

			auto wr_vec = [&](const int x, const int y, const int z){
				const int len = sprintf_s(buffer, "v %f %f %f\n", dot.x + SZ*x, dot.y + SZ*y, dot.z + SZ*z);
				f.write(buffer, len);
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
				const int len = sprintf_s(buffer, "f %d %d %d %d\n", N+a, N+b, N+c, N+d);
				f.write(buffer, len);
			};

			wr_face(1,3,4,2);
			wr_face(1,5,7,3);
			wr_face(2,4,8,6);
			wr_face(1,2,6,5);
			wr_face(3,7,8,4);
			wr_face(5,6,8,7);
		}
	}

	void createObj()
	{
		const string desktop = mi::Core::getDesktopFolder();
		mi::File f;
		if (!f.openForWrite(desktop+"/st.obj"))
		{
			return;
		}

		static Dots dots;
		dots.init();
		createDots(dots, *dots_original);
		outputDots(f, dots);
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

	void processUserInput()
	{
		processUserKeyInput();
		processUserMouseInput();
	}

	struct MouseButton
	{
		bool prev,curr;
		MouseButton()
		{
			prev = false;
			curr = false;
		}
		void update(bool new_state)
		{
			prev = curr;
			curr = new_state;
		}
		bool pressed() const
		{
			return !prev && curr;
		}
		operator bool() const
		{
			return curr;
		}
	};

	MouseButton mleft,mright;
	struct Point
	{
		int x,y;
		void set(int x, int y)
		{
			this->x = x;
			this->y = y;
		}
	};
	struct MouseData
	{
		float rh_first;
		float v_first;
	} md;
	Point mleft_pos, mright_pos;

	void processUserMouseInput()
	{
		mleft .update(glfwGetMouseButton(GLFW_MOUSE_BUTTON_LEFT )==GLFW_PRESS);
		mright.update(glfwGetMouseButton(GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS);

		Point mpos;
		{
			int mx,my;
			glfwGetMousePos(&mx, &my);
			mpos.set(mx,my);
		}

		{
			if (mleft.pressed())   { mleft_pos  = mpos; md.rh_first=eye.rh; md.v_first=eye.v; }
			if (mright.pressed())  { mright_pos = mpos; }
		}

		if (mleft)
		{
			eye.rh = md.rh_first + (mleft_pos.x - mpos.x) * 0.0025f;
			eye.v  = md. v_first - (mleft_pos.y - mpos.y) * 0.0050f;
		}
		if (mright)
		{
			eye.x -= cosf(eye.rh     ) * (mright_pos.y - mpos.y) * 0.025f;
			eye.z -= sinf(eye.rh     ) * (mright_pos.y - mpos.y) * 0.025f;
			eye.x += cosf(eye.rh+PI/2) * (mright_pos.x - mpos.x) * 0.025f;
			eye.z += sinf(eye.rh+PI/2) * (mright_pos.x - mpos.x) * 0.025f;
			mright_pos = mpos;
		}
	}

	void incdec(bool dec_key, bool inc_key, int& val, int min, int max)
	{
		if (dec_key && val>min)
			--val;
		if (inc_key && val<max)
			++val;
	}

	void processUserKeyInput()
	{
		static bool prev[256];
		static bool kbd[256];
		{
			BYTE _kbd[256] = {};
			GetKeyboardState(_kbd);
		
			for (int i=0; i<256; ++i)
			{
				const BYTE KON = 0x80;
				prev[i] = kbd[i];
				kbd[i] = (_kbd[i] & KON)!=0;
			}
		}


		const float PI = 3.1415923f;
		if (kbd['1'])  { eye3d(-4.2f, 1.5f, 5.4f, -1.0f, -0.2f); }
		if (kbd['2'])  { eye3d(4.2f, 1.5f, 5.4f, -2.1f, -0.2f); }
		if (kbd['3'])  { eye3d(-4.4f, +16.5f, +17.9f, -1.0f, -2.6f); }
		if (kbd['4'])  { eye3d(-10.8f, 7.0f, 6.3f, -0.5f, -1.6f); }
		if (kbd['5'])  { eye3d(29.9f, 7.0f, 7.9f, -2.6f, -1.6f); }
		if (kbd['6'])  { eye3d(-7.1f, 1.5f, 1.7f, -0.4f, -0.2f); }
		if (kbd['9'])  { eye2d(10.0f, 3.8f, 60.7f, -PI/2, -0.8f, 240); }
		if (kbd['0'])  { eye2d(-25.3f, 4.1f, 20.5f, -0.55f, -0.85f, 150); }

		const float mv = 0.100f;
		const float mr = 0.025f;

		auto move = [&](int r){
			eye.x += cosf(eye.rh + r*PI/180) * mv;
			eye.z += sinf(eye.rh + r*PI/180) * mv;
		};

		if (kbd[' '] || kbd['Z'])  { data.frame_auto=Data::NO_DIR; ++data.frame_index; }
		if (kbd['X'])  { data.frame_auto=Data::NO_DIR; if (--data.frame_index<0){data.frame_index=0;} }
		if (kbd['S'])  move(180);
		if (kbd['W'])  move(0);
		if (kbd['A'])  move(270);
		if (kbd['D'])  move(90);
		if (kbd['Q'])  { eye.rh += mr; move(270); }
		if (kbd['E'])  { eye.rh -= mr; move( 90); }
		if (kbd['R'])  { eye.y += mv; eye.v -= mr; }
		if (kbd['F'])  { eye.y -= mv; eye.v += mr; }
		if (kbd[VK_F11] && !prev[VK_F11]) { debug_show = !debug_show; }

		incdec(
			kbd['O'],
			kbd['L'],
			view2d_width, 1, 9999);
		incdec(
			kbd['I']&&!prev['I'],
			kbd['K']&&!prev['K'],
			picture_interval, 1, 30);

		if (kbd['N'])  output_dot_size = minmax(output_dot_size-0.1f, 0.1f, 10.0f);
		if (kbd['M'])  output_dot_size = minmax(output_dot_size+0.1f, 0.1f, 10.0f);

		if (kbd[VK_F1])
		{
			data.frame_auto = Data::INCR;
		}
		if (kbd[VK_F2])
		{
			data.frame_auto = Data::NO_DIR;
		}
		if (kbd[VK_F3])//rewind
		{
			data.frame_auto = Data::NO_DIR;
			data.frame_index = 0;
			data.output_picture = false;
		}
		if (kbd[VK_F4])
		{
			data.frame_auto = Data::DECR;
		}
		if (kbd[VK_F7])//play with output
		{
			data.frame_auto = Data::INCR;
			data.output_picture = true;
			data.frame_index = 0;
		}

		if (kbd[VK_F8])
		{
			createObj();
		}
	}

	void processGraphics()
	{
		VoxGrafix::global.dot_count   = 0;
		VoxGrafix::global.atari_count = 0;

		gl::ClearGraphics(120,160,200);
		display3dSectionPrepare();
		drawFieldGrid();
		display3d();
		display2dSectionPrepare();
		if (debug_show) display2d();
	}

	void drawFieldCube(float x_base)
	{
		// delta
		const float D = 0.0001f;

		glRGBA  gline_color(0.0f, 0.1f, 0.3f);
		glRGBA center_color(0.9f, 0.3f, 0.2f);
		glRGBA   wall_color(0.92f, 0.85f, 0.88f, 0.25f);
#if 0
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
			gline_color();

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
			glRGBA(0.3f, 0.2f, 0.2f)();
			glVertex3f(x1, y, z1);
			glVertex3f(x2, y, z1);
			glVertex3f(x2, y, z2);
			glVertex3f(x1, y, z2);
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
		const int h = 20;
		int y = 0;
		glRGBA(60,30,0)();
		++frame;
		freetype::print(font, 10,y+=h,
			"%d frames, %d sec",
			data.frame_index,
			data.frame_index/30);
		freetype::print(font, 10,y+=h,
			"total dots = %d",
			VoxGrafix::global.dot_count);
		freetype::print(font, 10,y+=h,
			"eye={x:%.1f,y:%.1f,z:%.1f,rh:%.2f,%.2f} [a/s/d/w]",
			eye.x,
			eye.y,
			eye.z,
			eye.rh,
			eye.v);
		freetype::print(font, 10,y+=h,
			"view_width=%.1f [o/l]",
			view2d_width/10.0f);
		freetype::print(font, 10,y+=h,
			"picture_interval=%d frame/picture [i/k]",
			picture_interval);
		freetype::print(font, 10,y+=h,
			"output dot size = %.1f [n/m]",
			output_dot_size);

		auto pr = [&](const char* s){
			freetype::print(font, 10,y+=h, s);
		};
		
		pr("[F1]  play");
		pr("[F2]  pause");
		pr("[F3]  rewind");
		pr("[F4]  reverse");
		pr("[F7]  play with save pictures");
		pr("[F8]  save .obj");
		pr("[1-6] preset cam (2D)");
		pr("[9,0] preset cam (3D)");
		pr("[spc] next frame");
		pr("[Z]   next frame");
		pr("[X]   prev frame");
	}

	void drawMovie(MovieData& mov, float add_x, glRGBA color)
	{
		VoxGrafix::DrawParam param;
		param.movie_inc  = 16;
		param.person_inc = 16;
		this->dots_original = nullptr;
		mov.dot_size = 1.6f;
		const bool res = VoxGrafix::DrawMovieFrame(
			mov,
			param,
			data.frame_index,
			//mov.player_color_rgba,
			color,
			glRGBA(50,200,0, 80),
			"replay",
			VoxGrafix::DRAW_VOXELS_PERSON,
			add_x,
			&this->dots_original);
		if (!res)
		{
			data.frame_index = 0;
			data.frame_auto = Data::NO_DIR;
		}
	}

	void display3d()
	{
		int a = 230;
		int x = 180;
		glRGBA colors[6];
		colors[0].set(a,x,x, 160);
		colors[1].set(x,a,x, 160);
		colors[2].set(x,x,a, 160);
		colors[3].set(a,a,x, 160);
		colors[4].set(a,x,a, 160);
		colors[5].set(a,a,x, 160);

		for (int i=0; i<6; ++i)
		{
			drawMovie(mov[i], i*4, colors[i]);
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
};

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
	AppCore::initGraphics(false, "ST 3D Viewer");
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	glHint(GL_LINE_SMOOTH_HINT,    GL_NICEST);

	ViewerApp app;
	app.init();
	if (arg.length()>0)
	{
		if (arg[0]=='"')
		{
			arg = arg.substr(1, arg.length()-2);
		}

		app.load(getBaseName(arg.c_str()));
	}

	int win_w,win_h;
	glfwGetWindowSize(&win_w, &win_h);

	glfwSwapInterval(1);
	double prev_msec = 0.0;
	while (glfwGetWindowParam(GLFW_OPENED))
	{
		const double curr_msec = mi::Timer::getMsec();

		double sleep_msec = (prev_msec + 1000/30.0f) - curr_msec;
		if (sleep_msec>0)
		{
			printf("%d\n", (int)sleep_msec);
			Sleep((int)sleep_msec);
		}
		prev_msec = curr_msec;

		{
			int w,h;
			glfwGetWindowSize(&w, &h);
			if (!(win_w==w && win_h==h))
			{
				win_w = w;
				win_h = h;
				glViewport(0, 0, win_w, win_h);
			}
		}

		app.runFrame();
		glfwSwapBuffers();
	}
	glfwTerminate();
	exit(1);

	return true;
}

int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR arg,int)
{
	return run_app(arg) ? EXIT_SUCCESS : EXIT_FAILURE;
}
