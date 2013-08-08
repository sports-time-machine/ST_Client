#define THIS_IS_MAIN
#include "../ViewerAppBase.h"

class ViewerApp : public ViewerAppBase
{
public:
	bool   alreadyTook[6];
	int    displayTimePerUnit[6];
	std::vector<Dots> snap3d;
	bool   quit_app;

	// 往路
	bool isPictureTake_Go(float x, int n)
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

	// 復路
	bool isPictureTake_Back(float x, int n)
	{
		switch (n)
		{
		default: return false;
		case 0:  return (x<-0.50f);
		case 1:  return (x< 0.00f);
		case 2:  return (x< 0.00f);
		case 3:  return (x< 0.00f);
		case 4:  return (x< 0.00f);
		case 5:  return (x< 0.50f);
		}
	}

	// カメラ番号0～5
	void eyeCamUnit(int n)
	{
		eye2d(4.0f*n, -0.40f, 40.0f, -PI/2, 0.0f, 40);
	}

	enum Direction
	{
		DIR_GO,
		DIR_BACK,
	};
	Direction dir;

	bool onInit()
	{
		loadConfigPsl();
	
		eyeCamUnit(1);
		for (int i=0; i<6; ++i)
		{
			alreadyTook[i] = false;
			displayTimePerUnit[i] = 0;
		}

		this->snap3d.resize(6);
		this->quit_app = false;
		this->debug_show = false;
		this->data.frame_index = 300;
		this->data.frame_auto = Data::INCR;
		this->dir = DIR_GO;
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

	void saveObj(int num)
	{
		string obj_filename = config.folder_format + config.obj_format;
		replaceVars(obj_filename, num);

		File f;
		f.openForWrite(obj_filename);

		int face_base = 0;
		for (int i=0; i<6; ++i)
		{
			face_base += ObjWriter::outputTetra(1.0f, f, snap3d[i], i*4.0f, face_base);
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
			if (displayTimePerUnit[i]<FR) continue;
			if (alreadyTook[i])           continue;

			int save_num = 0;
			bool snap = false;
			bool save = false;

			if (dir==DIR_GO)
			{
				snap = isPictureTake_Go(cx,i);
				save = (i==5);
				save_num = 1;
			}
			else
			{
				snap = isPictureTake_Back(cx,i);
				save = (i==0);
				save_num = 2;
			}

			if (snap)
			{
				alreadyTook[i] = true;
				saveScreenShot(1+i);
				snap3d[i].copyFrom(cams[i].dots);
				if (save)
				{
					saveObj(save_num);
					for (int i=0; i<6; ++i)
						alreadyTook[i] = false;
						
					switch (dir)
					{
					case DIR_GO:
						dir = DIR_BACK;
						break;
					case DIR_BACK:
						this->quit_app = true;
						break;
					}
				}
			}
			break;
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
