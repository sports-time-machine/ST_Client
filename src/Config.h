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

#define LOOKAT_EYE_DEPTH   (4.0f)
#define IDLE_IMAGE_Z       (5.0f)



enum
{
	// これ以外についてはConfig.cppのConfig::Config()に記述してあります
	INITIAL_WIN_SIZE_X   = 640,
	INITIAL_WIN_SIZE_Y   = 480,
	MOVIE_MAX_SECS       = 50,
	MOVIE_FPS            = 30,
	MOVIE_MAX_FRAMES     = MOVIE_MAX_SECS * MOVIE_FPS,
	MIN_VOXEL_INC        = 16,
	MAX_VOXEL_INC        = 128,
	ATARI_INC            = 20,
	MAX_PICT_NUMBER      = 10,        // PICT numコマンドで送れるピクチャの数
};


struct Config
{
	struct NamedImage
	{
		string     fullpath;
		mi::Image  image;

		void reload(const char* title)
		{
			printf("%s '%s'\n", title, fullpath.c_str());
			image.createFromImageA(fullpath);
		}
	};
	struct RunEnv
	{
		NamedImage background;
	};

	typedef std::map<string,mgl::glRGBA> PlayerColors;
	typedef std::map<int,NamedImage> IdleImages;
	typedef std::map<string,RunEnv> RunEnvs;

	struct Colors
	{
		mgl::glRGBA
			default_player_color,
			ground, grid,
			movie1, movie2, movie3,
			snapshot,
			text_h1, text_p, text_em, text_dt, text_dd;
		Colors();
	};

	struct Images
	{
		string dot;
		string sleep;
		string pic[MAX_PICT_NUMBER];
	};
	struct Metrics
	{
		int left_mm;
		int right_mm;
		int top_mm;
		int ground_px;
	};

	// 全体的な設定
	PlayerColors player_colors;
	IdleImages   idle_images;
	RunEnvs      run_env;
	Images       images;
	Colors       color;
	int          center_atari_voxel_threshould;     // アタリ中央をとるために必要なボクセル数（1K単位ぐらい）
	int          hit_threshold;
	int          snapshot_life_frames;
	int          auto_snapshot_interval;
	float        person_dot_px;
	string       movie_folder;
	string       picture_folder;

	// クライアント個別の設定
	string       server_name;
	int          person_inc;
	int          movie_inc;
	int          client_number;
	bool         enable_kinect;  //Kinectを使う
	bool         enable_color;   //Kinectのカラーを初期化する
	bool         initial_fullscreen;
	bool         mirroring;
	bool         ignore_udp;
	CamParam     cam1;
	CamParam     cam2;
	Metrics      metrics;//#?

	float getScreenLeftMeter()  const  { return (client_number-1) * GROUND_WIDTH; }
	float getScreenRightMeter() const  { return getScreenLeftMeter() + GROUND_WIDTH; }
	
	static RunEnv* getDefaultRunEnv()  { static RunEnv re; return &re; }

	Config();

	void reload();

private:
	// call by reload()
	void ApplyIdleImages(const string& folder, PSLv var);
};

#ifdef THIS_IS_MAIN
#define SmartExtern /*nop*/
#else
#define SmartExtern extern
#endif

SmartExtern Config _rw_config;
static const Config& config = _rw_config;

}//namespace stclient
