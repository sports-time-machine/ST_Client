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

class ViewerApp : public ViewerAppBase
{
public:
	enum AppMode
	{
		MODE_VIEW,
		MODE_SNAP,
		MODE_ONESHOT,
	};

	AppMode  appmode;
	int      snapshot_cam_index;

	ViewerApp()
	{
		appmode = MODE_VIEW;
		snapshot_cam_index = 0;
	}

	bool onInit()
	{
		if (!loadConfigPsl())
			return false;
		return true;
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

	void onProcessMouse()
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

	void snapOne(int unit_index)
	{
		this->snap(camsys[0].cams[unit_index].dots, unit_index, unit_index+1);
	}

	class KeyState
	{
	public:
		bool on(int n) const          { return kbd[n]; }
		bool operator()(int n) const  { return kbd[n] && !prev[n]; }

		void update()
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
	private:
		bool kbd[256];
		bool prev[256];
	};

	KeyState keys;

	void onProcessKeyboard_Common()
	{
		if (keys.on('X') || keys(VK_RIGHT) || keys.on(VK_NEXT))
		{
			data.frame_auto=Data::NO_DIR;
			++data.frame_index;
		}
		if (keys.on('Z') || keys(VK_LEFT) || keys.on(VK_PRIOR))
		{
			data.frame_auto=Data::NO_DIR;
			if (--data.frame_index<0){data.frame_index=0;}
		}
	}

	void onProcessKeyboard_Viewer()
	{
		if (keys('1'))  { eye3d(-4.2f, 1.5f, 5.4f, -1.0f, -0.2f); }
		if (keys('2'))  { eye3d(4.2f, 1.5f, 5.4f, -2.1f, -0.2f); }
		if (keys('3'))  { eye3d(-4.4f, +16.5f, +17.9f, -1.0f, -2.6f); }
		if (keys('4'))  { eye3d(-10.8f, 7.0f, 6.3f, -0.5f, -1.6f); }
		if (keys('5'))  { eye3d(29.9f, 7.0f, 7.9f, -2.6f, -1.6f); }
		if (keys('6'))  { eye3d(-7.1f, 1.5f, 1.7f, -0.4f, -0.2f); }
		if (keys('9'))  { eye2d(10.0f, 3.8f, 60.7f, -PI/2, -0.8f, 240); }
		if (keys('0'))  { eye2d(-25.3f, 4.1f, 20.5f, -0.55f, -0.85f, 150); }

		const float mv = 0.100f;
		const float mr = 0.025f;

		auto move = [&](int r){
			eye.x += cosf(eye.rh + r*PI/180) * mv;
			eye.z += sinf(eye.rh + r*PI/180) * mv;
		};

		if (keys(' '))  { data.frame_auto=Data::NO_DIR; ++data.frame_index; }
		if (keys.on('S'))  move(180);
		if (keys.on('W'))  move(0);
		if (keys.on('A'))  move(270);
		if (keys.on('D'))  move(90);
		if (keys.on('Q'))  { eye.rh += mr; move(270); }
		if (keys.on('E'))  { eye.rh -= mr; move( 90); }
		if (keys.on('R'))  { eye.y += mv; eye.v -= mr; }
		if (keys.on('F'))  { eye.y -= mv; eye.v += mr; }
		if (keys(VK_F11)) { debug_show = !debug_show; }
		incdec(
			keys.on('O'),
			keys.on('L'),
			view2d_width, 1, 9999);
		incdec(
			keys('I'),
			keys('K'),
			picture_interval, 1, 30);

		if (keys('N'))  output_dot_size = minmax(output_dot_size-0.1f, 0.1f, 10.0f);
		if (keys('M'))  output_dot_size = minmax(output_dot_size+0.1f, 0.1f, 10.0f);

		if (keys(VK_F1))
		{
			data.frame_auto = Data::INCR;
		}
		if (keys(VK_F2))
		{
			data.frame_auto = Data::NO_DIR;
		}
		if (keys(VK_F3))//rewind
		{
			data.frame_auto = Data::NO_DIR;
			data.frame_index = 0;
			data.output_picture = false;
		}
		if (keys(VK_F4))
		{
			data.frame_auto = Data::DECR;
		}
		if (keys(VK_F6))//play with output
		{
			data.frame_auto = Data::INCR;
			data.output_picture = true;
			data.frame_index = 0;
		}
	}

	void onProcessKeyboard_Snapshot()
	{
		auto select = [&](int n){
			setEyeCamUnit(n);
			snapshot_cam_index = n;
		};

		if (keys('1'))  { select(0); }
		if (keys('2'))  { select(1); }
		if (keys('3'))  { select(2); }
		if (keys('4'))  { select(3); }
		if (keys('5'))  { select(4); }
		if (keys('6'))  { select(5); }

		if (keys(' '))
		{
			appmode = MODE_ONESHOT;
		}
		if (keys('O'))
		{
			this->saveObj(1);
		}
		if (keys('P'))
		{
			this->saveObj(2);
		}
	}

	void onProcessKeyboard()
	{
		keys.update();
		onProcessKeyboard_Common();
		switch (appmode)
		{
		case MODE_VIEW:
			if (keys(VK_RETURN))
			{
				appmode = MODE_SNAP;
				setEyeCamUnit(snapshot_cam_index);
				break;
			}
			onProcessKeyboard_Viewer();
			break;
		case MODE_SNAP:
			if (keys(VK_RETURN))
			{
				appmode = MODE_VIEW;
				setDefaultCam();
				break;
			}
			onProcessKeyboard_Snapshot();
			break;
		case MODE_ONESHOT:
			break;
		}
	}

	void displayText_Frames()
	{
		freetype::print(font, 420,20,
			"%5d frames, %3d sec",
			data.frame_index,
			data.frame_index/30);
		freetype::print(font, 220,20,
			"[Enter] change mode");
	}

	void displayText_View()
	{
		const int h = 20;
		int y = 0;
		glRGBA(60,30,0)();

		auto pr = [&](const char* s){
			freetype::print(font, 10,y+=h, s);
		};

		displayText_Frames();
		pr("<< VIEWER MODE >>");
		pr("");

		for (int i=0; i<6; ++i)
		{
			freetype::print(font, 10,y+=h,
				"cam%d dots = %6d (%5.2f)",
				i+1,
				camsys[0].cams[i].valid_dots,
				camsys[0].cams[i].center.x);
		}
		freetype::print(font, 10,y+=h,
			"eye={x:%.2f,y:%.2f,z:%.2f,rh:%.2f,%.2f} [a/s/d/w]",
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
		
		pr("");
		pr("[F1].....rewind");
		pr("[F2].....reverse");
		pr("[F3].....play");
		pr("[F4].....pause");
		pr("[F6].....play with save pictures");
		pr("[1-6]....preset cam (2D)");
		pr("[9,0]....preset cam (3D)");
		pr("[Space]..next frame (one frame)");
		pr("[Z][X]...prev/next frame (continuous)");
	}

	void displayText_Snapshot()
	{
		glRGBA(240,70,0)();
		const int h = 20;
		const int x = 10;
		int y = 0;
		auto pr = [&](const char* s){
			freetype::print(font, 10,y+=h, s);
		};

		displayText_Frames();
		pr("<< SNAPSHOT MODE >>");
		pr("");
		for (int i=0; i<6; ++i)
		{
			char buf[1000];
			if (snap3d[i].exist)
			{
				sprintf_s(buf, "(frame:%d)",
					snap3d[i].frame_index);
			}
			else
			{
				sprintf_s(buf, "empty");
			}

			freetype::print(font, x,y+=h,
				"%s Camera[%d] = %s",
				(i==snapshot_cam_index) ? "===>" : "    ",
				i+1, buf);
		}

		pr("");
		pr("[1-6]....Select camera 1 to 6");
		pr("[Space]..Create PNG snapshot");
		pr("[o]......Create <gameid-1.OBJ> snapshot");
		pr("[p]......Create <gameid-2.OBJ> snapshot");
	}

	void onDisplay2D()
	{
		switch (appmode)
		{
		case MODE_VIEW:
			displayText_View();
			break;
		case MODE_SNAP:
			displayText_Snapshot();
			break;
		case MODE_ONESHOT:
			snapOne(snapshot_cam_index);
			appmode = MODE_SNAP;
			break;
		}
	}
};


Point2D glfwGetWindowSize()
{
	int w,h;
	glfwGetWindowSize(&w,&h);
	return Point2D(w,h);
}


std::vector<string> splitQuotedArg(const string& arg)
{
	const char QUOTE = '"';
	std::vector<string> args;
	size_t i=0;
	for (;;)
	{
		while (i<arg.size())
		{
			if (isspace(arg[i]))
				++i;
			break;
		}
		if (i==arg.size())
			break;

		string work;
		if (arg[i]!=QUOTE)
		{
			// raw-string
			while (i<arg.size())
			{
				if (isspace(arg[i]))
					break;
				work += arg[i++];
			}
			args.push_back(work);
		}
		else
		{
			// "quoted-string"
			++i;  //open quote
			while (i<arg.size())
			{
				if (arg[i]==QUOTE)
					break;
				work += arg[i++];
			}
			++i;  // close quote
			args.push_back(work);
		}
	}
	return args;
}



static bool run_app(string arg)
{
	AppCore::initGraphics(false, "ST 3D Viewer");
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	glHint(GL_LINE_SMOOTH_HINT,    GL_NICEST);

	ViewerApp app;
	app.init();

	auto args = splitQuotedArg(arg);
	for (size_t i=0; i<args.size(); ++i)
	{
		app.load(ViewerAppBase::getBaseName(args[i].c_str()));
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
