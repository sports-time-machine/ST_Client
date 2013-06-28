#include <OpenNI.h>
#include "StClient.h"
#include "Config.h"
#include "psl_if.h"

using namespace mi;
using namespace stclient;


Config::Colors::Colors()
{
	ground .set( 80, 40, 20);
	grid   .set(200,150,130);
	movie1 .set(120, 50, 50,200);
	movie2 .set( 50,120, 50,200);
	movie3 .set( 50, 50,120,200);
	text_h1.set(200,200,200);
	text_p .set(200,200,200);
	text_em.set(200,200,200);
	text_dt.set(200,200,200);
	text_dd.set(200,200,200);
}

Config::Config()
{
	// 全体的な設定
	center_atari_voxel_threshould = 2500;
	hit_threshold          = 10;
	snapshot_life_frames   = 100;
	auto_snapshot_interval = 0;
	person_dot_px          = 1.5f;

	// 個別の設定
	client_number        = -1;
	initial_fullscreen   = false;
	mirroring            = false;
	ignore_udp           = false;
	metrics.ground_px    = 480;
	metrics.left_mm      = 0;
	metrics.right_mm     = 4000;
	metrics.top_mm       = 2500;
	enable_kinect        = true;
	enable_color         = false;

	server_name          = "localhost";
	person_inc           = 32;
	movie_inc            = 64;
	client_number        = 0;
	enable_kinect        = true;
	enable_color         = false;//#?
	initial_fullscreen   = false;
	mirroring            = false;
	ignore_udp           = false;
}




static bool var_exist(PSL::variable& var)
{
	using namespace PSL;
	if (var.length()!=0)              return true;  //array
	if (var.memberLength()!=0)        return true;  //hash
	if (var.type()==variable::INT)    return true;
	if (var.type()==variable::FLOAT)  return true;
	if (var.type()==variable::HEX)    return true;
	if (var.type()==variable::STRING) return true;
	return false;
}

static bool var_exist(PSL::PSLVM& vm, const char* name)
{
	PSL::variable var = vm.get(name);
	return var_exist(var);
}


//================================
// コンフィグ用PSLをロードして実行
//================================
static bool load_config_and_run(PSL::PSLVM& psl, Config& config)
{
	auto load_psl = [&](PSL::string path)->bool{
		fprintf(stderr, "[PSL] Load '%s'...", path.c_str());
		auto err = psl.loadScript(path);
		if (err==PSL::PSLVM::NONE)
		{
			fprintf(stderr, "Ok\n");
			return true;
		}
		switch (err)
		{
		case PSL::PSLVM::FOPEN_ERROR:
			fprintf(stderr, "Cannot open file '%s'\n", path.c_str());
			return false;
		case PSL::PSLVM::PARSE_ERROR:
			fprintf(stderr, "PSL parse error (Syntax error?)\n");
			return false;
		default:
			fprintf(stderr, "PSL unknown error\n");
			return false;
		}
	};

	// いったんconfig.pslを読み込み、サーバー名を得る
	auto client_config = PSL::string("C:/ST/")+Core::getComputerName().c_str()+".psl";
	if (!load_psl(client_config))
	{
		Core::dialog("クライアントコンフィグのロードエラー");
		return false;
	}

	psl.run();
	PSLv server_name(psl.get("server_name"));
	if (server_name.type()!=PSLv::STRING)
	{
		Core::dialog("クライアントコンフィグにglobal server_nameの記載がないか、文字列ではありませんでした。");
		return false;
	}

	config.server_name = server_name.toString().c_str();

	// 共通コンフィグのロード
	auto server_config = PSL::string("//")+server_name.toString()+"/ST/Config.psl";
	if (!load_psl(server_config))
	{
		Core::dialog("Config.psl load error.");
		return false;
	}
	// クライアントコンフィグの再ロード
	if (!load_psl(client_config))
	{
		Core::dialog("(client-name).psl load error.");
		return false;
	}

	// 最終的な実行
	psl.run();
	return true;
}


//================================
// PSLデータをコンフィグに適用する
//================================
static void apply_psl_to_config(PSL::PSLVM& psl, Config& config)
{
#define CONFIG_LET2(DEST,NAME,C,FUNC)   if(var_exist(psl,#NAME)){ DEST=(C)PSL::variable(psl.get(#NAME)).FUNC(); }
#define CONFIG_LET(DEST,NAME,C,FUNC)   CONFIG_LET2(DEST.NAME, NAME, C, FUNC)
#define CONFIG_INT(DEST,NAME)          CONFIG_LET(DEST,NAME,int,toInt)
#define CONFIG_BOOL(DEST,NAME)         CONFIG_LET(DEST,NAME,bool,toBool)
#define CONFIG_FLOAT(DEST,NAME)        CONFIG_LET(DEST,NAME,float,toDouble)
	CONFIG_INT(config, client_number);
	CONFIG_INT(config, center_atari_voxel_threshould);
	CONFIG_BOOL(config, initial_fullscreen);
	CONFIG_BOOL(config, mirroring);
	printf("mirroring: %d\n", config.mirroring);

	// Metrics
	double left_meter  = 0.0f;
	double right_meter = 0.0f;
	double top_meter   = 0.0f;
	int    ground_px   = 0;
	CONFIG_LET2(top_meter,   metrics_topt_meter,  float, toDouble);
	CONFIG_LET2(ground_px,   metrics_ground_px,   int,   toInt);
	config.metrics.left_mm   = (int)(1000 * left_meter);
	config.metrics.right_mm  = (int)(1000 * right_meter);
	config.metrics.top_mm    = (int)(1000 * top_meter);
	config.metrics.ground_px = ground_px;

	auto set_rgb = [&](PSL::variable src, mgl::glRGBA& dest){
		if (!var_exist(src))
			return;
		int alpha = src["a"].toInt();
		if (alpha==0) alpha=255;
		dest.set(
			src["r"].toInt(),
			src["g"].toInt(),
			src["b"].toInt(),
			alpha);		
	};
	auto set_camera_param = [&](CamParam& cam, const char* varname){
		PSL::variable var = psl.get(varname);
		cam.x     = (float)var["x"];
		cam.y     = (float)var["y"];
		cam.z     = (float)var["z"];
		cam.rotx  = (float)var["rotx"];
		cam.roty  = (float)var["roty"];
		cam.rotz  = (float)var["rotz"];
		cam.scale = (float)var["scale"];
	};
	auto toString = [&](PSLv var)->const char*{
		if (var==PSLv(PSLv::NIL))
		{
			return "";
		}
		return var.toString().c_str();
	};
	auto pslString = [&](const char* name)->const char*{
		return PSL::variable(psl.get(name)).toString().c_str();
	};

	//=== GLOBAL VARS ===
	global.on_hit_setup = psl.get("onHitSetup");

	//=== LOCAL SETTINGS ===
	CONFIG_INT(config, person_inc);
	CONFIG_INT(config, movie_inc);
	CONFIG_INT(config, hit_threshold);
	CONFIG_INT(config, snapshot_life_frames);
	CONFIG_BOOL(config, ignore_udp);
	set_camera_param(config.cam1, "camera1");
	set_camera_param(config.cam2, "camera2");
	{
#define DEF_IMAGE(NAME) dest.NAME = toString(src[#NAME])
		// Images
		PSLv src = psl.get("images");
		auto& dest = config.images;
		DEF_IMAGE(background);
		DEF_IMAGE(sleep);
		DEF_IMAGE(idle);
		for (int i=0; i<MAX_PICT_NUMBER; ++i)
		{
			string pic_no = (string("pic") + to_s(i));
			config.images.pic[i] = toString(src[pic_no.c_str()]);
		}
#undef DEF_IMAGE
	}

	//=== GLOBAL SETTINGS ===
	auto& gc = config;
	CONFIG_BOOL(gc, enable_kinect);
	CONFIG_BOOL(gc, enable_color);

	//=== COLORS ===
	{
		PSLv src = psl.get("player_colors");
		PSLv names = src.keys();
		for (size_t i=0; i<names.length(); ++i)
		{
			PSLv key = names[i];
			set_rgb(src[key], config.player_colors[key.c_str()]);
		}
	}
	{
		PSLv colors = psl.get("colors");
		set_rgb(colors["default_player_color"],   gc.color.default_player_color);
		set_rgb(colors["ground"],   gc.color.ground);
		set_rgb(colors["grid"],     gc.color.grid);
		set_rgb(colors["movie1"],   gc.color.movie1);
		set_rgb(colors["movie2"],   gc.color.movie2);
		set_rgb(colors["movie3"],   gc.color.movie3);
		set_rgb(colors["text_h1"],  gc.color.text_h1);
		set_rgb(colors["text_p"],   gc.color.text_p);
		set_rgb(colors["text_em"],  gc.color.text_em);
		set_rgb(colors["text_dt"],  gc.color.text_dt);
		set_rgb(colors["text_dd"],  gc.color.text_dd);
		set_rgb(colors["snapshot"], gc.color.snapshot);
	}
	CONFIG_FLOAT(config, person_dot_px);
	CONFIG_INT(config, auto_snapshot_interval);

#undef CONFIG_INT
#undef CONFIG_BOOL
}


//====================================
// コンフィグファイルのロード
//====================================
bool load_config()
{
	PSL::PSLVM& psl = global.pslvm;

	if (!load_config_and_run(psl, _rw_config))
	{
		return false;
	}

	apply_psl_to_config(psl, _rw_config);
	return true;
}
