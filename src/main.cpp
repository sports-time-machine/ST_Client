#define THIS_IS_MAIN
#include <OpenNI.h>
#include "StClient.h"

#ifdef _M_X64
#pragma comment(lib,"OpenNI2_x64.lib")
#else
#pragma comment(lib,"OpenNI2_x32.lib")
#endif
#pragma comment(lib,"opengl32.lib")
#pragma comment(lib,"glu32.lib")

using namespace mi;
using namespace stclient;

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

static void get_kinect_devices(string& first, string& second)
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

static void ErrorDialog(const char* title)
{
	const char* text = openni::OpenNI::getExtendedError();
	Core::dialog(title, text);
}

#if 1
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

		if (config.enable_color)
		{
			printf("Create Color...");
			create_color();
			puts("done.");
		}
		else
		{
			puts("Skip color.");
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
#endif

static bool run_app()
{
	Kdev dev1, dev2;

	if (config.enable_kinect)
	{
		Msg::BarMessage("Init Kinect");
		init_kinect();

		string first, second;
		get_kinect_devices(first, second);
		
		if (first.empty())
		{
			Core::dialog("Kinectが見つかりません。起動を中止します。");
			return false;
		}
		if (second.empty())
		{
			Core::dialog("Kinectが2台見つかりません。起動を中止します。");
			return false;
		}

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
		if (config.enable_kinect)
		{
			openni::OpenNI::shutdown();
		}
		return false;
	}
	st_client.run();
	return true;
}

int main()
{
	mi::Console::setTitle("スポーツタイムマシン コンソール");
	if (!load_config())
	{
		return EXIT_FAILURE;
	}
	return run_app() ? EXIT_SUCCESS : EXIT_FAILURE;
}
