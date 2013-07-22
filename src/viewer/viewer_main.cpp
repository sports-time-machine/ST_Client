#define THIS_IS_MAIN
#include "../St3dData.h"
#include "../gl_funcs.h"
#include "../FreeType.h"
#include "../mi/Timer.h"

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

	ViewerApp()
	{
		frame = 0;
	}

	void init()
	{
		font.init("C:/Windows/Fonts/Cour.ttf", 12);
		eye.set(0.0f, 1.5f, 9.50f, -PI/2+0.25f, -0.60f);
		eye.fast_set = false;
	}

	void load(const string& basename)
	{
		mov.load(basename + "-1.stmov");
	}

	struct Data
	{
		bool frame_auto_incr;
		int frame_index;

		Data()
		{
			frame_auto_incr = true;
			frame_index = 0;
		}
	} data;

	void runFrame()
	{
		processUserInput();
		processGraphics();

		eye.updateCameraMove();

		if (data.frame_auto_incr)
		{
			++data.frame_index;
		}
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
		
		if (kbd[VK_F1])
		{
			if (!data.frame_auto_incr)
			{
				data.frame_index = 0;
				data.frame_auto_incr = true;
			}
			else
			{
				data.frame_index = 0;
				data.frame_auto_incr = false;
			}
		}

		if (kbd['1'])
		{
			eye.set(-2.9f, 1.5f, 3.6f, -1.03f, -0.0f);
		}
		if (kbd['2'])
		{
			eye.set(2.9f, 1.5f, 3.6f, -2.11f, -0.0f);
		}

		const float mv = 0.1f;
		const float mr = 0.01f;

		auto move = [&](int r){
			eye.x += cosf(eye.rh + r*PI/180) * mv;
			eye.z += sinf(eye.rh + r*PI/180) * mv;
		};

		if (kbd['A'])  move(270);
		if (kbd['D'])  move(90);
		if (kbd['W'])  move(0);
		if (kbd['S'])  move(180);
		if (kbd['Q'])  eye.rh -= mr;
		if (kbd['E'])  eye.rh += mr;

		if (kbd['L'])
		{
			load("C:/000Yamaguchi/00000AVYV0");
		}
	}

	void processGraphics()
	{
		gl::ClearGraphics(120,160,200);
		display3dSectionPrepare();
		drawFieldGrid(500);
		display3d();
		display2dSectionPrepare();
		display2d();
	}

	void drawFieldGrid(int size_cm)
	{
		glBegin(GL_LINES);
		const float F = size_cm/100.0f;

		glRGBA line_color(0.8f, 0.8f, 0.8f);
		glRGBA center_color(0.9f, 0.3f, 0.2f);

		glLineWidth(1.0f);
		for (int i=-size_cm/2; i<size_cm/2; i+=50)
		{
			const float f = i/100.0f;

			if (i==0)
			{
				center_color();
			}
			else
			{
				line_color();
			}

			glVertex3f(-F, 0, f);
			glVertex3f(+F, 0, f);

			glVertex3f( f, 0, -F);
			glVertex3f( f, 0, +F);
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

		freetype::print(font, 10,10, "%d", frame++);
	}

	void display3d()
	{
		VoxGrafix::DrawParam param;
		param.movie_inc = 500;
		param.person_inc = 500;
		param.dot_size = 1.0f;

		const bool res = VoxGrafix::DrawMovieFrame(
			mov,
			param,
			data.frame_index,
			mov.player_color_rgba,
			glRGBA(50,200,0, 80),
			"replay",
			VoxGrafix::DRAW_VOXELS_PERSON,
			+0.0f);
		if (!res)
		{
			data.frame_index = 0;
			data.frame_auto_incr = false;
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

static bool run_app()
{
	AppCore::initGraphics(false, "ST 3D Viewer");

	ViewerApp app;
	app.init();

	glfwSwapInterval(1);
	double prev_msec = 0.0;
	while (glfwGetWindowParam(GLFW_OPENED))
	{
		const double curr_msec = mi::Timer::getMsec();

		double sleep_msec = (prev_msec + 16.0) - curr_msec;
		if (sleep_msec>0)
		{
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

int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int)
{
	return run_app() ? EXIT_SUCCESS : EXIT_FAILURE;
}
