#define THIS_IS_MAIN
#include <OpenNI.h>
#include "ST_Client.h"
#include "Config.h"

//begin load psl
#pragma warning(push)
#pragma warning(disable: 4100) // unused variable
#pragma warning(disable: 4201) // non-standard expanded function
#pragma warning(disable: 4512) // 
#include "PSL/PSL.h"
#pragma warning(pop)
//end of psl

#ifdef _M_X64
#pragma comment(lib,"OpenNI2_x64.lib")
#else
#pragma comment(lib,"OpenNI2_x32.lib")
#endif


GlobalConfig::GlobalConfig()
{
	enable_kinect = true;
	enable_color  = false;
	wall_depth = 3.0f;
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

	kinect1_calibration.a = Point2i(0,0);
	kinect1_calibration.b = Point2i(640,0);
	kinect1_calibration.c = Point2i(0,480);
	kinect1_calibration.d = Point2i(640,480);
	kinect2_calibration.a = Point2i(0,0);
	kinect2_calibration.b = Point2i(640,0);
	kinect2_calibration.c = Point2i(0,480);
	kinect2_calibration.d = Point2i(640,480);
}



void ErrorDialog(const char* title)
{
	const char* text = openni::OpenNI::getExtendedError();
	Core::dialog(title, text);
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

	auto int_exists = [&](const char* name)->bool{
		return PSL::variable(psl.get(name)).type()==PSL::variable::INT;
	};

#define CONFIG_LET2(DEST,NAME,FUNC)   if(int_exists(#NAME)){ DEST=PSL::variable(psl.get(#NAME)).FUNC(); }
#define CONFIG_LET(DEST,NAME,FUNC)   CONFIG_LET2(DEST.NAME, NAME, FUNC)
#define CONFIG_INT(DEST,NAME)        CONFIG_LET(DEST,NAME,toInt)
#define CONFIG_BOOL(DEST,NAME)       CONFIG_LET(DEST,NAME,toBool)
#define CONFIG_FLOAT(DEST,NAME)      CONFIG_LET(DEST,NAME,toDouble)
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
	CONFIG_LET2(left_meter,  metrics_left_meter,  toDouble);
	CONFIG_LET2(right_meter, metrics_right_meter, toDouble);
	CONFIG_LET2(top_meter,   metrics_topt_meter,  toDouble);
	CONFIG_LET2(ground_px,   metrics_ground_px,   toInt);
	config.metrics.left_mm   = (int)(1000 * left_meter);
	config.metrics.right_mm  = (int)(1000 * right_meter);
	config.metrics.top_mm    = (int)(1000 * top_meter);
	config.metrics.ground_px = ground_px;


	// Init flags
	CONFIG_BOOL(global_config, enable_kinect);
	CONFIG_BOOL(global_config, enable_color);
	CONFIG_FLOAT(global_config, wall_depth);

	{
		PSL::variable src = psl.get("kinect1_calibration");
		auto& dest = config.kinect1_calibration;
		dest.a = Point2i(src[0][0], src[0][1]);
		dest.b = Point2i(src[1][0], src[1][1]);
		dest.c = Point2i(src[2][0], src[2][1]);
		dest.d = Point2i(src[3][0], src[3][1]);
	}

	{
		PSL::variable src = psl.get("kinect2_calibration");
		auto& dest = config.kinect2_calibration;
		dest.a = Point2i(src[0][0], src[0][1]);
		dest.b = Point2i(src[1][0], src[1][1]);
		dest.c = Point2i(src[2][0], src[2][1]);
		dest.d = Point2i(src[3][0], src[3][1]);
	}

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



int main(int argc, char** argv)
{
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
	if (st_client.init(argc, argv)==false)
	{
		if (global_config.enable_kinect)
		{
			openni::OpenNI::shutdown();
		}
		return 1;
	}
	st_client.run();
}
