#define THIS_IS_MAIN
#include "../ViewerAppBase.h"


class ViewerApp : public ViewerAppBase
{
public:
	std::map<int,bool> tookPicture;
	std::map<int,int> unitShown;

	bool isPictureTake(float x, int n)
	{
		switch (n)
		{
		default: return false;
		case 0:  return (frame==350);
		case 1:  return (x>=0.00f);
		case 2:  return (x>=0.00f);
		case 3:  return (x>=0.00f);
		case 4:  return (x>=0.00f);
		case 5:  return (x>=0.50f);
		}
	}

	// ÉJÉÅÉâî‘çÜ0Å`5
	void eyeCamUnit(int n)
	{
		eye2d(4.0f*n, -0.40f, 40.0f, -PI/2, 0.0f, 40);
	}

	void onInit()
	{
		eyeCamUnit(1);
		for (int i=0; i<6; ++i)
		{
			tookPicture[i] = false;
			unitShown[i] = 0;
		}
	}

	void onFrameBegin()
	{
		for (int i=0; i<6; ++i)
		{
			const auto& cam = cams[i];
			if (cam.center==InvalidPoint3D())
				continue;

			++unitShown[i];

			volatile float cx = cam.center.x;
			if (unitShown[i]>=5 && cx>=-2.0f)
			{
				eyeCamUnit(i);
				break;
			}
		}
	}

	void onFrameEnd()
	{
		for (int i=0; i<6; ++i)
		{
			const auto& cam = cams[i];
			if (cam.center==InvalidPoint3D())
				continue;

			volatile float cx = cam.center.x;
			if (unitShown[i]>=5 && tookPicture[i]==false && isPictureTake(cx,i))
			{
				tookPicture[i] = true;
				saveScreenShot(i);
				break;
			}
		}
	}

	void onProcessMouse()
	{
	}

	void onProcessKeyboard()
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

		if (kbd['1'])  { eyeCamUnit(0); }
		if (kbd['2'])  { eyeCamUnit(1); }
		if (kbd['3'])  { eyeCamUnit(2); }
		if (kbd['4'])  { eyeCamUnit(3); }
		if (kbd['5'])  { eyeCamUnit(4); }
		if (kbd['6'])  { eyeCamUnit(5); }

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
	return true;
}

int main(int argc, const char* argv[])
{
	return run_app(argc>=2 ? argv[1] : "") ? EXIT_SUCCESS : EXIT_FAILURE;
}
