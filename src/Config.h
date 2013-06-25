#pragma once
#include "gl_funcs.h"
#include "file_io.h"
#include "psl_if.h"

#define GROUND_WIDTH       (4.00f)
#define GROUND_LEFT        (-GROUND_WIDTH/2)
#define GROUND_RIGHT       (+GROUND_WIDTH/2)
#define GROUND_HEIGHT      (2.75f)
#define GROUND_DEPTH       (3.00f)


struct GlobalConfig
{
	std::string   background_image;
	bool          enable_kinect;
	bool          enable_color;
	float         wall_depth;
	int           auto_snapshot_interval;

	struct Colors
	{
		mgl::glRGBA
			ground, grid,
			person, movie1, movie2, movie3,
			snapshot,
			text_h1, text_p, text_em, text_dt, text_dd;
	};

	Colors color;

	float person_dot_px;

	GlobalConfig();
};

struct Config
{
	int   person_inc;
	int   movie_inc;
	int   client_number;
	int   initial_window_x;
	int   initial_window_y;
	bool  initial_fullscreen;
	bool  mirroring;
	int   hit_threshold;
	bool  ignore_udp;

	float getScreenLeftMeter() const
	{
		return (client_number-1) * GROUND_WIDTH;
	}
	
	float getScreenRightMeter() const
	{
		return getScreenLeftMeter() + GROUND_WIDTH;
	}


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
