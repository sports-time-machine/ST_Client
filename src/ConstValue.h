#pragma once
#include "gl_funcs.h"

namespace stclient{

// スポーツタイムマシンの定数群
//============================
// 1unit = 2kinects + 1client
static const float
	GROUND_WIDTH       = (4.00f),              // ユニットあたりの幅 (4m)
	GROUND_LEFT        = (-GROUND_WIDTH/2),    // 左端のメートル (-2m)
	GROUND_RIGHT       = (+GROUND_WIDTH/2),    // 右端のメートル (+2m)
	GROUND_HEIGHT      = (2.40f),              // ユニットの高さ (2.4m)
	GROUND_DEPTH       = (2.40f),              // ユニットの奥行き (2.4m)
	GROUND_NEAR        = (0.00f),              // ユニットの一番手前 (+0m)
	GROUND_FAR         = (GROUND_DEPTH),       // ユニットの一番奥 (+2.4f)
	GROUND_XNEAR       = (0.00f),              // 録画対象となる手前側
	GROUND_XFAR        = (GROUND_DEPTH-0.10f), // 録画対象となる最奥（壁から10cm分は無視する）
	GROUND_TOP         = (GROUND_HEIGHT),      // ユニットの上端
	GROUND_BOTTOM      = (0.00f),              // ユニットの下端
	GROUND_XTOP        = (GROUND_HEIGHT),      // 録画対象となる天井
	GROUND_XBOTTOM     = (0.00f);              // 録画対象となる床

// 当たり判定
//===========
// 床付近はノイズが多いので膝丈ぐらいからのみ有効
// 画面の左右にマージンを設けることで、画面外にいきつつある場合であっても、
// 人物の中央をとることができるようにする
// 人の肩幅はせいぜい50cmであるので、50cmマージンがあれば概ね問題ないと判断する
static const float
	ATARI_MARGIN       = (0.50f),
	ATARI_LEFT         = (GROUND_LEFT  - ATARI_MARGIN),
	ATARI_RIGHT        = (GROUND_RIGHT + ATARI_MARGIN),
	ATARI_BOTTOM       = (0.50f),
	ATARI_TOP          = (GROUND_HEIGHT),
	ATARI_NEAR         = (0.00f),
	ATARI_FAR          = (GROUND_DEPTH-0.20f);  //壁際はノイズが多いので省く

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
};

}//namespace stclient
