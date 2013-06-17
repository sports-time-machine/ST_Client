#ifndef _CRT_SECURE_NO_DEPRECATE 
	#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include "mi/Image.h"
#include "mi/Udp.h"
#include "mi/Libs.h"
#include "mi/Timer.h"
#include "FreeType.h"
#include "gl_funcs.h"
#include "file_io.h"
#include "Config.h"
#include <map>
#pragma warning(disable:4366)
#define GL_GENERATE_MIPMAP_SGIS 0x8191
#include "ST_Client.h"
#include <GL/glfw.h>
#pragma comment(lib,"GLFW_x32.lib")
#pragma warning(disable:4996) // unsafe function

#define USE_GLFW 0


#define local static



struct HitData
{
	bool hit[80*60];
};

local HitData hitdata;



struct Calset
{
	CamParam curr, prev;
};

local Calset cal_cam1, cal_cam2;

enum CameraMode
{
	CAM_A,
	CAM_B,
	CAM_BOTH,
};

local CameraMode camera_mode = CAM_BOTH;
local float eye_rh_base, eye_rv_base, eye_y_base;



void drawSphere(float x, float y, float z)
{
	static GLUquadricObj *sphere = nullptr;
	if (sphere==nullptr)
	{
		sphere = gluNewQuadric();
	}

	gluQuadricDrawStyle(sphere, GLU_LINE);
	glRGBA(255,255,255).glColorUpdate();
	glPushMatrix();
		glLoadIdentity();
		glTranslatef(x,y,z);
		gluSphere(sphere, 0.01, 10.0, 10.0);
	glPopMatrix();
}



const float PI = 3.141592653;


struct DepthLine
{
	uint16 big_depth[1024];
	int begin;
	int end;
	enum { INVALID=-1 };
};

typedef std::vector<DepthLine> DepthScreen;

local DepthScreen depth_screen;



struct TimeProfile
{
	double draw_wall;
	double draw_grid;

	double read1_depth_dev1;
	double read1_depth_dev2;
	double read2_depth_dev1;
	double read2_depth_dev2;

	double draw_depth;
};

local TimeProfile time_profile;



static int old_x, old_y;
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

	void gluLookAt()
	{
		const float eye_depth = 4.0f;
		const float ex = x + cos(rh) * eye_depth;
		const float ez = z + sin(rh) * eye_depth;
		const float ey = y + v;

		::gluLookAt(x,y,z, ex,ey,ez, 0,1,0);
	}
};

local Eye eye;



void view_2d_left()
{
	global.view.is_ortho = true;
	global.view.ortho.width = 4.5;
	global.view_mode = VM_2D_LEFT;
	eye.set(-10.0f, -0.2f, -1.5f, 0.0f, 0.0f);
}

void view_2d_top()
{
	global.view.is_ortho = true;
	global.view.ortho.width = 5.0;
	global.view_mode = VM_2D_TOP;
	eye.set(0.0f, 110.0, 5.2f, -PI/2, -100.0f);
}

void view_2d_front()
{
	global.view.is_ortho = true;
	global.view.ortho.width = 4.5;  // 少し広く
	global.view_mode = VM_2D_FRONT;
	eye.set(0.0f, -0.2f, 10.0f, -PI/2, 0.0f);
}

void view_2d_run()
{
	global.view.is_ortho = true;
	global.view.ortho.width = 4.0;
	global.view_mode = VM_2D_RUN;
	eye.set(0.0f, -0.4f, 5.0f, -PI/2, 0.0f);
}

void view_3d_left()
{
	global.view.is_ortho = false;
	global.view_mode = VM_3D_LEFT;
	eye.set(-2.9f, 1.5f, 3.6f, -1.03f, -0.82f);
}

void view_3d_right()
{
	global.view.is_ortho = false;
	global.view_mode = VM_3D_RIGHT;
	eye.set(2.9f, 1.5f, 3.6f, -2.11f, -0.82f);
}

void view_3d_front()
{
	global.view.is_ortho = false;
	global.view_mode = VM_3D_FRONT;
	eye.set(0.0f, 1.5f, 4.00f, -PI/2, -0.60f);
}




void Kdev::initRam()
{
	glGenTextures(1, &this->vram_tex);
	glGenTextures(1, &this->vram_floor);
	this->img_rawdepth.create(640,480);
}







struct HitObject
{
	Point point;
	glRGBA color;
};

local std::vector<HitObject> hit_objects;


struct VodyInfo
{
	int near_d;         // 最近デプス（255に近い）
	int far_d;          // 最遠デプス（0に近い）
	int raw_near_d;     // データ上の最近
	int raw_far_d;      // データ上の最近
	Box body;           // バーチャルボディ本体の矩形
	Box near_box;       // 近いもの（手とか）の矩形
	Box far_box;        // 遠いもの（体幹など）の矩形
	int total_pixels;   // バーチャルボディが占めるピクセル数
	int histogram[256]; // バーチャルボディのデプスヒストグラム
};

local VodyInfo vody;





class miFps
{
public:
	miFps();

	void update();
	float getFps() const;

private:
	static const int SIZE = 180;
	uint _frame_tick[SIZE];
	int _frame_index;
};


miFps::miFps()
{
	_frame_index = 0;
}

void miFps::update()
{
	_frame_index = (_frame_index+1) % SIZE;
	_frame_tick[_frame_index] = timeGetTime();
}

float miFps::getFps() const
{
	const int tail_index = (_frame_index - 1 + SIZE) % SIZE;
	const int tail = _frame_tick[tail_index];
	const int head = _frame_tick[_frame_index];
	if (tail==0)
	{
		return 0;
	}
	else
	{
		return 1000.0f / (head-tail);
	}
}


local miFps fps_counter;




enum MovieMode
{
	MOVIE_READY,
	MOVIE_RECORD,
	MOVIE_PLAYBACK,
};

local MovieMode movie_mode = MOVIE_READY;


#include "XnCppWrapper.h"
#include "OniSampleUtilities.h"

const int INITIAL_WIN_SIZE_X = 1024;
const int INITIAL_WIN_SIZE_Y = 768;
const int TEXTURE_SIZE = 512;

const int MOVIE_MAX_SECS = 50;
const int MOVIE_FPS = 30;
const int MOVIE_MAX_FRAMES = MOVIE_MAX_SECS * MOVIE_FPS;
const int PIXELS_PER_SCREEN = 640*480;


RgbaTex::RgbaTex()
{
	vram = nullptr;
	tex = 0;
	width = 0;
	height = 0;
	ram_width = 0;
	ram_height = 0;
	pitch = 0;
}

RgbaTex::~RgbaTex()
{
	if (vram!=nullptr) delete[] vram;
}

void RgbaTex::create(int w, int h)
{
	const int TEXTURE_SIZE = 512;

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	width      = w;
	height     = h;
	ram_width  = MIN_CHUNKS_SIZE(w, TEXTURE_SIZE);
	ram_height = MIN_CHUNKS_SIZE(h, TEXTURE_SIZE);
	pitch      = ram_width;
	vram = new RGBA_raw[ram_width * ram_height];

	fprintf(stderr, "RgbaTex: texture %d, %dx%d creted.\n",
		tex,
		w, h);
}





void toggle(bool& ref)
{
	auto log = [&](bool state, int key, const char* text){
		printf("%12s (%c): %s\n",
			text,
			key,
			state ? "YES" : "NO");
	};

	ref = !ref;
	puts("-----------------------------");
	log(mode.sync_enabled,     'k', "sync");
	log(mode.mixed_enabled,    'm', "mixed");
	log(mode.mirroring,        '?', "mirroring");
	log(mode.borderline,       'b', "borderline");
	log(mode.view4test,        '$', "view4test");
	puts("-----------------------------");
}

#define DEFAULT_DISPLAY_MODE	DISPLAY_MODE_DEPTH


StClient* StClient::ms_self = nullptr;

typedef std::vector<openni::RGB888Pixel> RgbScreen;
typedef std::map<int,RgbScreen> RgbScreenMovie;

local size_t movie_index = 0;
local openni::RGB888Pixel* moviex = nullptr;
local freetype::font_data monospace;



// @constructor, @init
StClient::StClient(Kdev& dev1_, Kdev& dev2_) :
	dev1(dev1_),
	dev2(dev2_),
	video_ram(nullptr),
	video_ram2(nullptr)
{
	ms_self = this;

	view_3d_left();


	// コンフィグデータからのロード
	cal_cam1.curr = config.cam1;
	cal_cam2.curr = config.cam2;



	udp_recv.init(UDP_CLIENT_RECV);
	printf("host: %s\n", Core::getComputerName().c_str());
	printf("ip: %s\n", mi::Udp::getIpAddress().c_str());

	mode.mirroring   = config.mirroring;

	depth_screen.resize(512);

	{
		HitObject ho;
		ho.point = Point(3,2);
		ho.color = glRGBA(20, 100, 250);
		hit_objects.push_back(ho);
	}

	{
		HitObject ho;
		ho.point = Point(529,380);
		ho.color = glRGBA(250, 50, 50);
		hit_objects.push_back(ho);
	}

	{
		HitObject ho;
		ho.point = Point(622,50);
		ho.color = glRGBA(255, 200, 120);
		hit_objects.push_back(ho);
	}
}

StClient::~StClient()
{
	delete[] video_ram;
	delete[] video_ram2;

	ms_self = nullptr;
}



bool StClient::init(int argc, char **argv)
{
	if (global_config.enable_kinect)
	{
		openni::VideoMode depthVideoMode;
		openni::VideoMode colorVideoMode;

		if (dev1.depth.isValid() && dev1.color.isValid())
		{
			depthVideoMode = dev1.depth.getVideoMode();
			colorVideoMode = dev1.color.getVideoMode();

			const int depthWidth  = depthVideoMode.getResolutionX();
			const int depthHeight = depthVideoMode.getResolutionY();
			const int colorWidth  = colorVideoMode.getResolutionX();
			const int colorHeight = colorVideoMode.getResolutionY();
			fprintf(stderr,
				"Kinect resolution: depth(%dx%d), color(%dx%d)\n",
				depthWidth, depthHeight,
				colorWidth, colorHeight);

			if (depthWidth==colorWidth && depthHeight==colorHeight)
			{
				m_width = depthWidth;
				m_height = depthHeight;
			}
			else
			{
				printf("Error - expect color and depth to be in same resolution: D: %dx%d, C: %dx%d\n",
					depthWidth, depthHeight,
					colorWidth, colorHeight);
				return false;
			}
		}
		else if (dev1.depth.isValid())
		{
			depthVideoMode = dev1.depth.getVideoMode();
			m_width  = depthVideoMode.getResolutionX();
			m_height = depthVideoMode.getResolutionY();
		}
		else if (dev1.color.isValid())
		{
			colorVideoMode = dev1.color.getVideoMode();
			m_width  = colorVideoMode.getResolutionX();
			m_height = colorVideoMode.getResolutionY();
		}
		else
		{
			printf("Error - expects at least one of the streams to be valid...\n");
			return openni::STATUS_ERROR;
		}
	}
	else
	{
		m_width = 0;
		m_height = 0;
	}

	// Texture map init
	printf("%d x %d\n", m_width, m_height);
	m_nTexMapX = MIN_CHUNKS_SIZE(m_width, TEXTURE_SIZE);
	m_nTexMapY = MIN_CHUNKS_SIZE(m_height, TEXTURE_SIZE);

	if (!initOpenGL(argc, argv))
	{
		return false;
	}

	dev1.initRam();
	dev2.initRam();


	glGenTextures(1, &vram_tex2);


	video_ram     = new RGBA_raw[m_nTexMapX * m_nTexMapY];
	video_ram2    = new RGBA_raw[m_nTexMapX * m_nTexMapY];


	// Init routine @init
	{
		puts("Init font...");
		const std::string font_folder = "C:/Windows/Fonts/";
		monospace.init(font_folder + "Cour.ttf", 12);

		// Consolas
		//monospace.init(font_folder + "trebuc.ttf", 10);
		puts("Init font...done!");
	}

	// @init @image @png @jpg
//	background_image.createFromImageA("C:/ST/Picture/Pretty-Blue-Heart-Design.jpg");
	global.background_image.createFromImageA("C:/ST/Picture/mountain-04.jpg");
	global.dot_image.createFromImageA("C:/ST/Picture/dot.png");

	return true;
}

bool StClient::run()	//Does not return
{
#if USE_GLFW
	for (;;)
	{
		if (!glfwGetWindowParam(GLFW_OPENED))
		{
			break;
		}

		display();


		glfwSwapBuffers();
	}
	glfwTerminate();
#else
	glutMainLoop();
#endif
	return true;
}

void buildBitmap(
		int tex,
		RGBA_raw* bitmap,
		int bw, int bh)
{
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bw, bh, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap);
}

void drawBitmap(
		int dx, int dy, int dw, int dh,
		float u1, float v1, float u2, float v2)
{
	const int x1 = dx;
	const int y1 = dy;
	const int x2 = dx + dw;
	const int y2 = dy + dh;

	gl::Texture(true);
	glColor4f(1,1,1,1);
	glBegin(GL_QUADS);
	glTexCoord2f(u1, v1); glVertex2f(x1, y1);
	glTexCoord2f(u2, v1); glVertex2f(x2, y1);
	glTexCoord2f(u2, v2); glVertex2f(x2, y2);
	glTexCoord2f(u1, v2); glVertex2f(x1, y2);
	glEnd();
}


local int decomp_time = 0;
local int draw_time = 0;
local uint8 floor_depth[640*480];
local uint8 depth_cook[640*480];
local int floor_depth_count = 0;
local uint16 floor_depth2[640*480];




void DrawRgbaTex_Build(const RgbaTex& img)
{
	gl::Texture(true);
	glBindTexture(GL_TEXTURE_2D, img.tex);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		img.ram_width, img.ram_height,
		0, GL_RGBA, GL_UNSIGNED_BYTE, img.vram);
}

void DrawRgbaTex_Draw(const RgbaTex& img, int dx, int dy, int dw, int dh)
{
	const int x1 = dx;
	const int y1 = dy;
	const int x2 = dx + dw;
	const int y2 = dy + dh;
	const float u1 = 0.0f;
	const float v1 = 0.0f;
	const float u2 = 1.0f * img.width  / img.ram_width;
	const float v2 = 1.0f * img.height / img.ram_height;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, img.tex);
	glColor4f(1,1,1,1);
	glBegin(GL_QUADS);
	glTexCoord2f(u1, v1); glVertex2f(x1, y1);
	glTexCoord2f(u2, v1); glVertex2f(x2, y1);
	glTexCoord2f(u2, v2); glVertex2f(x2, y2);
	glTexCoord2f(u1, v2); glVertex2f(x1, y2);
	glEnd();
}

void DrawRgbaTex(const RgbaTex& img, int dx, int dy, int dw, int dh)
{
	DrawRgbaTex_Build(img);
	DrawRgbaTex_Draw(img, dx,dy,dw,dh);
}


static MovieData curr_movie;


void StClient::drawPlaybackMovie()
{
	int xx = timeGetTime();
	static zlibpp::bytes outdata;
	if (outdata.size()==0)
	{
		outdata.resize(640*480);
		puts("resize outdata");
	}
	zlibpp::bytes& byte_stream = curr_movie.frames[movie_index++];
	zlibpp::decompress(byte_stream, outdata);
	decomp_time += timeGetTime()-xx;

	int yy = timeGetTime();
	const auto* src = outdata.data();
	for (int y=0; y<480; ++y)
	{
		RGBA_raw* dest = video_ram2 + y*m_nTexMapX;
		for (int x=0; x<640; ++x, ++dest, ++src)
		{
			const int d = *src;
			switch (d)
			{
			case 0:
				dest->a = 0;
				break;
			case 255:
				dest->r = 200;
				dest->g = 140;
				dest->b = 100;
				dest->a = 199;
				break;
			default:
				dest->r = d;
				dest->g = d;
				dest->b = 0;
				dest->a = 199;
				break;
			}
		}
	}
	draw_time += timeGetTime()-yy;

	// @draw
	buildBitmap(
		vram_tex2,
		video_ram2,
		m_nTexMapX, m_nTexMapY);
	drawBitmap(
		0, 0, 640, 480,
		0.0f,
		0.0f,
		(float)m_width  / m_nTexMapX,
		(float)m_height / m_nTexMapY);
}


#if 0
//#
void StClient::drawDepthModh()
{
	using namespace openni;

	const int WORK = 4;
	static uint8
		curr_pre[640*480],
		mixed[640*480],
		work[WORK][640*480];
	static int
		work_total[640*480];
	static int
		work_index = 0;

	uint8* const curr = work[work_index];
	work_index = (work_index+1) % WORK;

	{
		const uint8* src = curr_pre;
		uint8* dest = curr;
		for (int y=0; y<480; ++y)
		{
			for (int x=0; x<640; ++x)
			{
				uint8 depth = *src++;

				if (mode.borderline && depth>=10 && depth<=240)
				{
					if (depth%2==0)
						depth = 20;
					else
						depth = 240;
				}

				*dest++ = depth;
			}
		}
	}


	// Curr+Back => Mixed
	if (mode.mixed_enabled)
	{
		for (int i=0; i<640*480; ++i)
		{
			work_total[i] = 0;
		}
		for (int j=0; j<WORK; ++j)
		{
			for (int i=0; i<640*480; ++i)
			{
				if (work[j][i]!=255)
				{
					work_total[i] += work[j][i];
				}
			}
		}

		// Total => Mixed
		for (int i=0; i<640*480; ++i)
		{
			mixed[i] = work_total[i] / WORK;
		}
	}

	switch (movie_mode)
	{
	case MOVIE_RECORD:
		if (curr_movie.recorded_tail >= curr_movie.frames.size())
		{
			puts("time over! record stop.");
			movie_mode = MOVIE_READY;
		}
		else
		{
			zlibpp::bytes& byte_stream = curr_movie.frames[curr_movie.recorded_tail++];
			zlibpp::compress(curr, 640*480, byte_stream, 2);
			printf("frame %d, %d bytes (%.1f%%)\n",
				curr_movie.recorded_tail,
				byte_stream.size(),
				byte_stream.size() * 100.0 / (640*480));
		}
		break;
	case MOVIE_PLAYBACK:
		if (movie_index >= curr_movie.recorded_tail)
		{
			puts("movie is end. stop.");
			movie_mode = MOVIE_READY;
		}
		else
		{
			movie_mode = MOVIE_PLAYBACK;
			movie_index = 0;
		}
		break;
	}


	// Mixed to Texture
	{
		auto* src = mode.mixed_enabled ? mixed : curr;

		for (int y=0; y<480; ++y)
		{
			RGBA_raw* dest = video_ram + y*m_nTexMapX;

			for (int x=0; x<640; ++x, ++dest, ++src)
			{
				const int value = *src;
				switch (value)
				{
				case 0:
					dest->set(30,50,70,200);
					break;
				case 1:
					dest->set(0, 40, 80, 100);
					break;
				case 255:
					dest->set(80, 50, 20, 200);
					break;
				default:
					if (mode.alpha_mode)
					{
						dest->set(100, value, 255-value, 255);
					}
					else
					{
						dest->set(240, 220, 140, (value));
					}
					break;
				}
			}
		}
	}

	// @build
	buildBitmap(dev1.vram_tex, video_ram, m_nTexMapX, m_nTexMapY);

	// @draw
	const int draw_x = 0;
	const int draw_y = 0;
	const int draw_w = 640/2;
	const int draw_h = 480/2;
	drawBitmap(
		draw_x, draw_y,
		draw_w, draw_h,
		0.0f,
		0.0f,
		(float)m_width  / m_nTexMapX,
		(float)m_height / m_nTexMapY);
}
#endif


void StClient::displayDepthScreen()
{
}

void StClient::displayBlackScreen()
{
}

void StClient::displayPictureScreen()
{
	if (global.pic.enabled())
	{
		global.pic.draw(0,0, 640,480, 255);
	}
}




struct ChangeCalParamKeys
{
	bool rot_x, rot_y, rot_z, scale, ctrl, up, left, right, down;
};

ChangeCalParamKeys getChangeCalParamKeys()
{
	BYTE kbd[256];
	GetKeyboardState(kbd);

	ChangeCalParamKeys keys;
	keys.ctrl  = (kbd[VK_CONTROL] & 0x80)!=0;
	keys.rot_x = (kbd['T'] & 0x80)!=0;
	keys.rot_y = (kbd['Y'] & 0x80)!=0;
	keys.rot_z = (kbd['U'] & 0x80)!=0;
	keys.scale = (kbd['I'] & 0x80)!=0;
	return keys;
}


void display2()
{
	static uint time_begin;
	if (time_begin==0)
	{
		time_begin = timeGetTime();
	}

	static float cnt = 0;
	static int frames = 0;
	++frames;
	cnt += 0.01;
	glPushMatrix();
	glLoadIdentity();
	int time_diff = (timeGetTime() - time_begin);

	glRGBA::white.glColorUpdate();

	const int H=15;
	int y = 10;

	auto nl = [&](){ y+=H/2; };
	auto pr = freetype::print;

	glRGBA heading(80,255,120);
	glRGBA text(200,200,200);

	auto color = [](bool status){
		status
			? glRGBA(240,200,50).glColorUpdate()
			: glRGBA(200,200,200).glColorUpdate();
	};


	
	{
		auto keys = getChangeCalParamKeys();
		heading.glColorUpdate();
		pr(monospace, 320, y,
			(keys.rot_x) ? "<X-rotation (vertical)>" :
			(keys.rot_y) ? "<Y-rotation (horizontal)>" :
			(keys.rot_z) ? "<Z-rotation (rotation)>" :
			(keys.scale) ? "<Scaling>" : "");
	}


	heading.glColorUpdate();
	pr(monospace, 20, y+=H, "View Mode");
	text.glColorUpdate();
	{
		const auto vm = global.view_mode;
		color(vm==VM_2D_LEFT);  pr(monospace, 20, y+=H, "[F1] 2D left");
		color(vm==VM_2D_TOP);   pr(monospace, 20, y+=H, "[F2] 2D top");
		color(vm==VM_2D_FRONT); pr(monospace, 20, y+=H, "[F3] 2D front");
		color(false);           pr(monospace, 20, y+=H, "[F4] ----");
		color(vm==VM_2D_RUN);   pr(monospace, 20, y+=H, "[F5] 2D run");
		color(vm==VM_3D_LEFT);  pr(monospace, 20, y+=H, "[F6] 3D left");
		color(vm==VM_3D_RIGHT); pr(monospace, 20, y+=H, "[F7] 3D right");
		color(vm==VM_3D_FRONT); pr(monospace, 20, y+=H, "[F8] 3D front");
	}
	nl();

	heading.glColorUpdate();
	pr(monospace, 20, y+=H, "EYE");
	text.glColorUpdate();
	pr(monospace, 20, y+=H, "x =%+9.4f", eye.x);
	pr(monospace, 20, y+=H, "y =%+9.4f [q/e]", eye.y);
	pr(monospace, 20, y+=H, "z =%+9.4f", eye.z);
	pr(monospace, 20, y+=H, "rh=%+9.4f(rad)", eye.rh);
	pr(monospace, 20, y+=H, "v =%+9.4f", eye.v);
	nl();

	heading.glColorUpdate();
	pr(monospace, 20, y+=H, "Camera A:");
	text.glColorUpdate();
	pr(monospace, 20, y+=H, "pos x = %9.5f", cal_cam1.curr.x);
	pr(monospace, 20, y+=H, "pos y = %9.5f", cal_cam1.curr.y);
	pr(monospace, 20, y+=H, "pos z = %9.5f", cal_cam1.curr.z);
	pr(monospace, 20, y+=H, "rot x = %9.5f", cal_cam1.curr.rotx);
	pr(monospace, 20, y+=H, "rot y = %9.5f", cal_cam1.curr.roty);
	pr(monospace, 20, y+=H, "rot z = %9.5f", cal_cam1.curr.rotz);
	pr(monospace, 20, y+=H, "scale = %9.5f", cal_cam1.curr.scale);
	nl();

	heading.glColorUpdate();
	pr(monospace, 20, y+=H, "Camera B:");
	text.glColorUpdate();
	pr(monospace, 20, y+=H, "pos x = %9.5f", cal_cam2.curr.x);
	pr(monospace, 20, y+=H, "pos y = %9.5f", cal_cam2.curr.y);
	pr(monospace, 20, y+=H, "pos z = %9.5f", cal_cam2.curr.z);
	pr(monospace, 20, y+=H, "rot x = %9.5f", cal_cam2.curr.rotx);
	pr(monospace, 20, y+=H, "rot y = %9.5f", cal_cam2.curr.roty);
	pr(monospace, 20, y+=H, "rot z = %9.5f", cal_cam2.curr.rotz);
	pr(monospace, 20, y+=H, "scale = %9.5f", cal_cam2.curr.scale);
	nl();

	pr(monospace, 20, y+=H,
		"#%d Near(%dmm) Far(%dmm) [%s][%s]",
			config.client_number,
			config.near_threshold,
			config.far_threshold,
			mode.borderline ? "border" : "no border",
			mode.auto_clipping ? "auto clipping" : "no auto clip");

	// @fps
	pr(monospace, 20, y+=H, "%d, %d, %.1ffps, %.2ffps, %d, %d",
			frames,
			time_diff,
			1000.0f * frames/time_diff,
			fps_counter.getFps(),
			decomp_time,
			draw_time);

#if !WITHOUT_KINECT
	freetype::print(monospace, 20, 200, "(n=%d,f=%d) raw(n=%d,f=%d) (TP:%d)",
			vody.near_d,
			vody.far_d,
			vody.raw_near_d,
			vody.raw_far_d,
			vody.total_pixels);

	freetype::print(monospace, 20, 220, "(%d,%d,%d,%d)",
			vody.body.top,
			vody.body.bottom,
			vody.body.left,
			vody.body.right);

	freetype::print(monospace, 20, 240, "far(%d,%d,%d,%d)",
			vody.far_box.top,
			vody.far_box.bottom,
			vody.far_box.left,
			vody.far_box.right);

	freetype::print(monospace, 20, 260, "near(%d,%d,%d,%d)",
			vody.near_box.top,
			vody.near_box.bottom,
			vody.near_box.left,
			vody.near_box.right);
#endif//!WITHOUT_KINECT

	glPopMatrix();
}


void drawFieldGrid(int size_cm)
{
	glBegin(GL_LINES);
	const float F = size_cm/100.0f;

	glLineWidth(1.0f);
	for (int i=-size_cm/2; i<size_cm/2; i+=50)
	{
		const float f = i/100.0f;

		//
		if (i==0)
		{
			// centre line
			glRGBA(0.25f, 0.66f, 1.00f, 1.00f).glColorUpdate();
		}
		else if (i==100)
		{
			// centre line
			glRGBA(1.00f, 0.33f, 0.33f, 1.00f).glColorUpdate();
		}
		else
		{
			glRGBA(
				global_config.grid_r,
				global_config.grid_g,
				global_config.grid_b,
				0.40f).glColorUpdate();
		}

		glVertex3f(-F, 0, f);
		glVertex3f(+F, 0, f);

		glVertex3f( f, 0, -F);
		glVertex3f( f, 0, +F);
	}

	glEnd();

	const float BOX_WIDTH  = 4.0f;
	const float BOX_HEIGHT = 3.0f;
	const float BOX_DEPTH  = 3.0f;
	const float LEFT  = -(BOX_WIDTH/2);
	const float RIGHT = +(BOX_WIDTH/2);

	// Left and right box
	for (int i=0; i<2; ++i)
	{
		const float x = (i==0) ? LEFT : RIGHT;
		glBegin(GL_LINE_LOOP);
			glVertex3f(x,  BOX_HEIGHT,  0);
			glVertex3f(x,  BOX_HEIGHT, -BOX_DEPTH);
			glVertex3f(x,           0, -BOX_DEPTH);
			glVertex3f(x,           0,  0);
		glEnd();
	}

	// Ceil bar
	glBegin(GL_LINES);
		glVertex3f(LEFT,  BOX_HEIGHT, 0);
		glVertex3f(RIGHT, BOX_HEIGHT, 0);
		glVertex3f(LEFT,  BOX_HEIGHT, -BOX_DEPTH);
		glVertex3f(RIGHT, BOX_HEIGHT, -BOX_DEPTH);
	glEnd();



	// run space, @green
	glRGBA(0.25f, 1.00f, 0.25f, 0.25f).glColorUpdate();
	glBegin(GL_QUADS);
	glVertex3f(-2, 0,  0);
	glVertex3f(-2, 0, -2);
	glVertex3f( 2, 0, -2);
	glVertex3f( 2, 0,  0);
	glEnd();
}


void drawWall()
{
	auto& img = global.background_image;

	// @wall
	const float Z = global_config.wall_depth;
	gl::Texture(true);
	glPushMatrix();
	gl::LoadIdentity();
	glBindTexture(GL_TEXTURE_2D, img.getTexture());
	const float u = img.getTextureWidth();
	const float v = img.getTextureHeight();
	glBegin(GL_QUADS);
	const float SZ = Z/2;
	for (int i=-5; i<=5; ++i)
	{
		glTexCoord2f(0,0); glVertex3f(-SZ+Z*i, Z*1.5, Z);
		glTexCoord2f(u,0); glVertex3f( SZ+Z*i, Z*1.5, Z); //左上
		glTexCoord2f(u,v); glVertex3f( SZ+Z*i,  0.0f, Z); //左下
		glTexCoord2f(0,v); glVertex3f(-SZ+Z*i,  0.0f, Z);
	}
	glEnd();
	glPopMatrix();
	gl::Texture(false);
}


void drawBody___(RawDepthImage& raw, int red, int green, int blue)
{
#define USE_DOT_TEX 1

	// @body @dot
	const uint16* data = raw.image.data();
	raw.CalcDepthMinMax();
	if (!mode.simple_dot_body)
	{
		glBegin(GL_QUADS);
		gl::Texture(true);
		glBindTexture(GL_TEXTURE_2D, global.dot_image);
	}
	else
	{
		glBegin(GL_POINTS);
	}

	const float ax = 0.01;
	const float az = 0.01;

	const int RANGE = config.far_threshold - config.near_threshold;
	for (int y=0; y<480; ++y)
	{
		for (int x=0; x<640; ++x)
		{
			const int val = *data++;

			if (val < config.near_threshold)
			{
				continue;
			}
			
			if (val > config.far_threshold)
			{
				continue;
			}
			
			// - dev1.raw_depth.min_value
			int alpha = (val)*255 / RANGE;
			if (alpha<0) continue;
			if (alpha>255) alpha=255;

#if 0
			if ((y + alpha/10)%20<=10)
			{
				glRGBA(
					250 * alpha >> 8,
					220 * alpha >> 8,
					 50 * alpha >> 8,
					255-alpha/2).glColorUpdate();
			}
			else
#endif
			{
				glRGBA(
					red   * alpha >> 8,
					green * alpha >> 8,
					blue  * alpha >> 8,
					255-alpha/2).glColorUpdate();
			}

			// Aspect ratio 1:1
			const float z = val/2000.0f;
			const float dx = x/640.0f - 0.5;
			const float dy = (480-y)/640.0f;
			const float dz =  z;

			if (!mode.simple_dot_body)
			{
				const float F = 0.0025;

				glVertex3f(dx-ax, dy  , dz-az);
				glVertex3f(dx+ax, dy-F, dz-az);
				glVertex3f(dx+ax, dy,   dz+az);
				glVertex3f(dx-ax, dy-F, dz+az);
			}
			else
			{
				glVertex3f(dx,dy,dz);
			}
		}
	}

	glEnd();
}


struct Point3D
{
	float x,y,z;
};

typedef std::vector<Point3D> Dots;
Dots dot_set;



void DS_Init(Dots& dots)
{
	dots.clear();
}






void MixDepth(Dots& dots, const RawDepthImage& src, const CamParam& calparam)
{
	mat4x4 trans;
	{
		// X軸回転
		float cos = cosf(calparam.rotx);
		float sin = sinf(calparam.rotx);
		trans = mat4x4(
			1,   0,    0, 0,
			0, cos, -sin, 0,
			0, sin,  cos, 0,
			0,   0,    0, 1) * trans;
	}
	{
		// Y軸回転
		float cos = cosf(calparam.roty);
		float sin = sinf(calparam.roty);
		trans = mat4x4(
			 cos, 0, sin, 0,
			   0, 1,   0, 0,
			-sin, 0, cos, 0,
			   0, 0,   0, 1) * trans;
	}
	{
		// Z軸回転
		float cos = cosf(calparam.rotz);
		float sin = sinf(calparam.rotz);
		trans = mat4x4(
			cos,-sin, 0, 0,
			sin, cos, 0, 0,
			  0,   0, 1, 0,
			  0,   0, 0, 1) * trans;
	}

	// 平行移動
	const float s = calparam.scale;
	trans = mat4x4(
		s, 0, 0, calparam.x,
		0, s, 0, calparam.y,
		0, 0, s, calparam.z,
		0, 0, 0, 1) * trans;

	int index = 0;
	for (int y=0; y<480; ++y)
	{
		for (int x=0; x<640; ++x)
		{
			int z = src.image[index++];

			// ignore
			if (z==0) continue;

			Point3D p;
			float fx = (320-x)/640.0f;
			float fy = (240-y)/640.0f;
			float fz = z/1000.0f; // milli-meter(mm) to meter(m)

			// -0.5 <= fx <= 0.5
			// -0.5 <= fy <= 0.5
			//  0.0 <= fz <= 10.0  (10m)

			fx = fx * fz;
			fy = fy * fz;

			vec4 point = trans * vec4(fx, fy, fz, 1.0f);

			// 平行移動
			p.x = point[0];
			p.y = point[1];
			p.z = point[2];
			dots.push_back(p);
		}
	}
}

void drawBoxels(const Dots& dots, glRGBA inner_color, glRGBA outer_color, bool half=false)
{
	// @boxel @dot
	glBegin(GL_POINTS);

	for (size_t i=0; i<dots.size(); ++i)
	{
		if ((i%2==0))
		{
			continue;
		}

		const float x = dots[i].x;
		const float y = dots[i].y;
		const float z = dots[i].z;

		const bool in_x = (x>=-2.0f && x<=+2.0f);
		const bool in_y = (y>=+0.0f && y<=+4.0f);
		const bool in_z = (z>=+0.0f);
		
		float col = z/4;
		if (col<0.25f) col=0.25f;
		if (col>0.90f) col=0.90f;
		col = 1.00f - col;
		const int col255 = (int)(col*255);

		if (in_x && in_y && in_z)
		{
			glRGBA(
				inner_color.r,
				inner_color.g,
				inner_color.b,
				col255).glColorUpdate();
		}
		else
		{
			glRGBA(
				outer_color.r,
				outer_color.g,
				outer_color.b,
				col255>>3).glColorUpdate();
		}

		glVertex3f(x,y,-z);
	}

	glEnd();
}



size_t hit_object_stage = 0;
void StClient::display()
{
	while (doCommand())
	{
	}

	// @fps
	fps_counter.update();

	// @display
	glClearColor(
		global_config.ground_r,
		global_config.ground_g,
		global_config.ground_b,
		1.00);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Kinectから情報をもらう
	if (dev1.device.isValid())
	{
		{
			mi::Timer tm(&time_profile.read1_depth_dev1);
			dev1.CreateRawDepthImage_Read();
		}
		{
			mi::Timer tm(&time_profile.read2_depth_dev1);
			dev1.CreateRawDepthImage();
		}
	}
	else
	{
		// ダミーの情報 @random @noise
		static int no = 0;
		for (int i=0; i<640*480; ++i)
		{
			int v = 0;
			if (((i*2930553>>3)^((i*39920>>4)+no))%3==0)
			{
				v = (i+no*3)%6500 + (i+no)%2000 + 500;
			}
			dev1.raw_depth.image[i] = v;
		}
		no += 6;
	}

	if (dev2.device.isValid())
	{
		{
			mi::Timer tm(&time_profile.read1_depth_dev2);
			dev2.CreateRawDepthImage_Read();
		}
		{
			mi::Timer tm(&time_profile.read2_depth_dev2);
			dev2.CreateRawDepthImage();
		}
	}


	//============
	// 3D Section
	//============
	::glMatrixMode(GL_PROJECTION);
	::glLoadIdentity();


	if (global.view.is_ortho)
	{
		// 2D視点です!!
		const double w = global.view.ortho.width;
		glOrtho(-w/2, +w/2, 0, (w*3/4), -3.0, +120.0);
	}
	else
	{
		// 3D視点です!!
		gluPerspective(30.0f, 4.0f/3.0f, 1.0f, 100.0f);
	}


	eye.gluLookAt();

	gl::Texture(false);
	gl::DepthTest(true);
	::glMatrixMode(GL_MODELVIEW);
	::glLoadIdentity();

	{mi::Timer tm(&time_profile.draw_wall);
		//drawWall();
	}
	{mi::Timer tm(&time_profile.draw_grid);
		drawFieldGrid(500);
	}

	glRGBA color_cam1(80,190,250);
	glRGBA color_cam2(250,190,80);
	glRGBA color_both(255,255,255);
	glRGBA color_other(120,120,120);
	glRGBA color_outer(80,80,144);

	if (camera_mode==CAM_BOTH)
	{
		{
			DS_Init(dot_set);
			MixDepth(dot_set, dev1.raw_depth, cal_cam1.curr);
			MixDepth(dot_set, dev2.raw_depth, cal_cam2.curr);
			drawBoxels(dot_set, color_both, color_outer);
		}
	}
	else
	{
		if (camera_mode==CAM_A)
		{
			DS_Init(dot_set);
			MixDepth(dot_set, dev1.raw_depth, cal_cam1.curr);
			drawBoxels(dot_set, color_cam1, color_outer);
			DS_Init(dot_set);
			MixDepth(dot_set, dev2.raw_depth, cal_cam2.curr);
			drawBoxels(dot_set, color_other, color_other);
		}
		else
		{
			DS_Init(dot_set);
			MixDepth(dot_set, dev1.raw_depth, cal_cam1.curr);
			drawBoxels(dot_set, color_other, color_other);
			DS_Init(dot_set);
			MixDepth(dot_set, dev2.raw_depth, cal_cam2.curr);
			drawBoxels(dot_set, color_cam2, color_outer);
		}
	}


	memset(hitdata.hit, 0, sizeof(hitdata.hit));
	for (size_t i=0; i<dot_set.size(); ++i)
	{
		// デプスはGreenのなかだけ
		Point3D p = dot_set[i];
		if (!(p.z>=0.0f && p.z<=2.0f))
		{
			// ignore: too far, too near
			continue;
		}

		// (x,z)を、大きさ1の正方形におさめる（はみ出すこともある）
		float fx = ((p.x+2.0)/4.0);
		float fy = ((2.4-p.y)/2.4);

		const int x = (int)(fx * 80);
		const int y = (int)(fy * 60);

		if ((uint)x<80 && (uint)y<60)
		{
			hitdata.hit[x + y*80] = true;
		}
	}




	switch (movie_mode)
	{
	case MOVIE_READY:
		break;
	case MOVIE_RECORD:
		if (curr_movie.recorded_tail >= curr_movie.frames.size())
		{
			puts("time over! record stop.");
			movie_mode = MOVIE_READY;
		}
		else
		{
#if 0
			float* float_mem = new float[3*dot_set.size()];
			int index = 0;
			int boxel_count = 0;
			for (size_t i=0; i<dot_set.size(); ++i)
			{
				const auto& d = dot_set[i];
				if (d.x>=-2.5f && d.x<=+2.5f && d.y>=-0.5f && d.y<=3.0f && d.z>=0.0f && d.z<=4.0f)
				{
					float_mem[index+0] = d.x;
					float_mem[index+1] = d.y;
					float_mem[index+2] = d.z;
					index += 3;
					++boxel_count;
				}
			}

			zlibpp::bytes& byte_stream = curr_movie.frames[curr_movie.recorded_tail++];
			zlibpp::compress(
				(byte*)float_mem,
				sizeof(float)*3*dot_set.size(),
				byte_stream, 0);
			printf("frame %d, %d boxels, %d bytes (%.1f%%)\n",
				curr_movie.recorded_tail,
				boxel_count,
				byte_stream.size(),
				byte_stream.size() * 100.0 / (640*480*3*sizeof(float)));
			delete[] float_mem;
#endif
			static uint16* int_mem = new uint16[2*3*640*480];
			int index = 0;
			int boxel_count = 0;
			for (size_t i=0; i<dot_set.size(); ++i)
			{
				const auto& d = dot_set[i];
				if (d.x>=-2.5f && d.x<=+2.5f && d.y>=-0.5f && d.y<=3.0f && d.z>=0.0f && d.z<=4.0f)
				{
					int_mem[index+0] = (uint16)((d.x+2.5f)*65535/5.0f);
					int_mem[index+1] = (uint16)((d.y     )*65535/3.0f);
					int_mem[index+2] = (uint16)((d.z     )*65535/4.0f);
					index += 3;
					++boxel_count;
				}
			}

			zlibpp::bytes& byte_stream = curr_movie.frames[curr_movie.recorded_tail++];
			zlibpp::compress(
				(byte*)int_mem,
				sizeof(uint16)*3*boxel_count,
				byte_stream, 0);
			printf("frame %d, %d boxels, %d bytes (%.1f%%)\n",
				curr_movie.recorded_tail,
				boxel_count,
				byte_stream.size(),
				byte_stream.size() * 100.0 / (640*480*3*sizeof(uint16)));
//			delete[] int_mem;
		}
		break;
	case MOVIE_PLAYBACK:
		if (curr_movie.recorded_tail==0)
		{
			puts("Movie empty.");
		}
		else
		{
			zlibpp::bytes& byte_stream = curr_movie.frames[movie_index];
			if (++movie_index >= curr_movie.recorded_tail)
			{
				movie_mode = MOVIE_READY;
				puts("movie end.");
			}

#if 0
			zlibpp::bytes outdata;
			zlibpp::decompress(
				byte_stream,
				outdata);
			size_t boxel_count = outdata.size()/sizeof(float)/3;
			const float* float_mem = (const float*)outdata.data();
			
			Dots dots;
			dots.resize(boxel_count);
			for (size_t i=0; i<boxel_count; ++i)
			{
				dots[i].x = *float_mem++;
				dots[i].y = *float_mem++;
				dots[i].z = *float_mem++;
			}

			drawBoxels(dots, glRGBA(250,100,60), glRGBA(200,70,30));
#else
			zlibpp::bytes outdata;
			zlibpp::decompress(
				byte_stream,
				outdata);
			size_t boxel_count = outdata.size()/sizeof(uint16)/3;
			const uint16* int_mem = (const uint16*)outdata.data();
			
			Dots dots;
			dots.resize(boxel_count);
			for (size_t i=0; i<boxel_count; ++i)
			{
				const uint16 x = *int_mem++;
				const uint16 y = *int_mem++;
				const uint16 z = *int_mem++;

				dots[i].x = x/65535.0f * 5.0f - 2.5f;
				dots[i].y = y/65535.0f * 3.0f;
				dots[i].z = z/65535.0f * 4.0f;
			}

			drawBoxels(dots, glRGBA(250,100,60), glRGBA(200,70,30), true);
#endif
		}
		break;
	}


	//============
	// 2D Section
	//============
	gl::Projection();
	gl::LoadIdentity();
	glOrtho(0, 640, 480, 0, -1.0, 1.0);

	gl::Texture(false);
	gl::DepthTest(false);

	{
		glRGBA body(240,220,60, 180);
		glRGBA empty(50,60,80, 60);
		
		glBegin(GL_QUADS);
		for (int y=0; y<60; ++y)
		{
			for (int x=0; x<80; ++x)
			{
				hitdata.hit[x+y*80]
					? body.glColorUpdate()
					: empty.glColorUpdate();
				const int S = 2;
				const int M = 10;
				const int dx = x*S + 640 - M - 80*S;
				const int dy = y*S +   0 + M;
				glVertex2i(dx,   dy);
				glVertex2i(dx+S, dy);
				glVertex2i(dx+S, dy+S);
				glVertex2i(dx,   dy+S);
			}
		}
		glEnd();
	}


	switch (global.client_status)
	{
	case STATUS_BLACK:        displayBlackScreen();   break;
	case STATUS_PICTURE:      displayPictureScreen(); break;
	case STATUS_DEPTH:
		displayDepthScreen();
		break;
	
	case STATUS_GAMEREADY:
	case STATUS_GAME:
		displayDepthScreen();
		break;
	}

	glRGBA::white.glColorUpdate();

	display2();

#if 0
	glRGBA(255,255,255,100).glColorUpdate();
	gl::Line2D(Point2i(0,240), Point2i(640,240));
	gl::Line2D(Point2i(320,0), Point2i(320,480));

	if (!mode.view4test)
	{
		gl::Line2D(Point2i(0, 40), Point2i(640, 40));
		gl::Line2D(Point2i(0,440), Point2i(640,440));
	}

	if (mode.calibration)
	{
		displayCalibrationInfo();
	}

	glBegin(GL_POINTS);
	glRGBA(255,255,255).glColorUpdate();
		glVertex2d(old_x, old_y);
	glEnd();
#endif

#if !USE_GLFW
	glutSwapBuffers();
#endif
}


enum
{
	KEY_F1 = 1001,
	KEY_F2 = 1002,
	KEY_F3 = 1003,
	KEY_F4 = 1004,
	KEY_F5 = 1005,
	KEY_F6 = 1006,
	KEY_F7 = 1007,
	KEY_F8 = 1008,
	KEY_F9 = 1009,
	KEY_F10 = 1010,
	KEY_F11 = 1011,
	KEY_F12 = 1012,
	KEY_LEFT = 1100,
	KEY_UP = 1101,
	KEY_RIGHT = 1102,
	KEY_DOWN = 1103,
	KEY_PAGEUP = 1104,
	KEY_PAGEDOWN = 1105,
	KEY_HOME = 1106,
	KEY_END = 1107,
};

void saveAgent(int slot)
{
	printf("Save to file slot %d...", slot);

	char buf[100];
	sprintf_s(buf, "file%d", slot);
	FILE* fp = fopen(buf, "wb");
	if (fp==nullptr)
	{

		puts("invalid!!");
	}
	else
	{
		saveToFile(fp, curr_movie);
		fclose(fp);
		puts("done!");
	}	
}

void loadAgent(int slot)
{
	printf("Load from file slot %d...", slot);
	char buf[100];
	sprintf_s(buf, "file%d", slot);
	FILE* fp = fopen(buf, "rb");
	if (fp==nullptr)
	{
		puts("file not found!");
	}
	else
	{
		if (loadFromFile(fp, curr_movie))
		{
			fclose(fp);
			puts("done!");
			movie_mode = MOVIE_PLAYBACK;
			movie_index = 0;
		}
		else
		{
			puts("load error!");
		}
	}
}

void Kdev::saveFloorDepth()
{
	// Copy depth to floor
	memcpy(
		raw_floor.image.data(),
		raw_depth.image.data(),
		640*480*sizeof(uint16));
	
	raw_floor.max_value = raw_depth.max_value;
	raw_floor.min_value = raw_depth.min_value;
	raw_floor.range     = raw_depth.range;
}

void clearFloorDepth()
{
	for (int i=0; i<640*480; ++i)
	{
		floor_depth[i] = 0;
	}
}


void change_cal_param(Calset& set, float mx, float my, const ChangeCalParamKeys& keys)
{
	auto& curr = set.curr;
	auto& prev = set.prev;

	if (keys.scale)
	{
		curr.scale = prev.scale - mx + my;
	}
	else if (keys.rot_x)
	{
		curr.rotx = prev.rotx - mx + my;
	}
	else if (keys.rot_y)
	{
		curr.roty = prev.roty - mx + my;
	}
	else if (keys.rot_z)
	{
		curr.rotz = prev.rotz - mx + my;
	}
	else
	{
		switch (global.view_mode)
		{
		case VM_2D_TOP:// ウエキャリブレーション
			if (keys.ctrl)
			{
				curr.roty = prev.roty + mx - my;
			}
			else
			{
				curr.x = prev.x + mx;
				curr.z = prev.z - my;
			}
			break;
		case VM_2D_LEFT:// ヨコキャリブレーション
			if (keys.ctrl)
			{
				curr.rotx = prev.rotx - mx + my;
			}
			else
			{
				curr.z = prev.z - mx;
				curr.y = prev.y - my;
			}
			break;
		case VM_2D_FRONT:// マエキャリブレーション
		case VM_2D_RUN:  // 走り画面
			if (keys.ctrl)
			{
				curr.rotz = prev.rotz - mx - my;
			}
			else
			{
				curr.x = prev.x + mx;
				curr.y = prev.y - my;
			}
			break;
		default:
			if (keys.ctrl)
			{
				curr.x = prev.x + mx;
				curr.y = prev.y - my;
			}				
			else
			{
				curr.x = prev.x + mx;
				curr.z = prev.z - my;
			}
			break;
		}
	}

	set.prev = set.curr;
}


void do_calibration(float mx, float my)
{
	auto keys = getChangeCalParamKeys();
	switch (camera_mode)
	{
	case CAM_A:
		change_cal_param(cal_cam1, mx, my, keys);
		break;
	case CAM_B:
		change_cal_param(cal_cam2, mx, my, keys);
		break;
	default:
		change_cal_param(cal_cam1, mx, my, keys);
		change_cal_param(cal_cam2, mx, my, keys);
		break;
	}
}

void StClient::onMouseMove(int x, int y)
{
	BYTE kbd[256];
	GetKeyboardState(kbd);

	const bool left  = (kbd[VK_LBUTTON] & 0x80)!=0;
	const bool right = (kbd[VK_RBUTTON] & 0x80)!=0;
	const bool shift = (kbd[VK_SHIFT  ] & 0x80)!=0;

	x = x * 640 / global.window_w;
	y = y * 480 / global.window_h;

	bool move_eye = false;

	if (right)
	{
		move_eye = true;
	}
	else if (left)
	{
		const float mx = (x - old_x) * 0.01f * (shift ? 0.1f : 1.0f);
		const float my = (y - old_y) * 0.01f * (shift ? 0.1f : 1.0f);
		do_calibration(mx, my);
		old_x = x;
		old_y = y;	
	}

	// キャリブレーションのときは視点移動ができない
	switch (global.view_mode)
	{
	case VM_2D_TOP:
	case VM_2D_LEFT:
	case VM_2D_FRONT:
		//move_eye = false;
		break;
	case VM_2D_RUN:
		// ゲーム画面等倍時はOK
		break;
	}

	if (move_eye)
	{
		const float x_move = (x - old_x)*0.0010;
		const float y_move = (y - old_y)*0.0100;
		eye.rh = eye_rh_base - x_move;
		eye. v = eye_rv_base + y_move;
		
		if (!shift)
		{
			eye. y = eye_y_base  - y_move;
		}
	}
}

void StClient::onMouse(int button, int state, int x, int y)
{
	if ((button==GLUT_LEFT_BUTTON || button==GLUT_RIGHT_BUTTON) && state==GLUT_DOWN)
	{
		// Convert screen position to internal position
		x = x * 640 / global.window_w;
		y = y * 480 / global.window_h;

		old_x = x;
		old_y = y;
		eye_rh_base = eye.rh;
		eye_rv_base = eye.v;
		eye_y_base  = eye.y;

		// 現在値を保存しておく
		cal_cam1.prev = cal_cam1.curr;
		cal_cam2.prev = cal_cam2.curr;		
	}
}



void StClient::onKey(int key, int /*x*/, int /*y*/)
{
	BYTE kbd[256] = {};
	GetKeyboardState(kbd);

	const bool shift = (kbd[VK_SHIFT] & 0x80)!=0;
	const bool ctrl  = (kbd[VK_CONTROL] & 0x80)!=0;
	const bool key_left  = ((kbd[VK_LEFT ] & 0x80)!=0);
	const bool key_right = ((kbd[VK_RIGHT] & 0x80)!=0);
	const bool key_up    = ((kbd[VK_UP   ] & 0x80)!=0);
	const bool key_down  = ((kbd[VK_DOWN ] & 0x80)!=0);
	
	if (key_left || key_right || key_up || key_down)
	{
		const float U = shift ? 0.001 : 0.01;
		const float mx =
				(key_left  ? -U : 0.0f) +
				(key_right ? +U : 0.0f);
		const float my =
				(key_up   ? -U : 0.0f) +
				(key_down ? +U : 0.0f);
		do_calibration(mx, my);
		return;
	}


	const float movespeed = shift ? 0.01 : 0.1;

	enum { SK_SHIFT=0x10000 };
	enum { SK_CTRL =0x20000 };

	// A           'a'
	// Shift+A     'A'
	// F1          F1
	// Shift+F1    F1 | SHIFT
	if (!isalpha(key))
	{
		key += (shift ? SK_SHIFT : 0);
		key += (ctrl  ? SK_CTRL  : 0);
	}

	switch (key)
	{
	default:
		printf("[key %d]\n", key);
		break;

	case '1':
		camera_mode = CAM_A;
		break;
	case '2':
		camera_mode = CAM_B;
		break;
	case '3':
		camera_mode = CAM_BOTH;
		break;

	case 't':
	case 'y':
	case 'u':
	case KEY_LEFT:
	case KEY_RIGHT:
	case KEY_UP:
	case KEY_DOWN:
		// キャリブレーション用に使用
		break;

	case KEY_F1: view_2d_left();  break;
	case KEY_F2: view_2d_top();   break;
	case KEY_F3: view_2d_front(); break;
	case KEY_F4: break;
	
	case KEY_F5: view_2d_run();   break;
	case KEY_F6: view_3d_left();  break;
	case KEY_F7: view_3d_right(); break;
	case KEY_F8: view_3d_front(); break;

	case KEY_HOME:
		config.far_threshold -= shift ? 1 : 10;
		break;
	case KEY_END:
		config.far_threshold += shift ? 1 : 10;
		break;

 	case 'a':
 	case 'A':
		eye.x += movespeed * cosf(eye.rh - 90*PI/180);
		eye.z += movespeed * sinf(eye.rh - 90*PI/180);
		break;
 	case 'd':
 	case 'D':
		eye.x += movespeed * cosf(eye.rh + 90*PI/180);
		eye.z += movespeed * sinf(eye.rh + 90*PI/180);
		break;
 	case 's':
 	case 'S':
		eye.x += movespeed * cosf(eye.rh + 180*PI/180);
		eye.z += movespeed * sinf(eye.rh + 180*PI/180);
		break;
 	case 'w':
 	case 'W':
		eye.x += movespeed * cosf(eye.rh + 0*PI/180);
		eye.z += movespeed * sinf(eye.rh + 0*PI/180);
 		break;
	case 'q':
	case 'Q':
		eye.y += -movespeed;
		break;
	case 'e':
	case 'E':
		eye.y += movespeed;
		break;
	case KEY_PAGEDOWN:
		eye.y -= movespeed;
		eye.v += movespeed;
		break;
	case KEY_PAGEUP:
		eye.y += movespeed;
		eye.v -= movespeed;
		break;

	case 27:
		dev1.depth.stop();
		dev1.color.stop();
		dev1.depth.destroy();
		dev1.color.destroy();
		dev1.device.close();
		openni::OpenNI::shutdown();
		exit(1);
	case 'z':
		global.client_status = STATUS_DEPTH;
		break;
	case SK_CTRL + KEY_F1:
		if (movie_mode!=MOVIE_RECORD)
		{
			curr_movie.frames.clear();
			curr_movie.frames.resize(MOVIE_MAX_FRAMES);
			curr_movie.recorded_tail = 0;
			movie_mode = MOVIE_RECORD;
			movie_index = 0;
		}
		else
		{
			printf("recoding stop. %d frames recorded.\n", curr_movie.recorded_tail);

			size_t total_bytes = 0;
			for (size_t i=0; i<curr_movie.recorded_tail; ++i)
			{
				total_bytes += curr_movie.frames[i].size();
			}
			printf("total %u Kbytes.\n", total_bytes/1000);
			movie_mode = MOVIE_READY;
			movie_index = 0;
		}
		break;
	case SK_CTRL + KEY_F2:
		printf("playback movie.\n");
		movie_mode = MOVIE_PLAYBACK;
		movie_index = 0;
		break;
	
	case KEY_F9:
		load_config();
		break;

	case 'C':  toggle(mode.auto_clipping); break;
	case 'k':  toggle(mode.sync_enabled);  break;
	case 'm':  toggle(mode.mixed_enabled); break;
	case 'M':  toggle(mode.mirroring); break;
	case 'b':  toggle(mode.borderline);    break;
	case 'B':  toggle(mode.simple_dot_body);  break;

	case '$':
		toggle(mode.view4test);
		break;
	case 'T':
		clearFloorDepth();
		break;
	case 'X':
		dev1.saveFloorDepth();
		break;
	case 13:
		gl::ToggleFullScreen();
		break;
	}
}

bool StClient::initOpenGL(int argc, char **argv)
{
#if USE_GLFW
	glfwInit();
	glfwOpenWindow(0, 0, 0, 0, 0, 0, 0, 0, GLFW_WINDOW);
#else
	glutInit(&argc, argv);
	glutInitWindowPosition(
		config.initial_window_x,
		config.initial_window_y);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(INITIAL_WIN_SIZE_X, INITIAL_WIN_SIZE_Y);

	{
		std::string name;
		name += "スポーツタイムマシン クライアント";
		name += " (";
		name += Core::getComputerName();
		name += ")";
		glutCreateWindow(name.c_str());
	}

	if (config.initial_fullscreen)
	{
		gl::ToggleFullScreen();
	}

	glutIdleFunc(glutIdle);
	glutDisplayFunc(glutDisplay);
	glutKeyboardFunc(glutKeyboard);
	glutSpecialFunc(glutKeyboardSpecial);
	glutMouseFunc(glutMouse);
	glutReshapeFunc(glutReshape);
	glutMotionFunc(glutMouseMove);
	glEnable(GL_TEXTURE_2D);

	gl::ModelView();

	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	gl::AlphaBlending(true);
#endif

	return true;
}
