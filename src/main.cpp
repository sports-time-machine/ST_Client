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
	CONFIG_INT(config, far_threshold);
	CONFIG_INT(config, near_threshold);
	CONFIG_INT(client_config, client_number);
#undef CONFIG_INT
}

int main(int argc, char** argv)
{
	load_config();





	openni::Status rc = openni::STATUS_OK;

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


	SampleViewer sampleViewer("ST Client", device, depth, color);

	rc = sampleViewer.init(argc, argv);
	if (rc != openni::STATUS_OK)
	{
		openni::OpenNI::shutdown();
		return 3;
	}
	sampleViewer.run();
}
