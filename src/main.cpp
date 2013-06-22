#define THIS_IS_MAIN
#include <OpenNI.h>
#include "ST_Client.h"
#include "Config.h"

//begin load psl
#pragma warning(push)
#pragma warning(disable: 4100) // unused variable
#pragma warning(disable: 4201) // non-standard expanded function
#pragma warning(disable: 4512) // 
#pragma warning(disable: 4996) // unsafe function
#include "PSL/PSL.h"
#pragma warning(pop)
//end of psl

#ifdef _M_X64
#pragma comment(lib,"OpenNI2_x64.lib")
#else
#pragma comment(lib,"OpenNI2_x32.lib")
#endif
#pragma comment(lib,"opengl32.lib")
#pragma comment(lib,"glu32.lib")


using namespace mi;
using namespace stclient;



GlobalConfig::GlobalConfig()
{
	enable_kinect = true;
	enable_color  = false;
	wall_depth = 3.0f;
	ground_color.set(80, 40, 20);
	grid_color.set(200, 150, 130);
	person_color.set(60,60,60, 200);
	movie1_color.set(120,50,50,200);
	movie2_color.set(50,120,50,200);
	movie3_color.set(50,50,120,200);
	person_dot_px = 1.5f;
}


Config::Config()
{
	client_number = -1;
	near_threshold = 500;
	far_threshold = 5000;
	initial_window_x = 50;
	initial_window_y = 50;
	initial_fullscreen = false;
	mirroring = false;
	
	metrics.ground_px = 480;
	metrics.left_mm   = 0;
	metrics.right_mm  = 4000;
	metrics.top_mm    = 2500;
}



void ErrorDialog(const char* title)
{
	const char* text = openni::OpenNI::getExtendedError();
	Core::dialog(title, text);
}

static bool var_exist(PSL::variable& var)
{
	using namespace PSL;
	if (var.length()!=0)              return true;  //array
	if (var.memberLength()!=0)        return true;  //hash
	if (var.type()==variable::INT)    return true;
	if (var.type()==variable::FLOAT)  return true;
	if (var.type()==variable::HEX)    return true;
	if (var.type()==variable::STRING) return true;
	return false;
}

static bool var_exist(PSL::PSLVM& vm, const char* name)
{
	PSL::variable var = vm.get(name);
	return var_exist(var);
}


void load_config()
{
	PSL::PSLVM psl;

	auto load_psl = [&](PSL::string path)->bool{
		fprintf(stderr, "[PSL] Load '%s'...", path.c_str());
		auto err = psl.loadScript(path);
		if (err==PSL::PSLVM::NONE)
		{
			fprintf(stderr, "Ok\n");
			return true;
		}
		switch (err)
		{
		case PSL::PSLVM::FOPEN_ERROR:
			fprintf(stderr, "Cannot open file '%s'\n", path.c_str());
			return false;
		case PSL::PSLVM::PARSE_ERROR:
			fprintf(stderr, "PSL parse error (Syntax error?)\n");
			return false;
		default:
			fprintf(stderr, "PSL unknown error\n");
			return false;
		}
	};

	if (!load_psl("//STMx64/ST/Config.psl"))
	{
		puts("Config.psl load error.");
	}
	if (!load_psl(PSL::string("C:/ST/")+Core::getComputerName().c_str()+".psl"))
	{
		puts("(client-name).psl load error.");
	}

	psl.run();

	using namespace PSL;

#define CONFIG_LET2(DEST,NAME,C,FUNC)   if(var_exist(psl,#NAME)){ DEST=(C)PSL::variable(psl.get(#NAME)).FUNC(); }
#define CONFIG_LET(DEST,NAME,C,FUNC)   CONFIG_LET2(DEST.NAME, NAME, C, FUNC)
#define CONFIG_INT(DEST,NAME)          CONFIG_LET(DEST,NAME,int,toInt)
#define CONFIG_BOOL(DEST,NAME)         CONFIG_LET(DEST,NAME,bool,toBool)
#define CONFIG_FLOAT(DEST,NAME)        CONFIG_LET(DEST,NAME,float,toDouble)
	CONFIG_INT(config, far_threshold);
	CONFIG_INT(config, near_threshold);
	CONFIG_INT(config, far_cropping);
	CONFIG_INT(config, client_number);
	CONFIG_INT(config, initial_window_x);
	CONFIG_INT(config, initial_window_y);
	CONFIG_BOOL(config, initial_fullscreen);
	CONFIG_BOOL(config, mirroring);
	printf("mirroring: %d\n", config.mirroring);

	// Metrics
	double left_meter  = 0.0f;
	double right_meter = 0.0f;
	double top_meter   = 0.0f;
	int    ground_px   = 0;
	CONFIG_LET2(left_meter,  metrics_left_meter,  float, toDouble);
	CONFIG_LET2(right_meter, metrics_right_meter, float, toDouble);
	CONFIG_LET2(top_meter,   metrics_topt_meter,  float, toDouble);
	CONFIG_LET2(ground_px,   metrics_ground_px,   int,   toInt);
	config.metrics.left_mm   = (int)(1000 * left_meter);
	config.metrics.right_mm  = (int)(1000 * right_meter);
	config.metrics.top_mm    = (int)(1000 * top_meter);
	config.metrics.ground_px = ground_px;

	auto set_rgb = [&](const char* name, mgl::glRGBA& dest){
		PSL::variable src = psl.get(name);
		if (var_exist(src))
		{
			int alpha = src["a"].toInt();
			if (alpha==0) alpha=255;
			dest.set(
				src["r"].toInt(),
				src["g"].toInt(),
				src["b"].toInt(),
				alpha);
		}
	};
	auto set_camera_param = [&](CamParam& cam, const char* varname){
		PSL::variable var = psl.get(varname);
		cam.x     = (float)var["x"];
		cam.y     = (float)var["y"];
		cam.z     = (float)var["z"];
		cam.rotx  = (float)var["rotx"];
		cam.roty  = (float)var["roty"];
		cam.rotz  = (float)var["rotz"];
		cam.scale = (float)var["scale"];
	};
	auto pslString = [&](const char* name)->const char*{
		return PSL::variable(psl.get(name)).toString().c_str();
	};

	//=== LOCAL SETTINGS ===
	CONFIG_INT(config, person_inc);
	CONFIG_INT(config, movie_inc);
	set_camera_param(config.cam1, "camera1");
	set_camera_param(config.cam2, "camera2");

	//=== GLOBAL SETTINGS ===
	auto& gc = global_config;
	CONFIG_BOOL(gc, enable_kinect);
	CONFIG_BOOL(gc, enable_color);
	CONFIG_FLOAT(gc, wall_depth);
	gc.background_image = pslString("background_image");

	//=== COLORS ===
	set_rgb("ground_color", gc.ground_color);
	set_rgb("grid_color",   gc.grid_color);
	set_rgb("person_color", gc.person_color);
	set_rgb("movie1_color", gc.movie1_color);
	set_rgb("movie2_color", gc.movie2_color);
	set_rgb("movie3_color", gc.movie3_color);
	set_rgb("text_heading_color", gc.text.heading_color);
	set_rgb("text_normal_color",  gc.text.normal_color);
	set_rgb("text_dt_color",      gc.text.dt_color);
	set_rgb("text_dd_color",      gc.text.dd_color);
	CONFIG_FLOAT(global_config, person_dot_px);

#undef CONFIG_INT
#undef CONFIG_BOOL
}

static void init_kinect()
{
	printf("Initialize OpenNI...");
	if (openni::STATUS_OK!=openni::OpenNI::initialize())
	{
		printf("%s\n", openni::OpenNI::getExtendedError());
		exit(1);
	}
	else
	{
		puts("done.");
	}
}

static void get_kinect_devices(std::string& first, std::string& second)
{
	openni::Array<openni::DeviceInfo> devices;
	openni::OpenNI::enumerateDevices(&devices);

	printf("%d kinect(s) found.\n", devices.getSize());
	first  = "";
	second = "";
	for (int i=0; i<devices.getSize(); ++i)
	{
		const auto& dev = devices[i];
		printf("  [%d] %s %s %s\n",
			i,
			dev.getVendor(),
			dev.getName(),
			dev.getUri());
		if (i==0) first  = dev.getUri();
		if (i==1) second = dev.getUri();
	}
}


static void init_kinect(const char* uri, Kdev& k)
{
	auto create_device = [&](){
		if (openni::STATUS_OK!=k.device.open(uri))
		{
			ErrorDialog("Device open failed");
			exit(1);
		}
	};

	auto create_depth = [&](){
		if (openni::STATUS_OK==k.depth.create(k.device, openni::SENSOR_DEPTH))
		{
			auto status = k.depth.start();
			if (status!=openni::STATUS_OK)
			{
				ErrorDialog("Couldn't start depth stream");
				exit(1);
			}
		}
		else
		{
			ErrorDialog("Couldn't find depth stream");
			exit(1);
		}
	};

	auto create_color = [&](){
		if (openni::STATUS_OK==k.color.create(k.device, openni::SENSOR_COLOR))
		{
			if (openni::STATUS_OK!=k.color.start())
			{
				ErrorDialog("Couldn't start color stream");
				exit(1);
			}
		}
		else
		{
			ErrorDialog("Couldn't find color stream");
			exit(1);
		}
	};

	try
	{
		printf("Create Device...");
		create_device();
		puts("done.");

		printf("Create Depth...");
		create_depth();
		puts("done.");

		if (global_config.enable_color)
		{
			printf("Create Color...");
			create_color();
			puts("done.");
		}
		else
		{
			puts("Skip color.\n");
		}
	}
	catch (...)
	{
		puts("Unknown fault.");
		exit(1);
	}

	if (!k.depth.isValid())
	{
		ErrorDialog("No valid streams.");
		exit(1);
	}
}


#include "mi/Timer.h"
#include "mi/Console.h"

#include "file_io.h"


int main()
{
//	global_config.enable_kinect = false;

	mi::Console::setTitle("スポーツタイムマシン コンソール");

	load_config();

	Kdev dev1, dev2;

	if (global_config.enable_kinect)
	{
		init_kinect();

		std::string first, second;
		get_kinect_devices(first, second);
		if (!first.empty())
		{
			init_kinect(
				first.c_str(),
				dev1);
		}
		if (!second.empty())
		{
			init_kinect(
				second.c_str(),
				dev2);
		}
	}

	StClient st_client(dev1, dev2);
	if (st_client.init()==false)
	{
		if (global_config.enable_kinect)
		{
			openni::OpenNI::shutdown();
		}
		return 1;
	}
	st_client.run();
}
