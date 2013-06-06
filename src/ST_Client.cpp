#ifndef _CRT_SECURE_NO_DEPRECATE 
	#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include "miCore.h"
#include "miImage.h"
#include "FreeType.h"
#include "gl_funcs.h"
#include "file_io.h"
#include <gl/glut.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#ifdef _M_X64
#pragma comment(lib,"OpenNI2_x64.lib")
#else
#pragma comment(lib,"OpenNI2_x32.lib")
#endif

#include "miUdpReceiver.h"
#include "Config.h"
#include <map>
#include <vector>

struct Global
{
	int window_w;
	int window_h;
} global;



extern void load_config();

static bool is_fullscreen = false;

void toggleFullscreen()
{
	if (is_fullscreen)
	{
		glutReshapeWindow(640,480);
	}
	else
	{
		glutFullScreen();
	}
	is_fullscreen = !is_fullscreen;
}


class gl
{
public:
	static void ModelView();
	static void Projection();
	static void LoadIdentity();

	static void DepthTest(bool state)        { glCapState(GL_DEPTH_TEST, state); }
	static void Texture(bool state)          { glCapState(GL_TEXTURE_2D, state); }

	static void AlphaBlending();
	static void glCapState(int cap, bool state);
};

void gl::glCapState(int cap, bool state)
{
	(state ? glEnable : glDisable)(cap);
}

void gl::AlphaBlending()
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void gl::Projection()
{
	glMatrixMode(GL_PROJECTION);
}

void gl::ModelView()
{
	glMatrixMode(GL_MODELVIEW);
}

void gl::LoadIdentity()
{
	glLoadIdentity();
}

class ModelViewObject
{
public:
	ModelViewObject()
	{
		gl::ModelView();
		glPushMatrix();
		gl::LoadIdentity();
	}
	~ModelViewObject()
	{
		glPopMatrix();
		gl::Projection();
	}
};









inline int minmax(int x, int min, int max)
{
	return (x<min) ? min : (x>max) ? max : x;
}



Point2i* calibration_focus = nullptr;





struct Box
{
	int left,top,right,bottom;
	void set(int a, int b, int c, int d)
	{
		left = a;
		top = b;
		right = c;
		bottom = d;
	}
};

struct Point
{
	int x,y;
	Point() : x(0),y(0) { }
	Point(int a, int b) : x(a),y(b) { }

	bool in(const Box& box) const
	{
		return x>=box.left && y>=box.top && x<=box.right && y<=box.bottom;
	}
};


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



// WINNT.H
#undef STATUS_TIMEOUT


enum ClientStatus
{
	// Kinect Calibration
	STATUS_CALIBRATION,

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

ClientStatus client_status = STATUS_DEPTH;









miImage pic;
miImage clam;
miImage background_image;



static inline uint8 uint8crop(int x)
{
	return (x<0) ? 0 : (x>255) ? 255 : 0;
}


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

#pragma warning(disable:4366)
#define GL_GENERATE_MIPMAP_SGIS 0x8191
#include "ST_Client.h"

#include "XnCppWrapper.h"
#include <GL/glut.h>
#include "OniSampleUtilities.h"

const int GL_WIN_SIZE_X = 640;
const int GL_WIN_SIZE_Y	= 480;
const int TEXTURE_SIZE = 512;

const int MOVIE_MAX_SECS = 50;
const int MOVIE_FPS = 30;
const int MOVIE_MAX_FRAMES = MOVIE_MAX_SECS * MOVIE_FPS;
const int PIXELS_PER_SCREEN = 640*480;

struct Mode
{
	bool show_hit_boxes;
	bool sync_enabled;
	bool mixed_enabled;
	bool zero255_show;
	bool alpha_mode;
	bool pixel_completion;
	bool mirroring;
	bool borderline;
	bool calibration;
} mode;

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
	log(mode.calibration,      '_', "calibration");
	puts("-----------------------------");
}



//#define DEFAULT_DISPLAY_MODE	DISPLAY_MODE_OVERLAY
#define DEFAULT_DISPLAY_MODE	DISPLAY_MODE_DEPTH

#define MIN_NUM_CHUNKS(data_size, chunk_size)	((((data_size)-1) / (chunk_size) + 1))
#define MIN_CHUNKS_SIZE(data_size, chunk_size)	(MIN_NUM_CHUNKS(data_size, chunk_size) * (chunk_size))

SampleViewer* SampleViewer::ms_self = NULL;




typedef std::vector<openni::RGB888Pixel> RgbScreen;
typedef std::map<int,RgbScreen> RgbScreenMovie;






size_t movie_index = 0;

openni::RGB888Pixel* moviex = nullptr;


miUdpReceiver udp_recv;
miUdpSender   udp_send;

const int UDP_SERVER_RECV = 38702;
const int UDP_CLIENT_RECV = 38708;
const int UDP_CLIENT_SEND = 38709;



// @constructor, @init
SampleViewer::SampleViewer(openni::Device& device, openni::VideoStream& depth, openni::VideoStream& color) :
	m_device(device), m_depthStream(depth), m_colorStream(color), m_streams(NULL),
	m_eViewState(DEFAULT_DISPLAY_MODE),
	video_ram(nullptr),
	video_ram2(nullptr)
{
	udp_recv.init(UDP_CLIENT_RECV);
	ms_self = this;
	printf("host: %s\n", Core::getComputerName().c_str());
	printf("ip: %s\n", miUdp::getIpAddress().c_str());


	mode.mirroring = true;
	mode.calibration = true;



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
SampleViewer::~SampleViewer()
{
	delete[] video_ram;
	delete[] video_ram2;

	ms_self = NULL;

	if (m_streams != NULL)
	{
		delete []m_streams;
	}
}

freetype::font_data monospace, serif;



openni::Status SampleViewer::init(int argc, char **argv)
{
#if !WITHOUT_KINECT
	openni::VideoMode depthVideoMode;
	openni::VideoMode colorVideoMode;

	if (m_depthStream.isValid() && m_colorStream.isValid())
	{
		depthVideoMode = m_depthStream.getVideoMode();
		colorVideoMode = m_colorStream.getVideoMode();

		int depthWidth = depthVideoMode.getResolutionX();
		int depthHeight = depthVideoMode.getResolutionY();
		int colorWidth = colorVideoMode.getResolutionX();
		int colorHeight = colorVideoMode.getResolutionY();

		if (depthWidth == colorWidth &&
			depthHeight == colorHeight)
		{
			m_width = depthWidth;
			m_height = depthHeight;
		}
		else
		{
			printf("Error - expect color and depth to be in same resolution: D: %dx%d, C: %dx%d\n",
				depthWidth, depthHeight,
				colorWidth, colorHeight);
			return openni::STATUS_ERROR;
		}
	}
	else if (m_depthStream.isValid())
	{
		depthVideoMode = m_depthStream.getVideoMode();
		m_width = depthVideoMode.getResolutionX();
		m_height = depthVideoMode.getResolutionY();
	}
	else if (m_colorStream.isValid())
	{
		colorVideoMode = m_colorStream.getVideoMode();
		m_width = colorVideoMode.getResolutionX();
		m_height = colorVideoMode.getResolutionY();
	}
	else
	{
		printf("Error - expects at least one of the streams to be valid...\n");
		return openni::STATUS_ERROR;
	}
#else
	m_width = 0;
	m_height = 0;
#endif

	m_streams = new openni::VideoStream*[2];
	m_streams[0] = &m_depthStream;
	m_streams[1] = &m_colorStream;

	// Texture map init
	printf("%d x %d\n", m_width, m_height);
	m_nTexMapX = MIN_CHUNKS_SIZE(m_width, TEXTURE_SIZE);
	m_nTexMapY = MIN_CHUNKS_SIZE(m_height, TEXTURE_SIZE);

	video_ram  = new RGBA_raw[m_nTexMapX * m_nTexMapY];
	video_ram2 = new RGBA_raw[m_nTexMapX * m_nTexMapY];


	if (initOpenGL(argc, argv) != openni::STATUS_OK)
	{
		return openni::STATUS_ERROR;
	}

	glGenTextures(1, &vram_tex);
	glGenTextures(1, &vram_tex2);

	// Init routine @init@
	{
		puts("Init font...");
		const std::string font_folder = "C:/Windows/Fonts/";
		monospace.init(font_folder + "Courbd.ttf", 12);
	//	serif    .init(font_folder + "verdana.ttf", 16);
		puts("Init font...done!");
	}

	// @init @image
	background_image.createFromImageA("C:/ST/Picture/Pretty-Blue-Heart-Design.jpg");

	return openni::STATUS_OK;
}

openni::Status SampleViewer::run()	//Does not return
{
	glutMainLoop();

	return openni::STATUS_OK;
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


void drawBitmapLuminance(
		int tex,
		int dx, int dy, int dw, int dh,
		const uint8* bitmap,
		float u1, float v1, float u2, float v2)
{
	gl::Texture(true);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 640, 480, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, bitmap);

	const int x1 = dx;
	const int y1 = dy;
	const int x2 = dx + dw;
	const int y2 = dy + dh;

	glColor4f(1,1,1,1);
	glBegin(GL_QUADS);
	glTexCoord2f(u1, v1); glVertex2f(x1, y1);
	glTexCoord2f(u2, v1); glVertex2f(x2, y1);
	glTexCoord2f(u2, v2); glVertex2f(x2, y2);
	glTexCoord2f(u1, v2); glVertex2f(x1, y2);
	glEnd();
}



void SampleViewer::drawImageMode()
{
	using namespace openni;

	const RGB888Pixel* source_row = (const RGB888Pixel*)m_colorFrame.getData();
	RGBA_raw* dest_row = video_ram + m_colorFrame.getCropOriginY() * m_nTexMapX;
	const int source_rows = m_colorFrame.getStrideInBytes() / sizeof(RGB888Pixel);

	for (int y=0; y<m_colorFrame.getHeight(); ++y)
	{
		const RGB888Pixel* src = source_row;
		RGBA_raw* dest = dest_row + m_colorFrame.getCropOriginX();

		for (int x=0; x<m_colorFrame.getWidth(); ++x)
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
const uint8* last_depth_image = nullptr;
uint8 floor_depth[640*480];
uint8 depth_cook[640*480];
int floor_depth_count = 0;

void SampleViewer::BuildDepthImage(uint8* const final_dest)
{
	using namespace openni;

	// Create 'depth_raw' from Kinect
	static uint8 depth_raw[640*480];
	{
		const auto* depth_row = (const DepthPixel*)m_depthFrame.getData();
		const int rowsize = m_depthFrame.getStrideInBytes() / sizeof(DepthPixel);
		int index = 0;
		uint8* dest = mode.calibration ? depth_raw : final_dest;
		for (int y=0; y<480; ++y)
		{
			const auto* src = depth_row + (mode.mirroring ? 639 : 0);

			for (int x=0; x<640; ++x, ++index)
			{
				int depth = *src;
				mode.mirroring ? --src : ++src;

				if (depth==0)
				{
					// invalid data (too near, too far)
					dest[index] = 0;
				}
				else
				{
					int NEAR_DIST = minmax(config.near_threshold, 50, 10000);
					int FAR_DIST  = minmax(config.far_threshold, 50, 10000);
					if (NEAR_DIST==FAR_DIST)
					{
						++FAR_DIST;
					}

					depth = (depth-NEAR_DIST)*255/(FAR_DIST-NEAR_DIST);
					if (depth>255)
					{
						// too far
						dest[index] = 0;
					}
					else if (depth <= floor_depth[index])
					{
						dest[index] = 1;
					}
					else 
					{
						//depth = depth - floor_depth[index];
						if (depth>255) depth=255;
						if (depth<2) depth=2;
						dest[index] = 255-depth;
					}
				}
			}

			depth_row += rowsize;
		}
	}


	if (mode.calibration)
	{
		//     x
		//  A-----B          A-_g
		//  |     |         /   \_
		// y|     |  -->  e/      B
		//  |     |       /  h   /f
		//  C-----D      C------D
		const auto& kc = config.kinect_calibration;
		Point2i a = kc.a;
		Point2i b = kc.b;
		Point2i c = kc.c;
		Point2i d = kc.d;
		for (int y=0; y<480; ++y)
		{
			Point2i e(
				a.x*(480-y)/480 + c.x*(y+1)/480,
				a.y*(480-y)/480 + c.y*(y+1)/480);
			Point2i f(
				b.x*(480-y)/480 + d.x*(y+1)/480,
				b.y*(480-y)/480 + d.y*(y+1)/480);
			for (int x=0; x<640; ++x)
			{
				Point2i k(
					e.x*(640-x)/640 + f.x*(x+1)/640,
					e.y*(640-x)/640 + f.y*(x+1)/640);
				final_dest[y*640 + x] = depth_raw[k.y*640 + k.x];
			}
		}
	}
}


static MovieData curr_movie;



void SampleViewer::drawDepthMode()
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

	// Depth raw => Current pre buffer

	BuildDepthImage(curr_pre);
	last_depth_image = curr_pre;

	{
		const uint8* src = curr_pre;
		uint8* dest = curr;
		for (int y=0; y<480; ++y)
		{
			for (int x=0; x<640; ++x)
			{
				uint8 depth = *src++;

				if (mode.borderline && depth>=100 && depth<=240 && depth%2==0)
				{
					depth = 20;
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
			printf("%d bytes (%.1f%%)\n",
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
#if 1
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
#endif
#if 1
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
#endif

#if 1
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
#endif
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
					dest->r = 30;
					dest->g = 50;
					dest->b = 70;
					dest->a = 200;
					break;
				case 1:
					dest->r = 220;
					dest->g = 160;
					dest->b = 100;
					dest->a = 240;
					break;
				case 255:
					dest->r = 80;
					dest->g = 50;
					dest->b = 20;
					dest->a = 200;
					break;
				default:
					if (mode.alpha_mode)
					{
						dest->r = 100;
						dest->g = value;
						dest->b = 255-value;
						dest->a = 255;
					}
					else
					{
						dest->r = value;
						dest->g = value;
						dest->b = value;
						dest->a = 220;
					}
					break;
				}
			}
		}
	}

	// @build
	buildBitmap(vram_tex, video_ram, m_nTexMapX, m_nTexMapY);

	// @draw
	const int draw_x = 0;
	const int draw_w = GL_WIN_SIZE_X;
	drawBitmap(
		draw_x, 0,
		draw_w, GL_WIN_SIZE_Y,
		0.0f,
		0.0f,
		(float)m_width  / m_nTexMapX,
		(float)m_height / m_nTexMapY);
}

void SampleViewer::displayDepthScreen()
{
	static float r;
	r += 0.7;
	clam.drawRotated(170,70, 80,150, r, 80);
}

void SampleViewer::displayBlackScreen()
{
}

void SampleViewer::displayPictureScreen()
{
	if (pic.enabled())
	{
		pic.draw(0,0, 640,480, 255);
	}
}


class VariantType
{
public:
	VariantType(const std::string&);
	int to_i() const  { return intvalue; }
	const char* to_s() const { return strvalue.c_str(); }
	bool is_int() const { return intvalue!=0 || (intvalue==0 && strvalue[0]=='0'); }
	const std::string& string() const { return strvalue; }

private:
	std::string strvalue;
	int intvalue;
};

VariantType::VariantType(const std::string& s)
{
	strvalue = s;
	char* endptr = nullptr;
	intvalue = (int)strtol(s.c_str(), &endptr, 0);
}





void splitStringToLines(const std::string& rawstring, std::vector<std::string>& lines)
{
	lines.clear();

	const char* src = rawstring.c_str();

	while (*src!='\0')
	{
		// blank
		if (*src=='\n' || *src=='\r')
		{
			++src;
			continue;
		}

		std::string line;
		while (*src!='\0' && *src!='\n' && *src!='\r')
		{
			line += *src++;
		}
		lines.push_back(line);
	}
}

bool splitString(const std::string& rawstring, std::string& cmd, std::vector<VariantType>& arg)
{
	cmd.clear();
	arg.clear();

	const char* src = rawstring.c_str();

	auto skip_whitespaces = [&](){
		while (*src!='\0' && isspace(*src))
		{
			++src;
		}
	};

	auto word_copy_to_dest = [&](std::string& dest, bool upper)->bool{
		dest.clear();
		skip_whitespaces();
		if (*src=='\0')
			return false;

		// Create 'cmd'
		while (*src && !isspace(*src))
		{
			dest += upper ? toupper(*src) : *src;
			++src;
		}
		return true;
	};

	if (!word_copy_to_dest(cmd, true))
		return false;

	// Create 'args'
	for (;;)
	{
		std::string temp;
		if (!word_copy_to_dest(temp, false))
			break;
		arg.push_back(temp);
	}
	return true;
}

bool commandIs(const std::string& cmd,
		const char* cmd1,
		const char* cmd2=nullptr,
		const char* cmd3=nullptr)
{
	if (cmd1!=nullptr && cmd.compare(cmd1)==0)
		return true;
	if (cmd2!=nullptr && cmd.compare(cmd2)==0)
		return true;
	if (cmd3!=nullptr && cmd.compare(cmd3)==0)
		return true;
	return false;
}



	enum InvalidFormat
	{
		INVALID_FORMAT,
	};

typedef const std::vector<VariantType> Args;


void arg_check(Args& arg, size_t x)
{
	if (arg.size()!=x)
		throw INVALID_FORMAT;
}




void commandDepth(Args& arg)
{
	arg_check(arg, 0);
	client_status = STATUS_DEPTH;
}

void commandBlack(Args& arg)
{
	arg_check(arg, 0);
	client_status = STATUS_BLACK;
}

void commandMirror(Args& arg)
{
	arg_check(arg, 0);
	toggle(mode.mirroring);
}

void commandDiskInfo(Args& arg)
{
	arg_check(arg, 0);

	static const ULARGE_INTEGER zero = {};
	ULARGE_INTEGER free_bytes;
	ULARGE_INTEGER total_bytes;

	if (GetDiskFreeSpaceEx("C:", &free_bytes, &total_bytes, nullptr)==0)
	{
		free_bytes  = zero;
		total_bytes = zero;
		fprintf(stderr, "(error) GetDiskFreeSpaceEx\n");
	}

	auto mega_bytes = [](ULARGE_INTEGER ul)->uint32{
		const uint64 size = ((uint64)ul.HighPart<<32) | ((uint64)ul.LowPart);
		return (uint32)(size / 1000000);
	};

	uint32 free  = mega_bytes(free_bytes);
	uint32 total = mega_bytes(total_bytes);
	printf("%u MB free(%.1f%%), %u MB total\n",
			free,
			free*100.0f/total,
			total);
}

void commandReloadConfig(Args& arg)
{
	arg_check(arg, 0);
	load_config();
}

void sendStatus()
{
	std::string s;
	s += "STATUS ";
	s += Core::getComputerName();
	s += " ";
	s += (
		(client_status==STATUS_CALIBRATION) ? "CALIBRATION" :
		(client_status==STATUS_BLACK) ? "BLACK" :
		(client_status==STATUS_PICTURE) ? "PICTURE" :
		(client_status==STATUS_IDLE) ? "IDLE" :
		(client_status==STATUS_GAMEREADY) ? "GAMEREADY" :
		(client_status==STATUS_GAME) ? "GAME" :
		(client_status==STATUS_DEPTH) ? "DEPTH" :
		(client_status==STATUS_GAMESTOP) ? "GAMESTOP" :
		(client_status==STATUS_TIMEOUT) ? "TIMEOUT" :
		(client_status==STATUS_GOAL) ? "GOAL" :
			"UNKNOWN-STATUS");
	udp_send.send(s);
}

void commandStatus(Args& arg)
{
	arg_check(arg, 0);
	sendStatus();
}

void commandStart(Args& arg)
{
	arg_check(arg, 0);

	client_status = STATUS_GAME;
	sendStatus();
}

void commandBorderLine(Args& arg)
{
	arg_check(arg, 0);
	toggle(mode.borderline);
}

const char* to_s(int x)
{
	static char to_s_buf[1000];
	_ltoa(x, to_s_buf, 10);
	return to_s_buf;
}

void commandPing(Args& arg)
{
	arg_check(arg, 1);

	printf("PING received: server is '%s'\n", arg[0].to_s());

	std::string s;
	s += "PONG ";
	s += Core::getComputerName();
	s += " ";
	s += miUdp::getIpAddress();
	s += " ";
	s += to_s(config.client_number);

	udp_send.init(arg[0].to_s(), UDP_SERVER_RECV);
	udp_send.send(s);
}

void commandPict(Args& arg)
{
	arg_check(arg, 1);

	std::string path;
	path += "//STMX64/ST/Picture/";
	path += arg[0].to_s();
	if (pic.createFromImageA(path.c_str()))
	{
		client_status = STATUS_PICTURE;
		return;
	}
	else
	{
		printf("picture load error. %s\n", arg[0].to_s());
	}
}


File save_file;




void commandIdent(Args& arg)
{
	arg_check(arg, 2);


	printf("%s, %s\n", arg[0].to_s(), arg[1].to_s());
	
	std::string filename = (arg[0].string() + "-" + arg[1].string());
	if (!save_file.openForWrite(filename.c_str()))
	{
		puts("Open error (savefile)");
		return;
	}

	printf("Filename %s ok\n", filename.c_str());

	client_status = STATUS_GAMEREADY;
	sendStatus();
}

void commandBye(Args& arg)
{
	arg_check(arg, 0);
	exit(0);
}

void commandHitBoxes(Args& arg)
{
	arg_check(arg, 0);
	toggle(mode.show_hit_boxes);
}


bool SampleViewer::doCommand()
{
	std::string rawstring;
	if (udp_recv.receive(rawstring)<=0)
	{
		return false;
	}

	std::vector<std::string> lines;
	splitStringToLines(rawstring, lines);

	for (size_t i=0; i<lines.size(); ++i)
	{
		doCommand2(lines[i]);
	}
	return true;
}

bool SampleViewer::doCommand2(const std::string& line)
{
	std::string cmd;
	std::vector<VariantType> arg;
	splitString(line, cmd, arg);

	// Daemon command
	if (cmd[0]=='#')
	{
		printf("[DAEMON COMMAND] '%s' -- ignore", cmd.c_str());
		return true;
	}


	printf("[UDP COMMAND] '%s' ", cmd.c_str());
	for (size_t i=0; i<arg.size(); ++i)
	{
		if (arg[i].is_int())
		{
			printf("[int:%d]", arg[i].to_i());
		}
		else
		{
			printf("[str:%s]", arg[i].to_s());
		}
	}
	printf("\n");

	const int argc = arg.size();

	try
	{
		// @command
#define COMMAND(CMD, PROC)    if (cmd.compare(CMD)==0) { PROC(arg); return true; }
		COMMAND("HITBOXES", commandHitBoxes);
		COMMAND("RELOADCONFIG", commandReloadConfig);

		COMMAND("DISKINFO", commandDiskInfo);
		COMMAND("MIRROR",   commandMirror);
		COMMAND("BLACK",    commandBlack);
		COMMAND("DEPTH",    commandDepth);
		COMMAND("STATUS",   commandStatus);
		COMMAND("START",    commandStart);

		COMMAND("BORDERLINE", commandBorderLine);
		COMMAND("IDENT",    commandIdent);
		COMMAND("PING",     commandPing);
		COMMAND("PICT",     commandPict);
		COMMAND("BYE",      commandBye);
		COMMAND("QUIT",     commandBye);
		COMMAND("EXIT",     commandBye);
#undef COMMAND

		printf("Invalid udp-command '%s'\n", cmd.c_str());
	}
	catch (InvalidFormat)
	{
		printf("Invalid format '%s' argc=%d\n", cmd.c_str(), argc);
	}

	return false;
}


size_t hit_object_stage = 0;

float eye_rad = -0.43f;

float
	eye_x=-4.69,
	eye_y=+1.65,
	eye_z=+3.68;


void SampleViewer::displayCalibration()
{
	// @calibration @tool
	//glOrtho(-3.0f, +3.0f, 0.0f, 3.0f, 0.0f, 5.0f);
	glOrtho(-1.0f, +1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
	gl::Projection();
	gl::LoadIdentity();
	gl::DepthTest(true);
	gluPerspective(60.0, 4.0/3.0, 1.0, 200.0);
	glTranslatef(0.0f,0.0f,-5.0f);
	gluLookAt(
		eye_x, eye_y, eye_z,
		eye_x + cos(eye_rad)*10,
		0.0,
		eye_z + sin(eye_rad)*10,
		0.0, 1.0, 0.0);	//視点と視線の設定

	gl::ModelView();
	gl::LoadIdentity();
	gl::Texture(false);

	static GLUquadricObj* sphere = gluNewQuadric();
	glRGBA(200,100,100,99).glColorUpdate();
	gluSphere(sphere, 1.0, 10, 10);

	vec cube[]={
		vec(-2,0,0),
		vec( 2,0,0),
		vec( 2,0,4),
		vec(-2,0,4),
		vec(-2,3,0),
		vec( 2,3,0),
		vec( 2,3,4),
		vec(-2,3,4),
	};

	glRGBA(200,100,100).glColorUpdate();
	glBegin(GL_QUADS);
		cube[0].glVertexUpdate();
		cube[1].glVertexUpdate();
		cube[2].glVertexUpdate();
		cube[3].glVertexUpdate();
	glEnd();

	for (int i=0; i<2; ++i)
	{
		const int mod = (i==0) ? GL_QUADS : GL_LINE_LOOP;
		((i==0)
			? glRGBA(100,120,50,53)
			: glRGBA(255,255,255)).glColorUpdate();
		glBegin(mod);
			cube[1].glVertexUpdate();
			cube[2].glVertexUpdate();
			cube[6].glVertexUpdate();
			cube[5].glVertexUpdate();
		glEnd();
		glBegin(mod);
			cube[0].glVertexUpdate();
			cube[3].glVertexUpdate();
			cube[7].glVertexUpdate();
			cube[4].glVertexUpdate();
		glEnd();
		glBegin(mod);
			cube[0].glVertexUpdate();
			cube[1].glVertexUpdate();
			cube[5].glVertexUpdate();
			cube[4].glVertexUpdate();
		glEnd();
		glBegin(mod);
			cube[2].glVertexUpdate();
			cube[3].glVertexUpdate();
			cube[7].glVertexUpdate();
			cube[6].glVertexUpdate();
		glEnd();
	}

	glRGBA(255,255,255).glColorUpdate();
	glBegin(GL_LINE_LOOP);
		cube[4].glVertexUpdate();
		cube[5].glVertexUpdate();
		cube[6].glVertexUpdate();
		cube[7].glVertexUpdate();
	glEnd();

//	glRGBA(25,55,255).glColorUpdate();
//	glRectangle(10, 0, 50, 50);

	glRGBA(64,64,80,128).glColorUpdate();
	glBegin(GL_QUADS);
		glVertex3f(-50, 0, -50);
		glVertex3f( 50, 0, -50);
		glVertex3f( 50, 0,  50);
		glVertex3f(-50, 0,  50);
	glEnd();

	glBegin(GL_LINES);
	for (int i=-50; i<=50; i++)
	{
		((i==0)
			? glRGBA(220,150,50)
			: glRGBA(60,60,60)).glColorUpdate();
		vec(-50.0f, 0, i).glVertexUpdate();
		vec(+50.0f, 0, i).glVertexUpdate();
		((i==0)
			? glRGBA(220,150,50)
			: glRGBA(60,60,60)).glColorUpdate();
		vec(i, 0, -50.0f).glVertexUpdate();
		vec(i, 0, +50.0f).glVertexUpdate();
	}
	glEnd();
}



void SampleViewer::display()
{
	while (doCommand())
	{
	}

	// @display
	glClearColor(0.5, 0.75, 0.25, 1.00);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	gl::Texture(false);
	gl::DepthTest(false);

	glOrtho(0, 640, 480, 0, -1.0, 1.0);

	gl::Projection();
	gl::LoadIdentity();

	{
		background_image.draw(0,0,640,480);
		clam.draw(20,20,100,100);
		m_depthStream.readFrame(&m_depthFrame);
		drawDepthMode();
		glRectangle(10,10,50,50);
	}

	{
		ModelViewObject mo;
		glRGBA::black.glColorUpdate();
		freetype::print(monospace, 20, 30, "hello");
	}


	// キャリブレーション情報が無効（設定中）のみ
	if (!mode.calibration)
	{
		displayCalibrationInfo();
	}

	// Swap the OpenGL display buffers
	glutSwapBuffers();
}


void SampleViewer::displayCalibrationInfo()
{
	gl::Texture(false);
	gl::DepthTest(false);

//	gl::Projection();
//	gl::ModelView();

	glRGBA(0,0,0,128).glColorUpdate();
	glBegin(GL_QUADS);
		Point2f(0,0).glVertex2();
		Point2f(700,0).glVertex2();
		Point2f(640,480).glVertex2();
		Point2f(0,480).glVertex2();
	glEnd();

	const auto& kc = config.kinect_calibration;

	glRGBA::white.glColorUpdate();
	glBegin(GL_LINE_LOOP);
		kc.a.glVertex2();
		kc.b.glVertex2();
		kc.d.glVertex2();
		kc.c.glVertex2();
	glEnd();

	if (calibration_focus!=nullptr)
	{
		glRGBA::white.glColorUpdate();
		glRectangleFill(
			calibration_focus->x-5,
			calibration_focus->y-5,
			10,10);
	}

	{
		ModelViewObject m;
		glRGBA::white.glColorUpdate();
		freetype::print(monospace, 20,  20, "[Calibration Mode]");
		freetype::print(monospace, 20,  40, "A = (%d,%d)", kc.a.x, kc.a.y);
		freetype::print(monospace, 20,  60, "B = (%d,%d)", kc.b.x, kc.b.y);
		freetype::print(monospace, 20,  80, "C = (%d,%d)", kc.c.x, kc.c.y);
		freetype::print(monospace, 20, 100, "D = (%d,%d)", kc.d.x, kc.d.y);
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

void saveFloorDepth()
{
	if (last_depth_image==nullptr)
		return;

	for (int i=0; i<640*480; ++i)
	{
		floor_depth[i] = last_depth_image[i];
	}
}


void SampleViewer::onMouse(int button, int state, int x, int y)
{
	if (button==GLUT_LEFT_BUTTON && state==GLUT_DOWN)
	{
		// Convert screen position to internal position
		x = x * 640 / global.window_w;
		y = y * 480 / global.window_h;

		// 補正なし時のみ設定できる
		if (calibration_focus!=nullptr && !mode.calibration)
		{
			calibration_focus->x = x;
			calibration_focus->y = y;
		}
	}
}


void SampleViewer::onKey(int key, int /*x*/, int /*y*/)
{
	switch (key)
	{
	default:
		printf("[key %d]\n", key);
		break;
	case 1100://left
		eye_rad -= 0.01;
		break;
	case 1102://right
		eye_rad += 0.01;
		break;
	case 1101://up
		eye_x += cos(eye_rad)*0.2;
		eye_z += sin(eye_rad)*0.2;
		break;
	case 1103://down
		eye_x -= cos(eye_rad)*0.2;
		eye_z -= sin(eye_rad)*0.2;
		break;
	case 1104://pageup
		eye_y += 0.5;
		break;
	case 1105://pagedown
		eye_y -= 0.5;
		break;


	case 27:
		m_depthStream.stop();
		m_colorStream.stop();
		m_depthStream.destroy();
		m_colorStream.destroy();
		m_device.close();
		openni::OpenNI::shutdown();
		exit(1);
	case 'w':
		client_status = STATUS_DEPTH;
		break;
	case 'M':
		m_depthStream.setMirroringEnabled(!m_depthStream.getMirroringEnabled());
		m_colorStream.setMirroringEnabled(!m_colorStream.getMirroringEnabled());
		break;
	case 'r':
		curr_movie.frames.clear();
		curr_movie.frames.resize(MOVIE_MAX_FRAMES);
		curr_movie.recorded_tail = 0;
		movie_mode = MOVIE_RECORD;
		movie_index = 0;
		break;
	case 's':
		printf("recoding stop. %d frames recorded.\n", curr_movie.recorded_tail);
		{
			size_t total_bytes = 0;
			for (size_t i=0; i<curr_movie.recorded_tail; ++i)
			{
				total_bytes += curr_movie.frames[i].size();
			}
			printf("total %u Kbytes.\n", total_bytes/1000);
		}
		movie_mode = MOVIE_READY;
		movie_index = 0;
		break;
	
	case KEY_F3:
		load_config();
		break;

	case 'p':
		printf("playback movie.\n");
		movie_mode = MOVIE_PLAYBACK;
		movie_index = 0;
		break;
	case 'k':  toggle(mode.sync_enabled);  break;
	case 'm':  toggle(mode.mixed_enabled); break;
	case 'z':  toggle(mode.zero255_show);  break;
	case 'a':  toggle(mode.alpha_mode);    break;
	case 'e':  toggle(mode.pixel_completion);    break;
	case 'b':  toggle(mode.borderline); break;

	case '1':
		calibration_focus = &config.kinect_calibration.a;
		break;
	case '2':
		calibration_focus = &config.kinect_calibration.b;
		break;
	case '3':
		calibration_focus = &config.kinect_calibration.c;
		break;
	case '4':
		calibration_focus = &config.kinect_calibration.d;
		break;
	case '5':
		calibration_focus = nullptr;
		break;
	case '\t':
		toggle(mode.calibration);
		break;

	case 't':
		saveFloorDepth();
		break;
	case 13:
		toggleFullscreen();
		break;
	}
}

openni::Status SampleViewer::initOpenGL(int argc, char **argv)
{
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
		toggleFullscreen();
	}


	initOpenGLHooks();

	gl::ModelView();

	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_TEXTURE_2D);

	gl::AlphaBlending();

	clam.createFromImageA("C:/ST/Picture/03.jpg");

	return openni::STATUS_OK;
}

void SampleViewer::glutIdle()
{
	glutPostRedisplay();
}
void SampleViewer::glutDisplay()
{
	SampleViewer::ms_self->display();
}
void SampleViewer::glutKeyboard(unsigned char key, int x, int y)
{
	SampleViewer::ms_self->onKey(key, x, y);
}
void SampleViewer::glutKeyboardSpecial(int key, int x, int y)
{
	SampleViewer::ms_self->onKey(key+1000, x, y);
}
void SampleViewer::glutMouse(int button, int state, int x, int y)
{
	SampleViewer::ms_self->onMouse(button, state, x, y);
}
void SampleViewer::glutReshape(int width, int height)
{
	global.window_w = width;
	global.window_h = height;
	glViewport(0, 0, width, height);
}

void SampleViewer::initOpenGLHooks()
{
	glutKeyboardFunc(glutKeyboard);
	glutDisplayFunc(glutDisplay);
	glutIdleFunc(glutIdle);
	glutSpecialFunc(glutKeyboardSpecial);
	glutMouseFunc(glutMouse);
	glutReshapeFunc(glutReshape);
}
