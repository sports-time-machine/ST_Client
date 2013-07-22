#pragma once
#include "gl_funcs.h"

namespace stclient{

static const float
	GROUND_WIDTH       = (4.00f),
	GROUND_LEFT        = (-GROUND_WIDTH/2),
	GROUND_RIGHT       = (+GROUND_WIDTH/2),
	GROUND_HEIGHT      = (2.40f),
	GROUND_DEPTH       = (2.40f),
	GROUND_NEAR        = (0.00f),
	GROUND_FAR         = (GROUND_DEPTH),
	GROUND_TOP         = (GROUND_HEIGHT),
	GROUND_BOTTOM      = (0.00f);

// 床付近はノイズが多いので膝丈ぐらいからのみ有効
// 画面の左右にマージンを設けることで、画面外にいきつつある場合であっても、
// 人物の中央をとることができるようにする
// 人の肩幅はせいぜい50cmであるので、50cmマージンがあれば概ね問題ないと判断する
static const float
	ATARI_MARGIN       = (0.50f),
	ATARI_LEFT         = (GROUND_LEFT  - ATARI_MARGIN),
	ATARI_RIGHT        = (GROUND_RIGHT + ATARI_MARGIN),
	ATARI_BOTTOM       = (0.50f),
	ATARI_TOP          = (GROUND_HEIGHT);

static const float
	LOOKAT_EYE_DEPTH   = (4.0f),
	IDLE_IMAGE_Z       = (5.0f);

static mgl::glRGBA
	TIMEMACHINE_ORANGE(240,160,80, 160);

enum NonameEnum
{
	// これ以外についてはConfig.cppのConfig::Config()に記述してあります
	INITIAL_WIN_SIZE_X   = 640,
	INITIAL_WIN_SIZE_Y   = 480,
	MOVIE_FPS            = 30,
	MIN_VOXEL_INC        = 16,
	MAX_VOXEL_INC        = 1024,
	ATARI_INC            = 20,
	MAX_PICT_NUMBER      = 10,        // PICT numコマンドで送れるピクチャの数
	AUTO_CF_THRESHOULD   = 800,       // 自動床消しの閾値
};

}//namespace stclient
