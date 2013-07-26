#pragma once
#include <OpenNI.h>
#include "mi/mi.h"
#include "FreeType.h"
#include "Config.h"
#include "file_io.h"
#include "gl_funcs.h"
#include "psl_if.h"
#include "St3dData.h"
#pragma warning(disable:4244) //conversion


namespace stclient{

using namespace mgl;


const float PI = 3.14159265f;

#define WITHOUT_KINECT 1

#define MIN_NUM_CHUNKS(data_size, chunk_size)	((((data_size)-1) / (chunk_size) + 1))
#define MIN_CHUNKS_SIZE(data_size, chunk_size)	(MIN_NUM_CHUNKS(data_size, chunk_size) * (chunk_size))

const int UDP_CLIENT_TO_CONTROLLER = 38702;
const int UDP_CONTROLLER_TO_CLIENT = 38708;


// WINNT.H
#undef STATUS_TIMEOUT

enum ClientStatus
{
	STATUS_SLEEP,
	STATUS_IDLE,
	STATUS_PICT,
	STATUS_BLACK,
	STATUS_READY,
	STATUS_GAME,
	STATUS_REPLAY,
	STATUS_SAVING,
	STATUS_LOADING,
	STATUS_INIT_FLOOR,
};

enum ActiveCamera
{
	CAM_A,
	CAM_B,
	CAM_BOTH,
};


struct HitObject
{
	bool         enable;
	mi::Point    point;
	mgl::glRGBA  color;
	int          next_id;
	string       text;

	HitObject():
		enable(true)
	{
	}
};

typedef std::vector<HitObject> HitObjects;

class HitData
{
private:
	static const int AREA_W = 400; // 400cm
	static const int AREA_H = 300; // 300cm

public:
	static const int CEL_W  = AREA_W/10;
	static const int CEL_H  = AREA_H/10;

	static bool inner(int x, int y)
	{
		return (uint)x<CEL_W && (uint)y<CEL_H;
	}

	int get(int x, int y) const
	{
		if (!inner(x,y))
		{
			return 0;
		}
		return hit[x + y*CEL_W];
	}

	void inc(int x, int y, int gain=1)
	{
		if (inner(x,y))
		{
			hit[x + y*CEL_W] += gain;
		}
	}

	void clear()
	{
		memset(hit, 0, sizeof(hit));
	}

private:
	// 10cm3 box
	int hit[CEL_W * CEL_H];
};

struct Kdev
{
	openni::Device         device;
	openni::VideoStream    depth;
	openni::VideoStream    color;
	openni::VideoFrameRef  depthFrame;
	openni::VideoFrameRef  colorFrame;
	RawDepthImage          raw_depth;
	RawDepthImage          raw_floor;
	RawDepthImage          raw_cooked;
	RawDepthImage          raw_snapshot;

	void initRam();
	void CreateRawDepthImage_Read();
	void CreateRawDepthImage();
	void clearFloorDepth();
	void updateFloorDepth();
	void CreateCookedImage();
};

struct ChangeCalParamKeys
{
	bool rot_xy, rot_z, scale, scalex, ctrl;

	void init();
};

struct Calset
{
	CamParam curr, prev;
};


enum ViewMode
{
	VM_2D_TOP,      // 見下ろし：Z軸調整用
	VM_2D_LEFT,     // 左から：　Y軸調整用
	VM_2D_FRONT,    // 前から：　X軸調整用
	VM_2D_RUN,

	VM_3D_LEFT,
	VM_3D_RIGHT,
	VM_3D_FRONT,
};

struct MoiInitStruct
{
	bool  reverse;
	float top_speed;
	float accel_second;
	float break_rate;
	float size_meter;
	int   anim_speed;
	float disp_y;
};

// チーターなど実体
class MovingObjectImage
{
	friend class MovingObject;
public:
	MovingObjectImage();

	// 初期化
	void init(const string& id, const MoiInitStruct& mis);
	void addFrame(int length, const string& filepath);

	bool  isReverse()      const     { return _mis.reverse;      }
	float getTopSpeed()    const     { return _mis.top_speed;    }
	float getAccelSecond() const     { return _mis.accel_second; }
	float getBreakRate()   const     { return _mis.break_rate;   }
	float getDisplayY()    const     { return _mis.disp_y;       }
	int   getAnimSpeed()   const     { return _mis.anim_speed;   }
	float getSizeMeter()   const     { return _mis.size_meter;   }

private:
	typedef std::map<int,mi::Image> Images;
	typedef std::vector<int> Frames;

	MoiInitStruct _mis;
	string  _id;               // M:CHEETAH-1
	Images  _images;
	Frames  _frames;
};

// チーターなど
class MovingObject
{
public:
	enum Stage
	{
		STAGE_RUN1,
		STAGE_BREAK,
		STAGE_RUN2,
		STAGE_GOAL,
	};

public:
	MovingObject();
	void init();
	void init(const MovingObjectImage& moi);
	bool enabled() const      { return _moi!=nullptr; }
	void initRunParam();

	// getter
	int convertRealFrameToVirtualFrame(int real_frame) const;
	const mi::Image& getFrameImage(int virtual_frame) const;
	float getDistance()      const;
	const MovingObjectImage& getMoi() const { return *_moi; }

	// 28.0fメートル目でターンする
	float getTurnPosition()  const  { return 28.0f; }

	// command
	void resetDistance();
	void updateDistance();
	
private:
	const MovingObjectImage* _moi;
	Stage  _stage;
	float  _distance_meter;   // 原点からの距離
	float  _speed;            // 毎フレームの移動速度 m/frame
};


struct Global
{
	struct View
	{
		struct View2D
		{
			double width;
		} view2d;
		bool is_2d_view;
	};
	struct Debug
	{
		bool recording;
		bool show_realmovie;
		bool show_replay;
		bool show_partner;
		int  atari_voxels;
	};
	struct Images
	{
		mi::Image sleep;
		mi::Image dot;
		mi::Image pic[MAX_PICT_NUMBER];
	};
	struct Calibration
	{
		bool fast;               // キャリブレーションのときの移動・回転速度
		bool enabled;            // キャリブレーション可能
	};
	typedef const Config::RunEnv* RunEnvPtr;
	typedef std::map<string,MovingObjectImage> MovinbObjectImages;
	
	bool           mirroring;
	int            idle_select;
	RunEnvPtr      run_env;
	Calibration    calibration;
	Debug          debug;
	GameInfo       gameinfo;
	View           view;
	ViewMode       view_mode;
	int            window_w;
	int            window_h;
	Images         images;
	int            picture_number;          // PICTコマンドのときに表示するピクチャ番号
	Point3D        person_center;
	int            frame_index;
	bool           frame_auto_increment;
	int            game_start_frame;
	bool           in_game_or_replay;
	PSL::PSLVM     pslvm;
	int            hit_stage;
	PSLv           on_hit_setup;
	HitObjects     hit_objects;
	bool           show_debug_info;
	bool           debug_atari_ball;
	mgl::glRGBA    color_overlay;
	size_t         idle_image_number;
	int            total_frames;
	int            auto_clear_floor_count;
	MovingObject   partner_mo;
	MovinbObjectImages moi_lib;

	Global()
	{
		mirroring              = false;
		idle_select            = 0;
		run_env                = Config::getDefaultRunEnv();
		view_mode              = VM_2D_RUN;
		window_w               = 0;
		window_h               = 0;
		frame_index            = 0;
		frame_auto_increment   = false;
		hit_stage              = 0;
		show_debug_info        = false;
		calibration.fast       = false;
		calibration.enabled    = false;
		idle_image_number      = 0;
		total_frames           = 0;
		auto_clear_floor_count = 0;
		in_game_or_replay      = false;
		game_start_frame       = 0;
		color_overlay.set(0,0,0,0);  // transparent
	}
};

struct TimeProfile
{
	struct Environment
	{
		double total;
		double read1;
		double read2;
	} environment;

	struct Drawing
	{
		double total;
		double wall;
		double grid;
		double mix1;
		double mix2;
		double drawvoxels;
	} drawing;

	double frame;
	double atari;

	struct Record
	{
		double total;
		double enc_stage1;
		double enc_stage2;
		double enc_stage3;
	} record;

	struct Playback
	{
		double total;
		double dec_stage1;
		double dec_stage2;
		double draw;
	} playback;
};

SmartExtern Global       global;
SmartExtern TimeProfile  time_profile;



class Eye : public EyeCore
{
public:
	void view_2d_left();
	void view_2d_top();
	void view_2d_front();
	void view_2d_run();
	void view_3d_left();
	void view_3d_right();
	void view_3d_front();
};

class StClient
{
public:
	StClient(Kdev& dev1, Kdev& dev2);
	virtual ~StClient();

	bool init1_GraphicsOnly();
	bool init2();
	bool run();

	// コマンドクラスからよばれます
	void startMovieRecordSettings();
	void initGameInfo();
	void drawOneFrame();

	void clearFloorDepth()
	{
		dev1.clearFloorDepth();
		dev2.clearFloorDepth();
	}

	const char* getStatusName(int present=-1) const;
	void changeStatus(ClientStatus st);
	ClientStatus clientStatus() const      { return this->_private_client_status; }
	void reloadResources();

private:
	StClient(const StClient&);           // disable
	StClient& operator=(StClient&);      // disable

	enum MouseButton
	{
		MOUSE_LEFT  = 1,
		MOUSE_RIGHT = 2,
	};

	Kdev&                 dev1;
	Kdev&                 dev2;
	mi::UdpReceiver       udp_recv;
	mi::UdpSender         udp_send;
	Eye                   eye;
	ActiveCamera          active_camera;
	freetype::font_data   monospace;
	Calset                cal_cam1, cal_cam2;
	mi::Fps               fps_counter;
	HitData               hitdata;
	int                   flashing;
	int                   snapshot_life;
	ClientStatus          _private_client_status;


	void display();
	void displayEnvironment();
	void display3dSectionPrepare();
	void display3dSection();
	void display2dSectionPrepare();
	void display2dSection();
	void displayDebugInfo();
	void displayBlackScreen();
	void displayPictureScreen();

	void processOneFrame();
	void processKeyInput();
	bool processKeyInput_Calibration(int key);
	void processMouseInput();
	void processUdpCommands();

	void recordingStart();
	void recordingStop();
	void recordingReplay();
	bool recordingNow() const;
	bool replayingNow() const;

	void createSnapshot();

	void BuildDepthImage(uint8* dest);
	void do_calibration(float mx, float my);

	void MovieRecord();
	void SaveCamConfig();
	void LoadCamConfig();

	void drawRunEnv();
	void draw3dWall();
	void drawFieldGrid(int size_cm);
	void drawRealDots(Dots& dots, glRGBA color_body, float dot_size);
	void drawIdleImage();
	void drawMovingObject();
	void drawNormalGraphics();
	void drawNormalGraphicsObi();
	void drawManyTriangles();

	void CreateAtari(const Dots& dots);
	void CreateAtariFromBodyCenter();
	void CreateAtariFromDepthMatrix(const Dots& dots);

	void initDrawParamFromGlobal(VoxGrafix::DrawParam& param, float dot_size);

public:
	static string GetCamConfigPath();
};


}//namespace stclient



extern bool load_config();
extern void load_moving_objects();
