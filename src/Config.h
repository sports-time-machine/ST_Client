#pragma once
#include "gl_funcs.h"


struct GlobalConfig
{
	bool enable_kinect;
	bool enable_color;

	GlobalConfig();
};
struct Config
{
	int client_number;
	int near_threshold;
	int far_threshold;
	int far_cropping;
	int initial_window_x;
	int initial_window_y;
	bool initial_fullscreen;
	bool mirroring;
	
	struct KinectCalibration
	{
		Point2i a,b,c,d;
	} kinect_calibration;

	Config();
};

#ifdef THIS_IS_MAIN
#define SmartExtern /*nop*/
#else
#define SmartExtern extern
#endif

SmartExtern GlobalConfig  global_config;
SmartExtern Config        config;
