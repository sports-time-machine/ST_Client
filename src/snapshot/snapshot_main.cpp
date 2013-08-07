#define THIS_IS_MAIN
#include "../ViewerAppBase.h"

class ViewerApp : public ViewerAppBase
{
public:
	std::map<int,bool> tookPicture;
	std::map<int,int> displayTimePerUnit;
	bool quit_app;

	bool isPictureTake(float x, int n)
	{
		switch (n)
		{
		default: return false;
		case 0:  return (data.frame_index>=350 && data.frame_index<=400);
		case 1:  return (x>=0.00f);
		case 2:  return (x>=0.00f);
		case 3:  return (x>=0.00f);
		case 4:  return (x>=0.00f);
		case 5:  return (x>=0.50f);
		}
	}

	// カメラ番号0～5
	void eyeCamUnit(int n)
	{
		eye2d(4.0f*n, -0.40f, 40.0f, -PI/2, 0.0f, 40);
	}

	bool onInit()
	{
		loadConfigPsl();
	
		eyeCamUnit(1);
		for (int i=0; i<6; ++i)
		{
			tookPicture[i] = false;
			displayTimePerUnit[i] = 0;
		}

		this->quit_app = false;
		this->debug_show = false;
		this->data.frame_index = 300;
		this->data.frame_auto = Data::INCR;
		return true;
	}

	void onFrameBegin()
	{
		for (int i=0; i<6; ++i)
		{
			const auto& cam = cams[i];
			if (cam.center==InvalidPoint3D())
				continue;

			++displayTimePerUnit[i];

			volatile float cx = cam.center.x;
			if (displayTimePerUnit[i]>=5 && cx>=-2.0f)
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

			// 最低でもFRフレーム表示してからでないと
			// スナップショットを撮らない
			const int FR = 5;

			volatile float cx = cam.center.x;
			if (displayTimePerUnit[i]>=FR && tookPicture[i]==false && isPictureTake(cx,i))
			{
				tookPicture[i] = true;
				saveScreenShot(1+i);
				if (i==5)
				{
					this->quit_app = true;
				}
				break;
			}
		}
	}

	void onProcessMouse()
	{
	}

	void onProcessKeyboard()
	{
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
	ViewerApp app;
	if (!app.init())
	{
		return false;
	}
	if (arg.length()==0)
	{
		Msg::ErrorMessage("Empty argument");
		return false;
	}

	if (arg[0]=='"')
	{
		arg = arg.substr(1, arg.length()-2);
	}
	if (!app.load(ViewerAppBase::getBaseName(arg.c_str())))
	{
		Msg::ErrorMessage("Load error");
		return false;
	}

	AppCore::initGraphics(false, "ST Snapshot");
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	glHint(GL_LINE_SMOOTH_HINT,    GL_NICEST);

	glfwSwapInterval(0);
	double prev_msec = 0.0;
	while (glfwGetWindowParam(GLFW_OPENED))
	{
		app.runFrame();
		if (app.quit_app)
			break;
		glfwSwapBuffers();
	}
	glfwTerminate();
	return true;
}

int main(int argc, const char* argv[])
{
	return run_app(argc>=2 ? argv[1] : "") ? EXIT_SUCCESS : EXIT_FAILURE;
}
