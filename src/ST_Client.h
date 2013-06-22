#pragma once
#include <OpenNI.h>
#include "mi/mi.h"
#include "FreeType.h"
#include "Config.h"
#include "vec4.h"
#include "file_io.h"
#include "gl_funcs.h"
#pragma warning(disable:4244) //conversion


namespace stclient{


const float PI = 3.141592653;

#define WITHOUT_KINECT 1

#define MIN_NUM_CHUNKS(data_size, chunk_size)	((((data_size)-1) / (chunk_size) + 1))
#define MIN_CHUNKS_SIZE(data_size, chunk_size)	(MIN_NUM_CHUNKS(data_size, chunk_size) * (chunk_size))

enum VariousConstants
{
	MIN_VOXEL_INC = 16,
	MAX_VOXEL_INC = 128,
	ATARI_INC = 20,
};


const int UDP_SERVER_RECV = 38702;
const int UDP_CLIENT_RECV = 38708;



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
	// Idle
	STATUS_IDLE,

	// Demo status
	STATUS_BLACK,
	STATUS_PICTURE,
	STATUS_DEPTH,

	// Main status
	STATUS_GAMEREADY,   // IDENTを受けてから
	STATUS_GAME,        // STARTしてから

	// Game end status
	STATUS_TIMEOUT,
	STATUS_GAMESTOP,
	STATUS_GOAL,
};

enum ActiveCamera
{
	CAM_A,
	CAM_B,
	CAM_BOTH,
};


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
	int          hit_id;

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

	void inc(int x, int y)
	{
		if (inner(x,y))
		{
			++hit[x + y*CEL_W];
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

	uint vram_tex;
	uint vram_floor;

	void initRam();

	void CreateRawDepthImage_Read();
	void CreateRawDepthImage();
	void saveFloorDepth();
};

struct Mode
{
	bool auto_clipping;
	bool show_hit_boxes;
	bool mixed_enabled;//#?
	bool mirroring;
	bool borderline;//#?

	Mode()
	{
		auto_clipping = true;
	}
};


struct Eye
{
	float x,y,z;    // 視線の原点
	float rh;       // 視線の水平方向(rad)
	float v;        // 視線の垂直方向

	void set(float x, float y, float z, float h, float v)
	{
		this->x  = x;
		this->y  = y;
		this->z  = z;
		this->rh = h;
		this->v  = v;
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

struct Global
{
	ViewMode view_mode;
	int window_w;
	int window_h;
	ClientStatus client_status;
	mi::Image pic;
	mi::Image background_image;
	mi::Image dot_image;
	mi::File save_file;

	struct 
	{
		struct View2D
		{
			double width;
		} view2d;
		bool is_2d_view;
	} view;

	Global()
	{
		view_mode = VM_2D_LEFT;
		window_w = 0;
		window_h = 0;
		client_status = STATUS_DEPTH;
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


SmartExtern Global global;
SmartExtern Mode mode;
SmartExtern TimeProfile time_profile;


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

	bool init(int argc, char **argv);
	bool run();

private:
	void display();
	void displayEnvironment();
	void display3dSectionPrepare();
	void display3dSection();
	void display2dSectionPrepare();
	void display2dSection();


	enum MouseButton
	{
		MOUSE_LEFT  = 1,
		MOUSE_RIGHT = 2,
	};

	void processKeyInput();
	void processMouseInput();
	void processMouseInput_aux();

	bool initOpenGL(int argc, char **argv);

	Kdev& dev1;
	Kdev& dev2;

private:
	void displayBlackScreen();
	void displayPictureScreen();

	bool doCommand();
	bool doCommand2(const std::string& line);

private:
	StClient(const StClient&);           // disable
	StClient& operator=(StClient&);      // disable

	void BuildDepthImage(uint8* dest);

	void do_calibration(float mx, float my);


	unsigned int		m_nTexMapX;
	unsigned int		m_nTexMapY;

	uint vram_tex2;

	int			m_width;
	int			m_height;

	void CreateCoockedDepth(RawDepthImage& raw_cooked, const RawDepthImage& raw_depth, const RawDepthImage& raw_floor);

	mi::UdpReceiver       udp_recv;
	mi::UdpSender         udp_send;
	Eye                   eye;
	ActiveCamera          active_camera;
	freetype::font_data   monospace;
	MovieData             curr_movie;
	Calset                cal_cam1, cal_cam2;
	mi::Fps               fps_counter;
	int                   movie_index;
	MovieMode             movie_mode;
	HitData               hitdata;
	HitObjects            hit_objects;
	int                   flashing;

	void saveAgent(int slot);
	void loadAgent(int slot);

	void display2();
	void MoviePlayback();
	void MovieRecord();
	void DrawVoxels(Dots& dots);
	void CreateAtari(const Dots& dots);
	void set_clipboard_text();

	void clearFloorDepth();
	void reloadResources();
	void drawWall();
	void drawFieldGrid(int size_cm);
};

}//namespace stclient



extern void toggle(bool& ref);
extern void load_config();
