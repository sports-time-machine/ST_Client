#pragma once
#include "gl_funcs.h"


struct GlobalConfig
{
};
struct Config
{
	int client_number;
	int near_threshold;
	int far_threshold;
	int initial_window_x;
	int initial_window_y;
	int initial_fullscreen;
	
	struct KinectCalibration
	{
		Point2i a,b,c,d;
	} kinect_calibration;

	Config();
};

#ifdef THIS_IS_MAIN
#define EXTERN /*nop*/
#else
#define EXTERN extern
#endif


EXTERN GlobalConfig  global_config;
EXTERN Config        config;
