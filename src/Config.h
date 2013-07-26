#pragma once
#include "gl_funcs.h"
#include "file_io.h"
#include "psl_if.h"
#include "mi/Core.h"
#include "mi/Image.h"
#include "ConstValue.h"
#include "St3dData.h"

namespace stclient{

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
	int          whitemode_voxel_threshould;
	bool         debug_info_text;
	bool         debug_atari_ball;
	bool         auto_cf_enabled;
	int          person_base_alpha;
	float        partner_y;
	int          max_movie_second;
	float        obi_top_ratio;
	float        obi_bottom_ratio;
	int          auto_cf_threshould;

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
	int getMaxMovieFrames() const      { return max_movie_second * MOVIE_FPS; }
	
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
