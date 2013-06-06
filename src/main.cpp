#define THIS_IS_MAIN
#include <OpenNI.h>
#include "ST_Client.h"
#define ENABLE_KINECT_COLOR 0

#include "Config.h"

#pragma warning(push)
#pragma warning(disable: 4100) // unused variable
#pragma warning(disable: 4201) // non-standard expanded function
#pragma warning(disable: 4512) // 
#include "PSL/PSL.h"
#pragma warning(pop)

#ifdef _M_X64
#pragma comment(lib,"OpenNI2_x64.lib")
#else
#pragma comment(lib,"OpenNI2_x32.lib")
#endif


Config::Config()
{
	client_number = -1;
	near_threshold = 500;
	far_threshold = 5000;
	initial_window_x = 50;
	initial_window_y = 50;
	initial_fullscreen = false;
	
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

#define CONFIG_INT(DEST,NAME) DEST.NAME = PSL::variable(psl.get(#NAME)).toInt()
#define CONFIG_INT_A(DEST,NAME,I,J) DEST.NAME = PSL::variable(psl.get(#NAME))[I][J].toInt()
	CONFIG_INT(config, far_threshold);
	CONFIG_INT(config, near_threshold);
	CONFIG_INT(config, client_number);
	CONFIG_INT(config, initial_window_x);
	CONFIG_INT(config, initial_window_y);
	CONFIG_INT(config, initial_fullscreen);

	{
		PSL::variable src = psl.get("kinect_calibration");
		auto& dest = config.kinect_calibration;
		dest.a = Point2i(src[0][0], src[0][1]);
		dest.b = Point2i(src[1][0], src[1][1]);
		dest.c = Point2i(src[2][0], src[2][1]);
		dest.d = Point2i(src[3][0], src[3][1]);
	}
#undef CONFIG_INT
}

int main(int argc, char** argv)
{
	load_config();



	openni::Status rc;

#if !WITHOUT_KINECT
	rc = openni::STATUS_OK;

	openni::Device device;
	openni::VideoStream depth, color;
	const char* deviceURI = openni::ANY_DEVICE;
	if (argc > 1)
	{
		deviceURI = argv[1];
	}

	rc = openni::OpenNI::initialize();

	printf("After initialization:\n%s\n", openni::OpenNI::getExtendedError());

	auto create_device = [&](){
		rc = device.open(deviceURI);
		if (rc != openni::STATUS_OK)
		{
			ErrorDialog("Device open failed");
			exit(1);
		}
	};

	auto create_depth = [&](){
		rc = depth.create(device, openni::SENSOR_DEPTH);
		if (rc == openni::STATUS_OK)
		{
			rc = depth.start();
			if (rc != openni::STATUS_OK)
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
		rc = color.create(device, openni::SENSOR_COLOR);
		if (rc == openni::STATUS_OK)
		{
			rc = color.start();
			if (rc != openni::STATUS_OK)
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

#if ENABLE_KINECT_COLOR
		printf("Create Color...");
		create_color();
		puts("done.");
#endif
	}
	catch (...)
	{
		puts("Unknown fault.");
		return -1;
	}


#if ENABLE_KINECT_COLOR
	if (!depth.isValid() || !color.isValid())
	{
		printf("SimpleViewer: No valid streams. Exiting\n");
		openni::OpenNI::shutdown();
		return 2;
	}
#else
	if (!depth.isValid())
	{
		ErrorDialog("No valid streams.");
		exit(1);
	}
#endif
#endif//WITHOUT_KINECT

#if !WITHOUT_KINECT
	StClient st_client(device, depth, color);
#else
	openni::Device device;
	openni::VideoStream depth, color;
	SampleViewer sampleViewer("ST Client (wok)", device, depth, color);
#endif//WITHOUT_KINECT

	if (st_client.init(argc, argv)==false)
	{
#if !WITHOUT_KINECT
		openni::OpenNI::shutdown();
#endif//WITHOUT_KINECT
		return 3;
	}
	st_client.run();
}
