#pragma once
#include "gl_funcs.h"
#include "file_io.h"


struct GlobalConfig
{
	bool enable_kinect;
	bool enable_color;
	float wall_depth;
	float ground_r;
	float ground_g;
	float ground_b;
	float grid_r;
	float grid_g;
	float grid_b;

	GlobalConfig();
};

struct Config
{
	int person_inc;
	int movie_inc;
	int client_number;
	int near_threshold;
	int far_threshold;
	int far_cropping;
	int initial_window_x;
	int initial_window_y;
	bool initial_fullscreen;
	bool mirroring;

	CamParam cam1,cam2;

	struct Metrics
	{
		int left_mm;
		int right_mm;
		int top_mm;
		int ground_px;
	} metrics;

	Config();
};

#ifdef THIS_IS_MAIN
#define SmartExtern /*nop*/
#else
#define SmartExtern extern
#endif

SmartExtern GlobalConfig  global_config;
SmartExtern Config        config;
