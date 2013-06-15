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








struct Calparam
{
	struct Point3D
	{
		int x,y;
		float z;
		
		Point3D()
		{
			x=y=0;
			z=0.0f;
		}

		void setPoint(const Point3D& a, const Point3D& b, int t, int end)
		{
			const int q = end - t;
			this->x = (t*a.x + q*b.x) / end;
			this->y = (t*a.y + q*b.y) / end;
			this->z = (t*a.z + q*b.z) / end;
		}
	};

	float rot_x, rot_y, rot_z;
	float x,y,z;

	Calparam()
	{
		x = y = z = 0.0f;
		rot_x = 0.0f;
		rot_y = 0.0f;
		rot_z = 0.0f;
	}
};

struct Calset
{
	Calparam curr, prev;
};

Calset cal_cam1, cal_cam2;

Calset* focus_cal = nullptr;

enum CameraMode
{
	CAM_A,
	CAM_B,
	CAM_BOTH,
};

CameraMode camera_mode = CAM_BOTH;
float eye_rh_base, eye_rv_base;



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

DepthScreen depth_screen;



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

TimeProfile time_profile;



static int old_x, old_y;
float eye_d  =  4.00f;
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
		const float ex = x + cos(rh)*eye_d;
		const float ez = z + sin(rh)*eye_d;
		const float ey = y + v;

		::gluLookAt(x,y,z, ex,ey,ez, 0,1,0);
	}
};

float fovy = 40.0f;
float fov_ratio = 1.0;
float fov_step  = 1.0;

Eye eye;



void view_2d_top()
{
	global.view_mode = VM_2D_TOP;
	eye.set(0.0f, 18.4, 5.2f, -PI/2, -15.0f);
}

void view_2d_front()
{
	global.view_mode = VM_2D_FRONT;
	eye.set(0.0f, 2.2f, 5.2f, -PI/2, -1.92);
}

void view_3d_left()
{
	global.view_mode = VM_3D_LEFT;
	eye.set(-3.6f, 3.8f, 5.75f, -1.06f, -1.72f);
}

void view_3d_right()
{
	global.view_mode = VM_3D_RIGHT;
	eye.set(3.6f, 3.8f, 5.75f, -2.05f, -1.72f);
}

void view_3d_front()
{
	global.view_mode = VM_3D_FRONT;
	eye.set(0.0f, 3.8f, 4.75f, -PI/2, -2.10f);
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

std::vector<HitObject> hit_objects;


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

VodyInfo vody;





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


miFps fps_counter;




enum MovieMode
{
	MOVIE_READY,
	MOVIE_RECORD,
	MOVIE_PLAYBACK,
};

MovieMode movie_mode = MOVIE_READY;


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

size_t movie_index = 0;

openni::RGB888Pixel* moviex = nullptr;


GLuint depth_tex;



// @constructor, @init
StClient::StClient(Kdev& dev1_, Kdev& dev2_) :
	dev1(dev1_),
	dev2(dev2_),
	video_ram(nullptr),
	video_ram2(nullptr)
{
	ms_self = this;

	glGenTextures(1, &depth_tex);

	view_3d_left();


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


freetype::font_data monospace;





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


int decomp_time = 0;
int draw_time = 0;
uint8 floor_depth[640*480];
uint8 depth_cook[640*480];
int floor_depth_count = 0;
uint16 floor_depth2[640*480];




void Kdev::RawDepthImageToRgbaTex(const RawDepthImage& raw, RgbaTex& dimg)
{
	const uint16* src = raw.image.data();
	const int range = max(1, raw.range);
	for (int y=0; y<480; ++y)
	{
		RGBA_raw* dest = dimg.vram + y*dimg.pitch;
		for (int x=0; x<640; ++x, ++dest, ++src)
		{
			const uint16 d = *src;
			uint v = 255 * (d - raw.min_value) / range;
			if (v==0)
			{
				// n/a
				dest->set(80,50,0);
			}
			else if (v>255)
			{
				// too far!
				dest->set(0,30,70);
			}
			else
			{
				if (mode.borderline && v>=20 && v<=240)
				{
					if (v%2==0)
						v = 20;
					else
						v = 240;
				}

				dest->set(
					(255-v)*256>>8,
					(255-v)*230>>8,
					(255-v)*50>>8,
					255);
			}
		}
	}
}


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

	heading.glColorUpdate();
	pr(monospace, 20, y+=H, "View Mode: %s",
		(global.view_mode==VM_2D_TOP)   ? "2D top" :
		(global.view_mode==VM_2D_FRONT) ? "2D front" :
		(global.view_mode==VM_3D_LEFT)  ? "3D left" :
		(global.view_mode==VM_3D_RIGHT) ? "3D right" :
		(global.view_mode==VM_3D_FRONT) ? "3D front" : "unknown");
	text.glColorUpdate();
	pr(monospace, 20, y+=H, "[F1] 2D top");
	pr(monospace, 20, y+=H, "[F2] 2D front");
	pr(monospace, 20, y+=H, "[F3] 3D left");
	pr(monospace, 20, y+=H, "[F4] 3D right");
	pr(monospace, 20, y+=H, "[F5] 3D front");
	nl();

	heading.glColorUpdate();
	pr(monospace, 20, y+=H, "EYE");
	text.glColorUpdate();
	pr(monospace, 20, y+=H, "x =%+9.4f", eye.x);
	pr(monospace, 20, y+=H, "y =%+9.4f", eye.y);
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
	pr(monospace, 20, y+=H, "rot x = %9.5f", cal_cam1.curr.rot_x);
	pr(monospace, 20, y+=H, "rot y = %9.5f", cal_cam1.curr.rot_y);
	pr(monospace, 20, y+=H, "rot z = %9.5f", cal_cam1.curr.rot_z);
	nl();

	heading.glColorUpdate();
	pr(monospace, 20, y+=H, "Camera B:");
	text.glColorUpdate();
	pr(monospace, 20, y+=H, "pos x = %9.5f", cal_cam2.curr.x);
	pr(monospace, 20, y+=H, "pos y = %9.5f", cal_cam2.curr.y);
	pr(monospace, 20, y+=H, "pos z = %9.5f", cal_cam2.curr.z);
	pr(monospace, 20, y+=H, "rot x = %9.5f", cal_cam2.curr.rot_x);
	pr(monospace, 20, y+=H, "rot y = %9.5f", cal_cam2.curr.rot_y);
	pr(monospace, 20, y+=H, "rot z = %9.5f", cal_cam2.curr.rot_z);
	nl();

	heading.glColorUpdate();
	pr(monospace, 20, y+=H, "Camera:");
	text.glColorUpdate();
	pr(monospace, 20, y+=H, " X 2D top view");
	pr(monospace, 20, y+=H, " [w] 3D view (left)");
	pr(monospace, 20, y+=H, " [e] 3D view (right)");
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






void MixDepth(Dots& dots, const RawDepthImage& src, const Calparam& calparam)
{
	mat4x4 trans;
	{
		// X軸回転
		float cos = cosf(calparam.rot_x);
		float sin = sinf(calparam.rot_x);
		trans = mat4x4(
			1,   0,    0, 0,
			0, cos, -sin, 0,
			0, sin,  cos, 0,
			0,   0,    0, 1) * trans;
	}
	{
		// Y軸回転
		float cos = cosf(calparam.rot_y);
		float sin = sinf(calparam.rot_y);
		trans = mat4x4(
			 cos, 0, sin, 0,
			   0, 1,   0, 0,
			-sin, 0, cos, 0,
			   0, 0,   0, 1) * trans;
	}
	{
		// Z軸回転
		float cos = cosf(calparam.rot_z);
		float sin = sinf(calparam.rot_z);
		trans = mat4x4(
			cos,-sin, 0, 0,
			sin, cos, 0, 0,
			  0,   0, 1, 0,
			  0,   0, 0, 1) * trans;
	}

	// 平行移動
	trans = mat4x4(
		1, 0, 0, calparam.x,
		0, 1, 0, calparam.y,
		0, 0, 1, calparam.z,
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

void drawBodyDS(const Dots& dots, glRGBA rgba)
{
	// @body @dot
	glBegin(GL_POINTS);

	for (size_t i=0; i<dots.size(); ++i)
	{
		float x = dots[i].x;
		float y = dots[i].y;
		float z = dots[i].z;

		float col = z/4;
		if (col<0.25f) col=0.25f;
		if (col>1.0f) col=1.0f;
		col = 1.0f - col;

		glRGBA(
			rgba.r,
			rgba.g,
			rgba.b,
			(int)(col*255)).glColorUpdate();

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
	bool draw3d = false;
	bool draw2d = false;
	::glMatrixMode(GL_PROJECTION);
	::glLoadIdentity();
	switch (global.view_mode)
	{
	case VM_2D_TOP:
		glOrtho(-3,+3, 0,(6.0*3/4), 1.0,+100);
		draw2d = true;
		break;
	case VM_2D_FRONT:
		glOrtho(-2,+2, 0,3, 1.0,+100);
		draw2d = true;
		break;
	case VM_3D_LEFT:
		gluPerspective(30.0f, 4.0f/3.0f, 1.0f, 100.0f);
		draw3d = true;
		break;
	case VM_3D_RIGHT:
		gluPerspective(30.0f, 4.0f/3.0f, 1.0f, 100.0f);
		draw3d = true;
		break;
	case VM_3D_FRONT:
		gluPerspective(30.0f, 4.0f/3.0f, 1.0f, 100.0f);
		draw3d = true;
		break;
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

	if (camera_mode==CAM_BOTH)
	{
		{
			DS_Init(dot_set);
			MixDepth(dot_set, dev1.raw_depth, cal_cam1.curr);
			MixDepth(dot_set, dev2.raw_depth, cal_cam2.curr);
			drawBodyDS(dot_set, color_both);
		}
	}
	else
	{
		{
			DS_Init(dot_set);
			MixDepth(dot_set, dev1.raw_depth, cal_cam1.curr);
			drawBodyDS(dot_set, (camera_mode==CAM_A) ? color_cam1 : color_other);
		}
		{
			DS_Init(dot_set);
			MixDepth(dot_set, dev2.raw_depth, cal_cam2.curr);
			drawBodyDS(dot_set, (camera_mode==CAM_B) ? color_cam2 : color_other);
		}
	}


#if 0
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
			zlibpp::bytes& byte_stream = curr_movie.frames[curr_movie.recorded_tail++];
			zlibpp::compress(
				(byte*)dev1.raw_depth.image.data(),
				640*480*sizeof(uint16),
				byte_stream, 2);
			printf("frame %d, %d bytes (%.1f%%)\n",
				curr_movie.recorded_tail,
				byte_stream.size(),
				byte_stream.size() * 100.0 / (640*480));
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

			zlibpp::bytes outdata;
			zlibpp::decompress(
				byte_stream,
				outdata);
			RawDepthImage raw;
			const uint16* raw_depth = (const uint16*)outdata.data();
			raw.image.resize(640*480);
			for (int i=0; i<640*480; ++i)
			{
				raw.image[i] = raw_depth[i];
			}
			raw.CalcDepthMinMax();
			{
				drawBody(raw, 185,140,166);
			}
		}
		break;
	}
#endif


	//============
	// 2D Section
	//============
	gl::Texture(false);
	gl::DepthTest(false);
	glOrtho(0, 640, 480, 0, -1.0, 1.0);

	gl::Projection();
	gl::LoadIdentity();

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

	{
		ModelViewObject mo;
		display2();
	}

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

void StClient::onMouseMove(int x, int y)
{
	BYTE kbd[256];
	GetKeyboardState(kbd);

	const bool left  = (kbd[VK_LBUTTON] & 0x80)!=0;
	const bool right = (kbd[VK_RBUTTON] & 0x80)!=0;
	const bool shift = (kbd[VK_SHIFT  ] & 0x80)!=0;
//	const bool ctrl  = (kbd[VK_CONTROL] & 0x80)!=0;
//	const bool alt   = (kbd[VK_MENU   ] & 0x80)!=0;

	const bool rot_x  = (kbd['T'] & 0x80)!=0;
	const bool rot_y  = (kbd['Y'] & 0x80)!=0;
	const bool rot_z  = (kbd['U'] & 0x80)!=0;

	x = x * 640 / global.window_w;
	y = y * 480 / global.window_h;

	bool move_eye = false;

	if (right)
	{
		move_eye = true;
	}
	else if (left)
	{
		switch (camera_mode)
		{
		case CAM_A:
		case CAM_B:
			if (rot_x)
			{
				focus_cal->curr.rot_x = focus_cal->prev.rot_x - (x - old_x)/100.0f;
			}
			else if (rot_y)
			{
				focus_cal->curr.rot_y = focus_cal->prev.rot_y - (x - old_x)/100.0f;
			}
			else if (rot_z)
			{
				focus_cal->curr.rot_z = focus_cal->prev.rot_z - (x - old_x)/100.0f;
			}
			else if (shift)
			{
				focus_cal->curr.x = focus_cal->prev.x + (x - old_x)/100.0f;
				focus_cal->curr.y = focus_cal->prev.y - (y - old_y)/100.0f;
			}
			else
			{
				focus_cal->curr.x = focus_cal->prev.x + (x - old_x)/100.0f;
				focus_cal->curr.z = focus_cal->prev.z - (y - old_y)/100.0f;
			}
			focus_cal->prev = focus_cal->curr;
			old_x = x;
			old_y = y;
			break;
		case CAM_BOTH:
			move_eye = true;
			break;
		}
	}

	if (move_eye)
	{
		eye.rh = eye_rh_base - (x - old_x)*0.0025 * fov_ratio;
		eye. v = eye_rv_base + (y - old_y)*0.0100 * fov_ratio;
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
		eye_rv_base = eye. v;

		switch (camera_mode)
		{
		case CAM_A:
			focus_cal = &cal_cam1;
			break;
		case CAM_B:
			focus_cal = &cal_cam2;
			break;
		default:
			focus_cal = nullptr;
			break;
		}

		// 現在値を保存しておく
		if (focus_cal!=nullptr)
		{
			focus_cal->prev = focus_cal->curr;
		}
	}
}



void StClient::onKey(int key, int /*x*/, int /*y*/)
{
	BYTE kbd[256]={};
	GetKeyboardState(kbd);

	const bool shift = (kbd[VK_SHIFT] & 0x80)!=0;
	const bool ctrl  = (kbd[VK_CONTROL] & 0x80)!=0;
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
		// キャリブレーション用に使用
		break;

	case KEY_F1: view_2d_top(); break;
	case KEY_F2: view_2d_front(); break;
	case KEY_F3: view_3d_left(); break;
	case KEY_F4: view_3d_right(); break;
	case KEY_F5: view_3d_front(); break;

	case KEY_HOME:
		config.far_threshold -= shift ? 1 : 10;
		break;
	case KEY_END:
		config.far_threshold += shift ? 1 : 10;
		break;

	case 'a':
	case 'A':
	case KEY_LEFT:
		eye.x += movespeed * cos(eye.rh - 90*PI/180);
		eye.z += movespeed * sin(eye.rh - 90*PI/180);
		break;
	case 'd':
	case 'D':
	case KEY_RIGHT:
		eye.x += movespeed * cos(eye.rh + 90*PI/180);
		eye.z += movespeed * sin(eye.rh + 90*PI/180);
		break;
	case 's':
	case 'S':
	case KEY_DOWN:
		eye.x += movespeed * cos(eye.rh + 180*PI/180) * fov_step;
		eye.z += movespeed * sin(eye.rh + 180*PI/180) * fov_step;
		break;
	case 'w':
	case 'W':
	case KEY_UP:
		eye.x += movespeed * cos(eye.rh + 0*PI/180) * fov_step;
		eye.z += movespeed * sin(eye.rh + 0*PI/180) * fov_step;
		break;
	case 'q':
	case 'Q':
	case KEY_PAGEDOWN:
		eye.y += -movespeed;
		break;
	case 'e':
	case 'E':
	case KEY_PAGEUP:
		eye.y += movespeed;
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
	
	case KEY_F8:
		load_config();
		break;

	case 'C':  toggle(mode.auto_clipping); break;
	case 'k':  toggle(mode.sync_enabled);  break;
	case 'm':  toggle(mode.mixed_enabled); break;
	case 'M':  toggle(mode.mirroring); break;
	case 'b':  toggle(mode.borderline);    break;
	case 'B':  toggle(mode.simple_dot_body);  break;

	case 'g':
		fovy -= 0.05;
		break;
	case 'h':
		fovy += 0.05;
		break;

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
