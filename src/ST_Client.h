#pragma once
#include <OpenNI.h>
#include "mi/Core.h"
#include "mi/Udp.h"
#include "mi/Image.h"
#include "Config.h"
#include "vec4.h"


const float PI = 3.141592653;

#define WITHOUT_KINECT 1

#define MIN_NUM_CHUNKS(data_size, chunk_size)	((((data_size)-1) / (chunk_size) + 1))
#define MIN_CHUNKS_SIZE(data_size, chunk_size)	(MIN_NUM_CHUNKS(data_size, chunk_size) * (chunk_size))


const int UDP_SERVER_RECV = 38702;
const int UDP_CLIENT_RECV = 38708;


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

struct RgbaTex
{
	RGBA_raw* vram;
	uint tex;
	uint width;
	uint height;
	uint ram_width, ram_height;
	uint pitch;     // f(2^X, width)

	RgbaTex();
	virtual ~RgbaTex();

	void create(int w, int h);
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

	RgbaTex  img_rawdepth;

	uint vram_tex;
	uint vram_floor;

	void initRam();

	void CreateRawDepthImage_Read();
	void CreateRawDepthImage();
	void saveFloorDepth();
};

struct Mode
{
	bool simple_dot_body;
	bool auto_clipping;
	bool show_hit_boxes;
	bool sync_enabled;
	bool mixed_enabled;
	bool mirroring;
	bool borderline;
	bool view4test;

	Mode()
	{
		auto_clipping = true;
		simple_dot_body = true;
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


class StClient
{
public:
	StClient(Kdev& dev1, Kdev& dev2);
	virtual ~StClient();

	virtual bool init(int argc, char **argv);
	virtual bool run();	//Does not return

protected:
	virtual void display();
	virtual void displayPostDraw(){};	// Overload to draw over the screen image

	virtual void onKey(int key, int x, int y);
	virtual void onMouse(int button, int state, int x, int y);
	virtual void onMouseMove(int x, int y);

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

	static StClient* ms_self;
	static void glutIdle();
	static void glutDisplay();
	static void glutKeyboard(unsigned char key, int x, int y);
	static void glutKeyboardSpecial(int key, int x, int y);
	static void glutMouse(int button, int state, int x, int y);
	static void glutMouseMove(int x, int y);
	static void glutReshape(int width, int height);

	unsigned int		m_nTexMapX;
	unsigned int		m_nTexMapY;

	RGBA_raw* video_ram;

	RGBA_raw* video_ram2;
	uint vram_tex2;

	int			m_width;
	int			m_height;

	void CreateCoockedDepth(RawDepthImage& raw_cooked, const RawDepthImage& raw_depth, const RawDepthImage& raw_floor);

	mi::UdpReceiver udp_recv;
	mi::UdpSender   udp_send;


	Eye     eye;


	void drawPlaybackMovie();
	void display2();
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
		struct Ortho
		{
			double width;
		} ortho;
		struct Perspective
		{
		} perspective;
		bool is_ortho;
	} view;

	Global()
	{
		view_mode = VM_2D_LEFT;
		window_w = 0;
		window_h = 0;
		client_status = STATUS_DEPTH;
	}
};

#include "Config.h"  // EXTERN


SmartExtern Global global;
SmartExtern Mode mode;


extern void load_config();
extern void toggle(bool& ref);
