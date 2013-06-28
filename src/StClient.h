#pragma once
#include <OpenNI.h>
#include "mi/mi.h"
#include "FreeType.h"
#include "Config.h"
#include "vec4.h"
#include "file_io.h"
#include "gl_funcs.h"
#include "psl_if.h"
#pragma warning(disable:4244) //conversion


namespace stclient{

using namespace mgl;


const float PI = 3.14159265f;

#define WITHOUT_KINECT 1

#define MIN_NUM_CHUNKS(data_size, chunk_size)	((((data_size)-1) / (chunk_size) + 1))
#define MIN_CHUNKS_SIZE(data_size, chunk_size)	(MIN_NUM_CHUNKS(data_size, chunk_size) * (chunk_size))

const int UDP_CLIENT_TO_CONTROLLER = 38702;
const int UDP_CONTROLLER_TO_CLIENT = 38708;



enum MovieMode
{
	MOVIE_READY,
	MOVIE_RECORD,
	MOVIE_PLAYBACK,
};

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


namespace Msg
{
	extern void BarMessage   (const string&, int width=70, int first_half=3);
	extern void Notice       (const string&);
	extern void SystemMessage(const string&);
	extern void ErrorMessage (const string&);
	extern void Notice       (const string&, const string&);
	extern void SystemMessage(const string&, const string&);
	extern void ErrorMessage (const string&, const string&);
}



struct RawDepthImage
{
	std::vector<uint16> image;
	uint16 min_value;
	uint16 max_value;
	uint16 range;        // max_value - min_value

	RawDepthImage()
	{
		image.resize(640*480);
		min_value = 0;
		max_value = 0;
		range = 0;
	}

	void CalcDepthMinMax();
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
	openni::Device device;
	openni::VideoStream depth;
	openni::VideoStream color;

	openni::VideoFrameRef depthFrame;
	openni::VideoFrameRef colorFrame;

	RawDepthImage raw_depth;
	RawDepthImage raw_floor;
	RawDepthImage raw_cooked;
	RawDepthImage raw_snapshot;

	uint vram_tex;
	uint vram_floor;

	void initRam();

	void CreateRawDepthImage_Read();
	void CreateRawDepthImage();

	void clearFloorDepth();
	void updateFloorDepth();

	void CreateCookedImage();
};

struct Mode
{
	bool mirroring;

	Mode()
	{
	}
};


struct Eye
{
	Eye()
	{
		go_pos = -1;
		fast_set = true;
	}

	float x,y,z;    // 視線の原点
	float rh;       // 視線の水平方向(rad)
	float v;        // 視線の垂直方向

	struct Go
	{
		float x,y,z,rh,v;
	} from,to;
	int go_pos;
	bool fast_set;

	enum { GO_FRAMES=25 };

	void set(float go_x, float go_y, float go_z, float go_rh, float go_v)
	{
		if (fast_set)
		{
			this->x  = go_x;
			this->y  = go_y;
			this->z  = go_z;
			this->rh = go_rh;
			this->v  = go_v;
		}
		else
		{
			this->from.x  = this->x;
			this->from.y  = this->y;
			this->from.z  = this->z;
			this->from.rh = this->rh;
			this->from.v  = this->v;
			this->to.x  = go_x;
			this->to.y  = go_y;
			this->to.z  = go_z;
			this->to.rh = go_rh;
			this->to.v  = go_v;
			this->go_pos = GO_FRAMES;
		}
	}

	void updateCameraMove()
	{
		if (go_pos>=0)
		{
			const float i =
				(go_pos==GO_FRAMES)
					?  1.0f
					: (1.0f * go_pos  / GO_FRAMES);
			const float j =
				(go_pos==0)
					?  1.0f
					: (1.0f * (GO_FRAMES-go_pos) / GO_FRAMES);
			x  = j*to.x  + i*from.x;
			y  = j*to.y  + i*from.y;
			z  = j*to.z  + i*from.z;
			rh = j*to.rh + i*from.rh;
			v  = j*to.v  + i*from.v;
			--go_pos;
		}
	}

	void gluLookAt();

	void view_2d_left();
	void view_2d_top();
	void view_2d_front();
	void view_2d_run();
	void view_3d_left();
	void view_3d_right();
	void view_3d_front();
};


struct ChangeCalParamKeys
{
	bool rot_xy, rot_z, scale, ctrl;

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

// ゲームひとつひとつの情報
struct GameInfo
{
	string     basename;      // ${BaseFolder}/3/2/Z/Y/X/0/0/0/0/0/00000XYZ23
	MovieData  movie;
	MovieData  partner1;
	MovieData  partner2;
	MovieData  partner3;
	mi::File   movie_file;

	// ゲーム情報の破棄、初期化
	void init();

	GameInfo()
	{
		init();
	}

	static string GetFolderName(const string& id);
	static string GetMovieFileName(const string& id);

	bool prepareForSave(const string& player_id, const string& game_id);
	void save();

private:
	void save_Movie(const string& basename);
	void save_Thumbnail(const string& basename, const string& suffix, int );
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
		mi::Image idle;
		mi::Image background;
		mi::Image sleep;
		mi::Image dot;
		mi::Image pic[MAX_PICT_NUMBER];
	};
	struct Calibration
	{
		bool fast;               // キャリブレーションのときの移動・回転速度
		bool enabled;            // キャリブレーション可能
	};

	Calibration  calibration;
	Debug        debug;
	GameInfo     gameinfo;
	View         view;
	ViewMode     view_mode;
	int          window_w;
	int          window_h;
	Images       images;
	int          picture_number;          // PICTコマンドのときに表示するピクチャ番号
	Point3D      person_center;
	int          frame_index;
	bool         frame_auto_increment;
	PSL::PSLVM   pslvm;
	int          hit_stage;
	PSLv         on_hit_setup;
	HitObjects   hit_objects;
	bool         show_debug_info;
	mgl::glRGBA  color_overlay;
	size_t       idle_image_number;

	bool calibrating_now() const
	{
		switch (view_mode)
		{
		case VM_2D_TOP:
		case VM_2D_LEFT:
		case VM_2D_FRONT:
			return true;
		}
		return false;
	}

	Global()
	{
		view_mode            = VM_2D_RUN;
		window_w             = 0;
		window_h             = 0;
		frame_index          = 0;
		frame_auto_increment = false;
		hit_stage            = 0;
		show_debug_info      = false;
		calibration.fast     = false;
		calibration.enabled  = false;
		idle_image_number    = 0;
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

#include "Config.h"  // EXTERN


SmartExtern Global       global;
SmartExtern Mode         mode;
SmartExtern TimeProfile  time_profile;


namespace Depth10b6b{
	void record(const RawDepthImage& depth1, const RawDepthImage& depth2, MovieData::Frame& dest_frame);
	void playback(RawDepthImage& dest1, RawDepthImage& dest2, const MovieData::Frame& frame);
}//namespace VoxelRecorder

namespace VoxelRecorder{
	void record(const Dots& dots, MovieData::Frame& dest_frame);
	void playback(Dots& dots, const MovieData::Frame& frame);
}//namespace VoxelRecorder

class StClient
{
public:
	StClient(Kdev& dev1, Kdev& dev2);
	virtual ~StClient();

	bool init();
	bool run();

	// コマンドクラスからよばれます
	void startMovieRecordSettings();
	void initGameInfo();

	void clearFloorDepth()
	{
		dev1.clearFloorDepth();
		dev2.clearFloorDepth();
	}

	const char* getStatusName(int present=-1) const;
	void changeStatus(ClientStatus st);
	ClientStatus clientStatus() const      { return this->_private_client_status; }

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


	bool initGraphics();

	void display();
	void displayEnvironment();
	void display3dSectionPrepare();
	void display3dSection();
	void display2dSectionPrepare();
	void display2dSection();
	void displayDebugInfo();
	void processOneFrame();

	void processKeyInput();
	bool processKeyInput_Calibration(int key);
	void processMouseInput();

	void processUdpCommands();

	void displayBlackScreen();
	void displayPictureScreen();

	void recordingStart();
	void recordingStop();
	void recordingReplay();
	bool recordingNow() const;
	bool replayingNow() const;

	void createSnapshot();

	void BuildDepthImage(uint8* dest);
	void do_calibration(float mx, float my);

	void loadAgent(int slot);

	void MovieRecord();
	void DrawRealMovie(Dots& dots);
	void set_clipboard_text();

	void reloadResources();

	void draw2dWall();
	void draw3dWall();
	void drawFieldGrid(int size_cm);
	void drawIdleImage();

	void CreateAtari(const Dots& dots);
	void CreateAtariFromBodyCenter();
	void CreateAtariFromDepthMatrix(const Dots& dots);
};

const char* to_s(int x);



enum DrawVoxelsStyle
{
	DRAW_VOXELS_NORMAL = 0,
	DRAW_VOXELS_HALF = 1,
	DRAW_VOXELS_QUAD = 2,
};

void MixDepth(Dots& dots, const RawDepthImage& src, const CamParam& cam);
void drawVoxels(const Dots& dots, glRGBA inner_color, glRGBA outer_color, DrawVoxelsStyle style = DRAW_VOXELS_NORMAL, float add_z=0.0f);
void myGetKeyboardState(BYTE* kbd);

}//namespace stclient



extern bool load_config();
