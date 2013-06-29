#include <OpenNI.h>
#include "StClient.h"
#include "Config.h"
#include "psl_if.h"

using namespace mi;
using namespace stclient;

#define SERVER_NAME_PSL "C:/ST/server_name.psl"

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
	movie_folder           = ""; //デフォルトママだとエラーになります
	picture_folder         = ""; //デフォルトママだとエラーになります
	center_atari_voxel_threshould = 7500;
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

	// サーバー名を得る
	if (!load_psl(SERVER_NAME_PSL))
	{
		Core::dialog(SERVER_NAME_PSL"のロードエラー");
		return false;
	}
	psl.run();
	PSLv server_name(psl.get("server_name"));
	if (server_name.type()!=PSLv::STRING)
	{
		Core::dialog(SERVER_NAME_PSL"にglobal server_nameの記載がないか、文字列ではありませんでした。");
		return false;
	}

	config.server_name = server_name.toString().c_str();

	// サーバーコンフィグのロード
	auto server_config = PSL::string("//")+server_name.toString()+"/ST/Config.psl";
	if (!load_psl(server_config))
	{
		Core::dialog("Config.psl load error.");
		return false;
	}
	// クライアントコンフィグの再ロード
	auto client_config = PSL::string("C:/ST/")+Core::getComputerName().c_str()+".psl";
	if (!load_psl(client_config))
	{
		Core::dialog("(client-name).psl load error.");
		return false;
	}

	// 最終的な実行
	psl.run();
	return true;
}


// idle_images = [file,file,...] をセット
static void ApplyIdleImages(Config::IdleImages& idle_images, const string& folder, PSLv var)
{
	idle_images.clear();

	for (size_t i=0; i<var.length(); ++i)
	{
		string fullpath = folder + var[i].toString().c_str();
		idle_images[i].fullpath = fullpath;
	}
}

// run_env = [name:[background:file], ...] をセット
static void ApplyRunEnvs(Config::RunEnvs& envs, const string& folder, PSLv var)
{
	envs.clear();

	PSLv keys = var.keys();
	for (size_t i=0; i<keys.length(); ++i)
	{
		PSL::string name = keys[i].toString().c_str();
		printf("[RunEnv] %s\n", name.c_str());

		auto& env = envs[name.c_str()];
		env.background.fullpath = folder + var[name]["background"].toString().c_str();
	}
}





//================================
// PSLデータをコンフィグに適用する
//================================
static void apply_psl_to_config(PSL::PSLVM& psl, Config& config)
{
#define CONFIG_LET2(DEST,NAME,C,FUNC)   if(var_exist(psl,#NAME)){ DEST=(C)PSL::variable(psl.get(#NAME)).FUNC; }
#define CONFIG_LET(DEST,NAME,C,FUNC)   CONFIG_LET2(DEST.NAME, NAME, C, FUNC)
#define CONFIG_INT(NAME)          CONFIG_LET(config,NAME,int,toInt())
#define CONFIG_BOOL(NAME)         CONFIG_LET(config,NAME,bool,toBool())
#define CONFIG_FLOAT(NAME)        CONFIG_LET(config,NAME,float,toDouble())
#define CONFIG_STRING(NAME)       CONFIG_LET(config,NAME,const char*,toString())
	CONFIG_INT(client_number);
	CONFIG_INT(center_atari_voxel_threshould);
	CONFIG_BOOL(initial_fullscreen);
	CONFIG_BOOL(mirroring);
	printf("mirroring: %d\n", config.mirroring);

	// Metrics
	double left_meter  = 0.0f;
	double right_meter = 0.0f;
	double top_meter   = 0.0f;
	int    ground_px   = 0;
	CONFIG_LET2(top_meter,   metrics_topt_meter,  float, toDouble());
	CONFIG_LET2(ground_px,   metrics_ground_px,   int,   toInt());
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

	// Import plain-old-data
	CONFIG_INT(person_inc);
	CONFIG_INT(movie_inc);
	CONFIG_INT(hit_threshold);
	CONFIG_INT(snapshot_life_frames);
	CONFIG_BOOL(enable_kinect);
	CONFIG_BOOL(enable_color);
	CONFIG_BOOL(ignore_udp);
	CONFIG_STRING(picture_folder);
	CONFIG_STRING(movie_folder);
	CONFIG_FLOAT(person_dot_px);
	CONFIG_INT(auto_snapshot_interval);

	//=== GLOBAL VARS ===
	global.on_hit_setup = psl.get("onHitSetup");

	set_camera_param(config.cam1, "camera1");
	set_camera_param(config.cam2, "camera2");
	{
#define DEF_IMAGE(NAME) dest.NAME = toString(src[#NAME])
		// Images
		PSLv src = psl.get("images");
		auto& dest = config.images;
		DEF_IMAGE(sleep);
		for (int i=0; i<MAX_PICT_NUMBER; ++i)
		{
			string pic_no = (string("pic") + to_s(i));
			config.images.pic[i] = toString(src[pic_no.c_str()]);
		}
#undef DEF_IMAGE
	}


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
		set_rgb(colors["default_player_color"], config.color.default_player_color);
		set_rgb(colors["ground"],   config.color.ground);
		set_rgb(colors["grid"],     config.color.grid);
		set_rgb(colors["movie1"],   config.color.movie1);
		set_rgb(colors["movie2"],   config.color.movie2);
		set_rgb(colors["movie3"],   config.color.movie3);
		set_rgb(colors["text_h1"],  config.color.text_h1);
		set_rgb(colors["text_p"],   config.color.text_p);
		set_rgb(colors["text_em"],  config.color.text_em);
		set_rgb(colors["text_dt"],  config.color.text_dt);
		set_rgb(colors["text_dd"],  config.color.text_dd);
		set_rgb(colors["snapshot"], config.color.snapshot);
	}

#undef CONFIG_INT
#undef CONFIG_BOOL
}


//====================================
// コンフィグファイルのロード
//====================================
#define VALIDATE(NAME) \
	if (config.NAME.empty()){\
		Core::dialog("コンフィグに" #NAME "がかかれていません");\
		return false;\
	}
bool load_config()
{
	PSL::PSLVM& psl = global.pslvm;

	if (!load_config_and_run(psl, _rw_config))
	{
		return false;
	}

	apply_psl_to_config(psl, _rw_config);

	// 検証
	VALIDATE(movie_folder);
	VALIDATE(picture_folder);

	// idle_images = [array];
	ApplyIdleImages(
		_rw_config.idle_images, 
		config.picture_folder,
		psl.get("idle_images"));
	ApplyRunEnvs(
		_rw_config.run_env, 
		config.picture_folder,
		psl.get("run_env"));

	return true;
}
