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
	
	kinect_calibration.a = Point2i(0,0);
	kinect_calibration.b = Point2i(640,0);
	kinect_calibration.c = Point2i(0,480);
	kinect_calibration.d = Point2i(640,480);
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
		return;
	}
	if (!load_psl(PSL::string("C:/ST/")+Core::getComputerName().c_str()+".psl"))
	{
		return;
	}

	psl.run();


	auto int_exists = [&](const char* name)->bool{
		return PSL::variable(psl.get(name)).type()==PSL::variable::INT;
	};

#define CONFIG_LET(DEST,NAME,FUNC)   if(int_exists(#NAME)){ DEST.NAME=PSL::variable(psl.get(#NAME)).FUNC(); }
#define CONFIG_INT(DEST,NAME)        CONFIG_LET(DEST,NAME,toInt)
#define CONFIG_BOOL(DEST,NAME)       CONFIG_LET(DEST,NAME,toBool)
	CONFIG_INT(config, far_threshold);
	CONFIG_INT(config, near_threshold);
	CONFIG_INT(config, far_cropping);
	CONFIG_INT(config, client_number);
	CONFIG_INT(config, initial_window_x);
	CONFIG_INT(config, initial_window_y);
	CONFIG_BOOL(config, initial_fullscreen);
	CONFIG_BOOL(config, mirroring);

	CONFIG_BOOL(global_config, enable_kinect);
	CONFIG_BOOL(global_config, enable_color);

	{
		PSL::variable src = psl.get("kinect_calibration");
		auto& dest = config.kinect_calibration;
		dest.a = Point2i(src[0][0], src[0][1]);
		dest.b = Point2i(src[1][0], src[1][1]);
		dest.c = Point2i(src[2][0], src[2][1]);
		dest.d = Point2i(src[3][0], src[3][1]);
	}
#undef CONFIG_INT
#undef CONFIG_BOOL
}

static void init_kinect(openni::Device& device, openni::VideoStream& depth, openni::VideoStream& color)
{
	const char* deviceURI = openni::ANY_DEVICE;

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

	auto create_device = [&](){
		if (openni::STATUS_OK!=device.open(deviceURI))
		{
			ErrorDialog("Device open failed");
			exit(1);
		}
	};

	auto create_depth = [&](){
		if (openni::STATUS_OK==depth.create(device, openni::SENSOR_DEPTH))
		{
			if (openni::STATUS_OK!=depth.start())
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
		if (openni::STATUS_OK==color.create(device, openni::SENSOR_COLOR))
		{
			if (openni::STATUS_OK!=color.start())
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

	if (!depth.isValid())
	{
		ErrorDialog("No valid streams.");
		exit(1);
	}
}

int main(int argc, char** argv)
{
	load_config();

	openni::Device device;
	openni::VideoStream depth;
	openni::VideoStream color;

	if (global_config.enable_kinect)
	{
		init_kinect(device, depth, color);
	}

	StClient st_client(device, depth, color);
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
