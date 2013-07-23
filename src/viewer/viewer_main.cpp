#define THIS_IS_MAIN
#include "../St3dData.h"
#include "../gl_funcs.h"
#include "../FreeType.h"
#include "../mi/Timer.h"
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
	MovieData mov;
	EyeCore eye;
	freetype::font_data font;
	int frame;
	Dots* dots_original;
	float output_dot_size;

	ViewerApp()
	{
		frame = 0;
		output_dot_size = 1.0f;
	}

	void init()
	{
		font.init("C:/Windows/Fonts/Cour.ttf", 12);
		eye.set(-4.2f, 1.5f, 5.4f, -1.0f, -0.2f);
		eye.fast_set = false;
	}

	void load(const string& basename)
	{
		mov.load(basename + "-1.stmov");
	}

	struct Data
	{
		enum FrameDir
		{
			NO_DIR,
			INCR,
			DECR,
		};
		FrameDir frame_auto;
		int frame_index;

		Data()
		{
			frame_auto = NO_DIR;
			frame_index = 0;
		}
	} data;

	void runFrame()
	{
		processUserInput();
		processGraphics();

		eye.updateCameraMove();

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

	void processUserInput()
	{
		bool kbd[256] = {};
		{
			BYTE _kbd[256] = {};
			GetKeyboardState(_kbd);
		
			for (int i=0; i<256; ++i)
			{
				const BYTE KON = 0x80;
				kbd[i] = (_kbd[i] & KON)!=0;
			}
		}

		if (kbd['1'])
		{
			eye.set(-4.2f, 1.5f, 5.4f, -1.0f, -0.2f);
		}
		if (kbd['2'])
		{
			eye.set(4.2f, 1.5f, 5.4f, -2.1f, -0.2f);
		}

		const float mv = 0.1f;
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
		if (kbd['Q'])  { eye.rh -= mr; move( 90); }
		if (kbd['E'])  { eye.rh += mr; move(270); }
		if (kbd['R'])  { eye.y += mv; eye.v -= mv; }
		if (kbd['F'])  { eye.y -= mv; eye.v += mv; }

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
		if (kbd[VK_F3])
		{
			data.frame_auto = Data::NO_DIR;
			data.frame_index = 0;
		}
		if (kbd[VK_F4])
		{
			data.frame_auto = Data::DECR;
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
		display2d();
	}

	void drawFieldGrid()
	{
		glBegin(GL_LINES);
		const float F = 400/100.0f;

		glRGBA line_color(0.8f, 0.8f, 0.8f);
		glRGBA center_color(0.9f, 0.3f, 0.2f);

		line_color();
		glLineWidth(1.0f);

		// 前後のライン
		for (double i=GROUND_LEFT; i<=GROUND_RIGHT; i+=0.5)
		{
			glVertex3d(i, 0, -GROUND_DEPTH);
			glVertex3d(i, 0,  0);
		}

		// 左右のライン
		for (double i=0; i<=GROUND_DEPTH; i+=GROUND_DEPTH/4)
		{
			glVertex3d(GROUND_LEFT,  0, -i);
			glVertex3d(GROUND_RIGHT, 0, -i);
		}

		glEnd();

		line_color();
		// Left and right box
		for (int i=0; i<2; ++i)
		{
			const float x = (i==0) ? GROUND_LEFT : GROUND_RIGHT;
			glBegin(GL_LINE_LOOP);
				glVertex3f(x, GROUND_HEIGHT, -GROUND_NEAR);
				glVertex3f(x, GROUND_HEIGHT, -GROUND_FAR);
				glVertex3f(x,          0.0f, -GROUND_FAR);
				glVertex3f(x,          0.0f, -GROUND_NEAR);
			glEnd();
		}

		// Ceil bar
		glBegin(GL_LINES);
			glVertex3f(GROUND_LEFT,  GROUND_HEIGHT, -GROUND_NEAR);
			glVertex3f(GROUND_RIGHT, GROUND_HEIGHT, -GROUND_NEAR);
			glVertex3f(GROUND_LEFT,  GROUND_HEIGHT, -GROUND_FAR);
			glVertex3f(GROUND_RIGHT, GROUND_HEIGHT, -GROUND_FAR);
		glEnd();



		// run space, @green
#if 0
		glRGBA(0.25f, 1.00f, 0.25f, 0.25f).glColorUpdate();
		glBegin(GL_QUADS);
			glVertex3f(GROUND_LEFT,  0, -GROUND_NEAR);
			glVertex3f(GROUND_LEFT,  0, -GROUND_FAR);
			glVertex3f(GROUND_RIGHT, 0, -GROUND_FAR);
			glVertex3f(GROUND_RIGHT, 0, -GROUND_NEAR);
		glEnd();
#endif
	}

	void display2d()
	{
		glRGBA(200,100,50)();
		glBegin(GL_TRIANGLE_STRIP);
			glVertex2f(0.25, 0.25);
			glVertex2f(0.5, 0.25);
			glVertex2f(0.25, 0.5);
		glEnd();

		const int h = 20;
		int y = 0;
		glRGBA(0,0,0)();
		++frame;
		freetype::print(font, 10,y+=h,
			"%d frames, %d sec",
			data.frame_index,
			data.frame_index/30);

		freetype::print(font, 10,y+=h,
			"total dots = %d",
			VoxGrafix::global.dot_count);

		freetype::print(font, 10,y+=h,
			"eye={x:%+5.1f,y:%+5.1f,z:%+5.1f,rh:%+5.1f} [a/s/d/w]",
			eye.x,
			eye.y,
			eye.z,
			eye.rh);

		freetype::print(font, 10,y+=h,
			"output dot size = %.1f [n/m]",
			output_dot_size);

		auto pr = [&](const char* s){
			freetype::print(font, 10,y+=h, s);
		};
		
		pr("[F1] play");
		pr("[F2] pause");
		pr("[F3] rewind");
		pr("[F4] reverse");
		pr("[1]  preset cam 1");
		pr("[2]  preset cam 2");
		pr("[F8] save obj file");
		pr("[Sp] next frame");
		pr("[Z]  next frame");
		pr("[X]  prev frame");
	}

	void display3d()
	{
		VoxGrafix::DrawParam param;
		param.movie_inc = 2500;
		param.person_inc = 500;
		mov.dot_size = 8.0f;

		this->dots_original = nullptr;
		const bool res = VoxGrafix::DrawMovieFrame(
			mov,
			param,
			data.frame_index,
			mov.player_color_rgba,
			glRGBA(50,200,0, 80),
			"replay",
			VoxGrafix::DRAW_VOXELS_PERSON,
			+0.0f,
			&this->dots_original);
		if (!res)
		{
			data.frame_index = 0;
			data.frame_auto = Data::NO_DIR;
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

		gluPerspective(30.0f, 4.0f/3.0f, 1.0f, 100.0f);
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

static bool run_app(const char* arg)
{
	AppCore::initGraphics(false, "ST 3D Viewer");

	ViewerApp app;
	app.init();
	app.load(getBaseName(arg));

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
