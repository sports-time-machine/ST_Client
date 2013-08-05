#define THIS_IS_MAIN
#include "../St3dData.h"
#include "../gl_funcs.h"
#include "../FreeType.h"
#include "../mi/Timer.h"
#include "../mi/Libs.h"
#include <FreeImage.h>
#include <direct.h>
#include "../MovieLib.h"
#pragma warning(disable:4244)
#include "../ViewerAppBase.h"

template<typename T> T minmax(T val, T min, T max)
{
	return (val<min) ? min : (val>max) ? max : val;
}

struct Point2D
{
	int x,y;
	Point2D() {}
	Point2D(int x, int y)            { set(x,y); }
	void set(int x, int y)           { this->x = x; this->y = y; }
	bool operator==(Point2D a) const { return x==a.x && y==a.y; }
	bool operator!=(Point2D a) const { return !operator==(a); }
};

class ViewerApp : public ViewerAppBase
{
public:
	ViewerApp()
	{
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
	struct MouseData
	{
		float rh_first;
		float v_first;
	} md;
	Point2D mleft_pos, mright_pos;

	void processUserMouseInput()
	{
		mleft .update(glfwGetMouseButton(GLFW_MOUSE_BUTTON_LEFT )==GLFW_PRESS);
		mright.update(glfwGetMouseButton(GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS);

		Point2D mpos;
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
};


Point2D glfwGetWindowSize()
{
	int w,h;
	glfwGetWindowSize(&w,&h);
	return Point2D(w,h);
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

		app.load(ViewerAppBase::getBaseName(arg.c_str()));
	}

	Point2D win_size = glfwGetWindowSize();

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

		Point2D tmp = glfwGetWindowSize();
		if (win_size!=tmp)
		{
			win_size = tmp;
			glViewport(0, 0, win_size.x, win_size.y);
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
