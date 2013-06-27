#pragma once
#include "gl_funcs.h"
#include "file_io.h"
#include "psl_if.h"

namespace stclient{

#define GROUND_WIDTH       (4.00f)
#define GROUND_LEFT        (-GROUND_WIDTH/2)
#define GROUND_RIGHT       (+GROUND_WIDTH/2)
#define GROUND_HEIGHT      (2.40f)
#define GROUND_DEPTH       (3.00f)
#define GROUND_NEAR        (0.00f)
#define GROUND_FAR         (GROUND_DEPTH)

// 床付近はノイズが多いので膝丈ぐらいからのみ有効
// 画面の左右にマージンを設けることで、画面外にいきつつある場合であっても、
// 人物の中央をとることができるようにする
// 人の肩幅はせいぜい50cmであるので、50cmマージンがあれば概ね問題ないと判断する
#define ATARI_MARGIN       (0.50f)
#define ATARI_LEFT         (GROUND_LEFT  - ATARI_MARGIN)
#define ATARI_RIGHT        (GROUND_RIGHT + ATARI_MARGIN)
#define ATARI_BOTTOM       (0.50f)
#define ATARI_TOP          (GROUND_HEIGHT)

enum
{
	INITIAL_WIN_SIZE_X   = 640,
	INITIAL_WIN_SIZE_Y   = 480,
	MOVIE_MAX_SECS       = 50,
	MOVIE_FPS            = 30,
	MOVIE_MAX_FRAMES     = MOVIE_MAX_SECS * MOVIE_FPS,
	MIN_VOXEL_INC        = 16,
	MAX_VOXEL_INC        = 128,
	ATARI_INC            = 20,
	SNAPSHOT_LIFE_FRAMES = 100,
	MAX_PICT_NUMBER      = 10,        // PICT numコマンドで送れるピクチャの数
};


struct GlobalConfig
{
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
	struct Images
	{
		string background;
		string dot;
		string sleep;
		string pic[MAX_PICT_NUMBER];
		string idle;
	};

	Images  images;
	int     person_inc;
	int     movie_inc;
	int     client_number;
	int     center_atari_voxel_threshould;     // アタリ中央をとるために必要なボクセル数（2500～とか）
	bool    initial_fullscreen;
	bool    mirroring;
	int     hit_threshold;
	bool    ignore_udp;

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

}//namespace stclient
