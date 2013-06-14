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

#define USE_GLFW 0


enum CameraMode
{
	CAM_A,
	CAM_B,
	CAM_BOTH,
};

CameraMode camera_mode = CAM_A;





const float PI = 3.141592653;


bool debug_bool = false;


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

float ex,ey,ez;
float eye_d  =  4.00f;
float eye_rh =  PI/2;
float eye_rv =  0.42;
float eye_x  = 0;
float eye_z  = -3.00f;

float eye_y  =  0.33f;
float fovy = 40.0f;
float fov_ratio = 1.0;
float fov_step  = 1.0;




void Kdev::initRam()
{
	glGenTextures(1, &this->vram_tex);
	glGenTextures(1, &this->vram_floor);
	this->img_rawdepth   .create(640,480);
	this->img_floor      .create(640,480);
	this->img_cooked     .create(640,480);
	this->img_transformed.create(640,480);
	
	calibration.a = Point2i(0,0);
	calibration.b = Point2i(640,0);
	calibration.c = Point2i(0,480);
	calibration.d = Point2i(640,480);

#if 0
	fov_ratio = 1.0;
	fovy   =    40.0f;
	eye_x  =  0;
	eye_y  =  1.3;
	eye_z  = -4.6;
	eye_rh = PI/2;
	eye_rv = 0.135;
#else
	// view1
	fov_ratio = 1.0;
	fovy   =    60.0f;
	eye_x  =  4.3f;
	eye_y  =  2.2f;
	eye_z  = -3.0f;
	eye_rh = 2.34500f;
	eye_rv = 1.00000f;

	// view2
	fov_ratio = 1.0;
	fovy   =    60.0f;
	eye_x  = -4.0f;
	eye_y  =  2.2f;
	eye_z  = -4.0f;
	eye_rh = 0.91750f;
	eye_rv = 1.30000f;
#endif
}







Point2i* calibration_focus = nullptr;

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

const int GL_WIN_SIZE_X = 640;
const int GL_WIN_SIZE_Y	= 480;
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
	log(mode.zero255_show,     'z', "zero255");
	log(mode.alpha_mode,       'a', "alpha");
	log(mode.pixel_completion, 'e', "pixel completion");
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

	udp_recv.init(UDP_CLIENT_RECV);
	printf("host: %s\n", Core::getComputerName().c_str());
	printf("ip: %s\n", mi::Udp::getIpAddress().c_str());

	mode.mirroring   = config.mirroring;
	mode.calibration = false;

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
		monospace.init(font_folder + "Courbd.ttf", 12);
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


void StClient::drawImageMode()
{
	using namespace openni;

	const RGB888Pixel* source_row = (const RGB888Pixel*)dev1.colorFrame.getData();
	RGBA_raw* dest_row = video_ram + dev1.colorFrame.getCropOriginY() * m_nTexMapX;
	const int source_rows = dev1.colorFrame.getStrideInBytes() / sizeof(RGB888Pixel);

	for (int y=0; y<dev1.colorFrame.getHeight(); ++y)
	{
		const RGB888Pixel* src = source_row;
		RGBA_raw* dest = dest_row + dev1.colorFrame.getCropOriginX();

		for (int x=0; x<dev1.colorFrame.getWidth(); ++x)
		{
			dest->r = src->r;
			dest->g = src->g;
			dest->b = src->b;
			dest->a = 255;
			++dest, ++src;
		}

		dest_row   += m_nTexMapX;
		source_row += source_rows;
	}

#if 0
	drawBitmap(
		0, 0, GL_WIN_SIZE_X, GL_WIN_SIZE_Y,
		m_pTexMap,
		m_nTexMapX, m_nTexMapY,
		0.0f,
		0.0f,
		(float)m_width  / m_nTexMapX,
		(float)m_height / m_nTexMapY);
#endif
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
		0, 0, GL_WIN_SIZE_X, GL_WIN_SIZE_Y,
		0.0f,
		0.0f,
		(float)m_width  / m_nTexMapX,
		(float)m_height / m_nTexMapY);
}


void StClient::drawDepthMode()
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
	const int draw_w = GL_WIN_SIZE_X/2;
	const int draw_h = GL_WIN_SIZE_Y/2;
	drawBitmap(
		draw_x, draw_y,
		draw_w, draw_h,
		0.0f,
		0.0f,
		(float)m_width  / m_nTexMapX,
		(float)m_height / m_nTexMapY);
}

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
	freetype::print(monospace, 0, 476, "%dmm",
		config.metrics.left_mm);
	freetype::print(monospace, 570, 476, "%5dmm",
		config.metrics.right_mm);

	freetype::print(monospace, 20, 160, "#%d Near(%dmm) Far(%dmm) [%s][%s]",
			config.client_number,
			config.near_threshold,
			config.far_threshold,
			mode.borderline ? "border" : "no border",
			mode.auto_clipping ? "auto clipping" : "no auto clip");

	// @fps
	freetype::print(monospace, 20, 180, "%d, %d, %.1ffps, %.2ffps, %d, %d",
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



void StClient::drawKdev(Kdev& kdev, int x, int y, int w, int h)
{
	w /= 2;
	h /= 2;
	const int x1 = x;
	const int y1 = y;
	const int x2 = x+w;
	const int y2 = y+h;

	if (mode.view4test)
	{
		kdev.raw_depth.CalcDepthMinMax();
		kdev.RawDepthImageToRgbaTex(dev1.raw_depth, dev1.img_rawdepth);
		DrawRgbaTex(dev1.img_rawdepth, x1,y1, w,h);
	}

	// Mode 2: Floor
	{
		if (mode.view4test){
			DrawRgbaTex(kdev.img_floor, x2,y1, w,h);
		}
	}

	// Mode 3: Floor filtered depth (cooked depth)
	{
		CreateCoockedDepth(kdev.raw_cooked, kdev.raw_depth, kdev.raw_floor);
		if (mode.view4test){
			kdev.RawDepthImageToRgbaTex(kdev.raw_cooked, kdev.img_cooked);
			DrawRgbaTex(kdev.img_cooked, x1,y2, w,h);
		}
	}

	// Mode 4: Transformed depth
	{
		kdev.CreateTransformed();
		kdev.RawDepthImageToRgbaTex(kdev.raw_transformed, kdev.img_transformed);
		if (mode.view4test){
			DrawRgbaTex(kdev.img_transformed, x2,y2, w,h);
		}else{
			DrawRgbaTex(kdev.img_transformed, x1,y1,2*w,2*h);
		}
	}
}


void drawFieldGrid(int size_cm)
{
	glBegin(GL_LINES);
	const float F = size_cm/100.0f;

	glLineWidth(1.0f);
	for (int i=0; i<size_cm; i+=50)
	{
		const float f = i/100.0f;

		//
		if (i==0)
		{
			// centre line
			glRGBA(0.25f, 0.66f, 1.00f, 1.00f).glColorUpdate();
		}
		else
		{
			glRGBA(
				global_config.grid_r,
				global_config.grid_g,
				global_config.grid_b,
				0.40f).glColorUpdate();
		}

		glVertex3f(-F, 0,  f);
		glVertex3f(+F, 0,  f);
		glVertex3f(-F, 0, -f);
		glVertex3f(+F, 0, -f);

		glVertex3f( f, 0, -F);
		glVertex3f( f, 0, +F);
		glVertex3f(-f, 0, -F);
		glVertex3f(-f, 0, +F);
	}

	glEnd();

	// run space
	glRGBA(0.25f, 1.00f, 0.25f, 0.25f).glColorUpdate();
	glBegin(GL_QUADS);
	glVertex3f(-2, 0, 2);
	glVertex3f(-2, 0, 0);
	glVertex3f( 2, 0, 0);
	glVertex3f( 2, 0, 2);
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


void drawBody(RawDepthImage& raw, int red, int green, int blue)
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

//	const int RANGE = min(dev1.raw_depth.range;
//
	

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



void drawBodyDS(DepthScreen& ds, int red, int green, int blue)
{
#define USE_DOT_TEX 1

	// @body @dot
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

//	const int RANGE = min(dev1.raw_depth.range;
//

	for (int i=0; i<1024; ++i)
	{
		ds[0].big_depth[i] = 10;
		ds[511].big_depth[i] = 10;
	}

	
	for (int y=0; y<512; ++y)
	{
		if (ds[y].begin==DepthLine::INVALID)
		{
			// ignore this line
			continue;
		}

		const DepthLine& dl = ds[y];
		for (int x=0; x<1024; ++x)
		{
			const int val = dl.big_depth[x];
			if (val==0)
			{
				continue;
			}

#if 1
			if (val < config.near_threshold)
			{
				continue;
			}
			
			if (val > config.far_threshold)
			{
				continue;
			}
#endif

			// - dev1.raw_depth.min_value
			int depth = 0;
			if (val==10)
			{
				depth = 0;
				glRGBA(
					240,
					240,
					50,
					255).glColorUpdate();
			}
			else
			{
				depth = (val - config.near_threshold)*255 / (config.far_threshold - config.near_threshold);

				int alpha = depth;
				//int alpha = 240;
				if (alpha<0) continue;
				if (alpha>255) alpha=255;
				//alpha = 255 - alpha;

				glRGBA(
					red   * alpha >> 8,
					green * alpha >> 8,
					blue  * alpha >> 8,
					alpha/2).glColorUpdate();
			}

			const float z = depth/1000.0f - 0.5;
			const float dy = (480-y)/512.0f * 2.6;
			const float dz =  z;
			
			// -0.5 < dx01 < 0.5
			const float dx01 = (x/1024.0f) - 0.5;
			const float dx = (dx01) * 4;

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


	GLUquadricObj *sphere = gluNewQuadric();
	gluQuadricDrawStyle(sphere, GLU_LINE);

	glRGBA(255,255,255).glColorUpdate();
	const float r = 0.05;
	glPushMatrix();
		glLoadIdentity();
		glTranslatef(-2, 0, 0);
		gluSphere(sphere, r, 10.0, 10.0);

		glLoadIdentity();
		glTranslatef(+2, 0, 0);
		gluSphere(sphere, r, 10.0, 10.0);

		glLoadIdentity();
		glTranslatef(-2, 2.4, 0);
		gluSphere(sphere, r, 10.0, 10.0);

		glLoadIdentity();
		glTranslatef(+2, 2.4, 0);
		gluSphere(sphere, r, 10.0, 10.0);
	glPopMatrix();

	gluDeleteQuadric(sphere);
}



void DS_Init(DepthScreen& ds)
{
	for (size_t i=0; i<ds.size(); ++i)
	{
		DepthLine& dl = ds[i];
		memset(dl.big_depth, 0, sizeof(dl.big_depth));
		dl.begin = DepthLine::INVALID;
		dl.end   = DepthLine::INVALID;
	}
}


void CreateMixed(DepthScreen& ds, const RawDepthImage& src)
{
	for (int y=0; y<480; ++y)
	{
		for (int x=0; x<640; ++x)
		{
			const int depth = src.image[x + y*640];
			
			if (depth==0)
			{
				// ignore
				continue;
			}

			const volatile int dx = x + 80;
			const volatile int dy = y - 40;

			if ((uint)dx >= 1024)
			{
				// x-overflow
				continue;
			}
			if ((uint)dy >= 512)
			{
				// y-overflow
				continue;
			}

			DepthLine& dl = ds[dy];
			if (dl.begin==DepthLine::INVALID)
			{
				dl.begin = dx;
				dl.end   = dx;
			}
			else
			{
				dl.begin = min(dl.begin, dx);  // enlarge left  <--
				dl.end   = max(dl.end,   dx);  // enlarge right -->
			}

			auto& dest = dl.big_depth[dx*1024/640];
			if (dest==0)
			{
				dest = depth;
			}
			else
			{
				dest = min(dest, depth);
			}
		}
	}
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


	gl::Texture(false);
	gl::DepthTest(true);

	ex = eye_x + cos(eye_rh)*eye_d;
	ez = eye_z + sin(eye_rh)*eye_d;
	ey = eye_rv;

	// @fov
	::glMatrixMode(GL_PROJECTION);
	::glLoadIdentity();
	
	if (1)
	{
		gluPerspective(30.0f, 4.0f/3.0f, 1.0f, 100.0f);
	}
	else
	{
		const float left   = -2.0f;
		const float right  = +2.0f;
		const float top    =  2.4f;
		const float bottom =  0.0f;
		const float nearf  = -5.0;
		const float farf   =  24.0;
		glOrtho(left, right, bottom, top, nearf, farf);
	}



#if 1
	::gluLookAt(
		eye_x,
		eye_y,
		eye_z,
		
		ex,
		ey,
		ez,
		0.0, 1.0, 0.0);
#endif

	const float diff_x = eye_x - ex;
	const float diff_y = eye_y - ey;
	const float diff_z = eye_z - ez;




	::glMatrixMode(GL_MODELVIEW);
	::glLoadIdentity();

	{
		mi::Timer tm(&time_profile.draw_wall);
		drawWall();
	}
	{
		mi::Timer tm(&time_profile.draw_grid);
		drawFieldGrid(500);
	}
	
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

//#	RawDepthImage raw_mixed;
//#	CreateMixed(raw_mixed, dev1.raw_depth);

	{
		// @dsinit
		DS_Init(depth_screen);

		// @body @draw @camera
		const char* text = "?";
		switch (camera_mode)
		{
		case CAM_A:
			CreateMixed(depth_screen, dev1.raw_depth);
			text = "A Only";
			drawBodyDS(depth_screen, 250,190,80);
			break;
		case CAM_B:
			CreateMixed(depth_screen, dev2.raw_depth);
			text = "B Only";
			drawBodyDS(depth_screen, 100,120,250);
			break;
		case CAM_BOTH:
			CreateMixed(depth_screen, dev1.raw_depth);
			CreateMixed(depth_screen, dev2.raw_depth);
			text = "Both";
			drawBodyDS(depth_screen, 250,250,250);
			break;
		}
		{
			ModelViewObject mo;
			glRGBA::white.glColorUpdate();
			freetype::print(monospace, 300,50,
				"[camera=%s]",
				text);
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


	{
		ModelViewObject mo;
		glRGBA::white.glColorUpdate();
		freetype::print(monospace, 20,420, "min:%dmm max:%dmm",
				dev1.raw_depth.min_value,
				dev1.raw_depth.max_value);
		freetype::print(monospace, 20,440, "EYE-HV(%.4f,%.4f) EYE-XZ(%.3f,%.3f,%.3f)",
				eye_rh,
				eye_rv,
				eye_x,
				eye_y,
				eye_z);
		freetype::print(monospace, 20,460, "DIFF(%.2f,%.2f,%.2f) FOVY(%.2f)",
			diff_x,
			diff_y,
			diff_z,
			fovy);

		freetype::print(monospace, 20,360, "read=[%5.2f,%5.2f] read2=[%5.2f,%4.2f] d-depth=%.2f, d-grid=%.2f, d-wall=%.2f",
			time_profile.read1_depth_dev1,
			time_profile.read1_depth_dev2,
			time_profile.read2_depth_dev1,
			time_profile.read2_depth_dev2,

			time_profile.draw_depth,
			time_profile.draw_grid,
			time_profile.draw_wall);
	}

	glRGBA::white.glColorUpdate();

	{
		ModelViewObject mo;
		display2();
	}

	if (mode.view4test)
	{
		ModelViewObject mo;
		glRGBA::white.glColorUpdate();
		freetype::print(monospace,  20,  20, "1. Raw depth view");
		freetype::print(monospace, 340,  20, "2. Floor filtered view");
		freetype::print(monospace,  20, 260, "3. Floor view");
		freetype::print(monospace, 340, 260, "4. Transformed view");
	}

	glRGBA(255,255,255,100).glColorUpdate();
	gl::Line2D(Point2i(0,240), Point2i(640,240));
	gl::Line2D(Point2i(320,0), Point2i(320,480));

	if (!mode.view4test)
	{
		gl::Line2D(Point2i(0, 40), Point2i(640, 40));
		gl::Line2D(Point2i(0,440), Point2i(640,440));
	}

	if (!mode.view4test)
	{
		ModelViewObject mo;
		glRGBA(255,255,255).glColorUpdate();
		freetype::print(monospace, 580,  40, "2400mm");
		freetype::print(monospace, 580, 240, "1200mm");
		freetype::print(monospace, 580, 440, "   0mm");
		freetype::print(monospace, 320,  10, "2m");
	}

	if (mode.calibration)
	{
		displayCalibrationInfo();
	}

	glBegin(GL_POINTS);
	glRGBA(255,255,255).glColorUpdate();
		glVertex2d(old_x, old_y);
	glEnd();

#if !USE_GLFW
	glutSwapBuffers();
#endif
}


void StClient::displayCalibrationInfo()
{
	gl::Texture(false);
	gl::DepthTest(false);

	glRGBA(0,0,0,128).glColorUpdate();
	glBegin(GL_QUADS);
		Point2f(0,0).glVertex2();
		Point2f(700,0).glVertex2();
		Point2f(640,480).glVertex2();
		Point2f(0,480).glVertex2();
	glEnd();


	auto draw_calib = [&](const KinectCalibration& kc, glRGBA rgba){
		rgba.glColorUpdate();
		glBegin(GL_LINE_LOOP);
			kc.a.glVertex2();
			kc.b.glVertex2();
			kc.d.glVertex2();
			kc.c.glVertex2();
		glEnd();
	};

	draw_calib(dev1.calibration, glRGBA(250,220,220));
	draw_calib(dev2.calibration, glRGBA(220,220,250));

	if (calibration_focus!=nullptr)
	{
		glRGBA::white.glColorUpdate();
		gl::RectangleFill(
			calibration_focus->x-5,
			calibration_focus->y-5,
			10,10);
	}

	{
		ModelViewObject m;
		glRGBA::white.glColorUpdate();
		{
			const auto& kc = dev1.calibration;
			freetype::print(monospace, 20,  20, "[Kinect-1 Calibration Mode]");
			freetype::print(monospace, 20,  40, "A = (%d,%d)", kc.a.x, kc.a.y);
			freetype::print(monospace, 20,  60, "B = (%d,%d)", kc.b.x, kc.b.y);
			freetype::print(monospace, 20,  80, "C = (%d,%d)", kc.c.x, kc.c.y);
			freetype::print(monospace, 20, 100, "D = (%d,%d)", kc.d.x, kc.d.y);
		}
		{
			const auto& kc = dev2.calibration;
			freetype::print(monospace, 20, 120, "[Kinect-2 Calibration Mode]");
			freetype::print(monospace, 20, 140, "A = (%d,%d)", kc.a.x, kc.a.y);
			freetype::print(monospace, 20, 160, "B = (%d,%d)", kc.b.x, kc.b.y);
			freetype::print(monospace, 20, 180, "C = (%d,%d)", kc.c.x, kc.c.y);
			freetype::print(monospace, 20, 200, "D = (%d,%d)", kc.d.x, kc.d.y);
		}
	}
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

	// Convert floor data: uint16[] to RGBA
	RawDepthImageToRgbaTex(raw_floor, img_floor);
}

void clearFloorDepth()
{
	for (int i=0; i<640*480; ++i)
	{
		floor_depth[i] = 0;
	}
}

float eye_rh_base, eye_rv_base;

void StClient::onMouseMove(int x, int y)
{
	const bool left_first  = (GetAsyncKeyState(VK_LBUTTON) & 1)!=0;
	const bool right_first = (GetAsyncKeyState(VK_RBUTTON) & 1)!=0;
	const bool left  = (GetAsyncKeyState(VK_LBUTTON) & 0x8000)!=0;
	const bool right = (GetAsyncKeyState(VK_RBUTTON) & 0x8000)!=0;

	if (left)
	{
		x = x * 640 / global.window_w;
		y = y * 480 / global.window_h;

		eye_rh = eye_rh_base - (x - old_x)*0.0025 * fov_ratio;
		eye_rv = eye_rv_base + (y - old_y)*0.0100 * fov_ratio;
	}
}

void StClient::onMouse(int button, int state, int x, int y)
{
	if (button==GLUT_LEFT_BUTTON && state==GLUT_DOWN)
	{
		// Convert screen position to internal position
		x = x * 640 / global.window_w;
		y = y * 480 / global.window_h;

		old_x = x;
		old_y = y;
		eye_rh_base = eye_rh;
		eye_rv_base = eye_rv;

		// 補正なし時のみ設定できる
		if (calibration_focus!=nullptr && !mode.calibration)
		{
			calibration_focus->x = x;
			calibration_focus->y = y;
		}
	}
}

void view1()
{
	fov_ratio = 1.0;
	fovy   =    60.0f;
	eye_x  =  4.3f;
	eye_y  =  2.2f;
	eye_z  = -4.0f;
	eye_rh = 2.34500f;
	eye_rv = 1.30000f;
}

void view2()
{
	fov_ratio = 1.0;
	fovy   =    60.0f;
	eye_x  = -4.0f;
	eye_y  =  2.2f;
	eye_z  = -4.0f;
	eye_rh = 0.91750f;
	eye_rv = 1.30000f;
}

void view3()
{
	fov_ratio = 1.0;
	fovy   =    60.0f;
	eye_x  =  0.0f;
	eye_y  =  2.2f;
	eye_z  = -6.3f;
	eye_rh = PI/2;
	eye_rv = 1.60f;
}

void StClient::onKey(int key, int /*x*/, int /*y*/)
{
	BYTE kbd[256]={};
	GetKeyboardState(kbd);

	const bool shift = (kbd[VK_SHIFT] & 0x80)!=0;
	const float movespeed = shift ? 0.01 : 0.1;

	enum { SK_SHIFT=0x10000 };

	switch (key + (shift ? SK_SHIFT : 0))
	{
	default:
		printf("[key %d]\n", key);
		break;

	case KEY_F8  | SK_SHIFT:   view1();  break;
	case KEY_F9  | SK_SHIFT:   view2();  break;
	case KEY_F10 | SK_SHIFT:   view3();  break;
	case KEY_F8:    camera_mode = CAM_A;  break;
	case KEY_F9:    camera_mode = CAM_B;  break;
	case KEY_F10:   camera_mode = CAM_BOTH;  break;

	case KEY_PAGEUP:
		config.near_threshold -= shift ? 1 : 10;
		break;
	case KEY_PAGEDOWN:
		config.near_threshold += shift ? 1 : 10;
		break;
	case KEY_HOME:
		config.far_threshold -= shift ? 1 : 10;
		break;
	case KEY_END:
		config.far_threshold += shift ? 1 : 10;
		break;

	case 'a':
	case 'A':
		eye_x += movespeed * cos(eye_rh - 90*PI/180);
		eye_z += movespeed * sin(eye_rh - 90*PI/180);
		break;
	case 'd':
	case 'D':
		eye_x += movespeed * cos(eye_rh + 90*PI/180);
		eye_z += movespeed * sin(eye_rh + 90*PI/180);
		break;
	case 's':
	case 'S':
		eye_x += movespeed * cos(eye_rh + 180*PI/180) * fov_step;
		eye_z += movespeed * sin(eye_rh + 180*PI/180) * fov_step;
		break;
	case 'w':
	case 'W':
		eye_x += movespeed * cos(eye_rh + 0*PI/180) * fov_step;
		eye_z += movespeed * sin(eye_rh + 0*PI/180) * fov_step;
		break;
	case 'q':
	case 'Q':
		eye_y += movespeed;
		break;
	case 'e':
	case 'E':
		eye_y += -movespeed;
		break;

	case KEY_LEFT:
		eye_rh -= shift ? 0.02 : 0.15;
		break;
	case KEY_RIGHT:
		eye_rh += shift ? 0.02 : 0.15;
		break;
	case KEY_UP:
		eye_rv -= (shift ? 0.02 : 0.33);
		break;
	case KEY_DOWN:
		eye_rv += (shift ? 0.02 : 0.33);
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
	case KEY_F1:
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
	case KEY_F2:
		printf("playback movie.\n");
		movie_mode = MOVIE_PLAYBACK;
		movie_index = 0;
		break;
	
	case KEY_F3:
		load_config();
		break;

	case 'C':  toggle(mode.auto_clipping); break;
	case 'k':  toggle(mode.sync_enabled);  break;
	case 'm':  toggle(mode.mixed_enabled); break;
	case 'M':  toggle(mode.mirroring); break;
	case 'Z':  toggle(mode.zero255_show);  break;
	case 'O':  toggle(mode.alpha_mode);    break;
 	case 'P':  toggle(mode.pixel_completion); break;
	case 'b':  toggle(mode.borderline);    break;
	case 'B':  toggle(mode.simple_dot_body);  break;

	case '1':  calibration_focus = &dev1.calibration.a;  break;
	case '2':  calibration_focus = &dev1.calibration.b;  break;
	case '3':  calibration_focus = &dev1.calibration.c;  break;
	case '4':  calibration_focus = &dev1.calibration.d;  break;
	case '5':  calibration_focus = &dev2.calibration.a;  break;
	case '6':  calibration_focus = &dev2.calibration.b;  break;
	case '7':  calibration_focus = &dev2.calibration.c;  break;
	case '8':  calibration_focus = &dev2.calibration.d;  break;

	case 'g':
		fovy -= 0.05;
		break;
	case 'h':
		fovy += 0.05;
		break;

	case 'J':
		fov_ratio = 1.0;
		fovy   =    40.0f;
		eye_x  =  0;
		eye_y  =  1.3;
		eye_z  = -4.6;
		eye_rh = PI/2;
		eye_rv = 0.135;
		break;

	case '9':
		calibration_focus = nullptr;
		break;
	case '\t':
		toggle(mode.calibration);
		break;

	case '$':
		toggle(mode.view4test);
		break;
	case 'T':
		clearFloorDepth();
		break;
	case 't':
		dev1.saveFloorDepth();
		break;
	case 13:
		gl::ToggleFullScreen();
		break;
	case ':':
		toggle(debug_bool);
		printf("debug_bool is %s\n", debug_bool ? "true" : "false");
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
	glutInitWindowSize(GL_WIN_SIZE_X, GL_WIN_SIZE_Y);

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
