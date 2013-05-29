#include <OpenNI.h>
#include "ST_Client.h"
#define ENABLE_KINECT_COLOR 0


int main(int argc, char** argv)
{
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
			printf("SimpleViewer: Device open failed:\n%s\n", openni::OpenNI::getExtendedError());
			throw "exception";
//			openni::OpenNI::shutdown();
//			return 1;
		}
	};

	auto create_depth = [&](){
		rc = depth.create(device, openni::SENSOR_DEPTH);
		if (rc == openni::STATUS_OK)
		{
			rc = depth.start();
			if (rc != openni::STATUS_OK)
			{
				printf("SimpleViewer: Couldn't start depth stream:\n%s\n", openni::OpenNI::getExtendedError());
				throw "exception";
//				depth.destroy();
			}
		}
		else
		{
			printf("SimpleViewer: Couldn't find depth stream:\n%s\n", openni::OpenNI::getExtendedError());
		}
	};

	auto create_color = [&](){
		rc = color.create(device, openni::SENSOR_COLOR);
		if (rc == openni::STATUS_OK)
		{
			rc = color.start();
			if (rc != openni::STATUS_OK)
			{
				printf("SimpleViewer: Couldn't start color stream:\n%s\n", openni::OpenNI::getExtendedError());
				throw "exception";
//				color.destroy();
			}
		}
		else
		{
			printf("SimpleViewer: Couldn't find color stream:\n%s\n", openni::OpenNI::getExtendedError());
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
	catch (const char*)
	{
		puts("Fault.");
		return -1;
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
		printf("SimpleViewer: No valid streams. Exiting\n");
		openni::OpenNI::shutdown();
		return 2;
	}
#endif


	SampleViewer sampleViewer("Simple Viewer", device, depth, color);

	rc = sampleViewer.init(argc, argv);
	if (rc != openni::STATUS_OK)
	{
		openni::OpenNI::shutdown();
		return 3;
	}
	sampleViewer.run();
}