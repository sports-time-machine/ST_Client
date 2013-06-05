#ifndef _CRT_SECURE_NO_DEPRECATE 
	#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#define SHOW_RAW_NEAR_AND_FAR   0

#include "miCore.h"
#include "miImage.h"
#include "FreeType.h"
#include "gl_funcs.h"
#include "file_io.h"

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



extern void load_config();


inline int minmax(int x, int min, int max)
{
	return (x<min) ? min : (x>max) ? max : x;
}







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

	bool in(const Box& box)
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




#if SHOW_RAW_NEAR_AND_FAR
int far_value = 0;
int near_value = 0;
#endif


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
	bool sync_enabled;
	bool mixed_enabled;
	bool zero255_show;
	bool alpha_mode;
	bool pixel_completion;
	bool mirroring;
	bool borderline;
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
	puts("-----------------------------");
}



//#define DEFAULT_DISPLAY_MODE	DISPLAY_MODE_OVERLAY
#define DEFAULT_DISPLAY_MODE	DISPLAY_MODE_DEPTH

#define MIN_NUM_CHUNKS(data_size, chunk_size)	((((data_size)-1) / (chunk_size) + 1))
#define MIN_CHUNKS_SIZE(data_size, chunk_size)	(MIN_NUM_CHUNKS(data_size, chunk_size) * (chunk_size))

SampleViewer* SampleViewer::ms_self = NULL;

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
SampleViewer::SampleViewer(const char* strSampleName, openni::Device& device, openni::VideoStream& depth, openni::VideoStream& color) :
	m_device(device), m_depthStream(depth), m_colorStream(color), m_streams(NULL),
	m_eViewState(DEFAULT_DISPLAY_MODE),
	video_ram(nullptr),
	video_ram2(nullptr)
{
	udp_recv.init(UDP_CLIENT_RECV);
	ms_self = this;
	strncpy(m_strSampleName, strSampleName, ONI_MAX_STR);
	printf("host: %s\n", Core::getComputerName().c_str());
	printf("ip: %s\n", miUdp::getIpAddress().c_str());



	mode.mirroring = true;



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
		monospace.init(font_folder + "Courbd.ttf", 16);
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

	glEnable(GL_TEXTURE_2D);
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
int floor_depth_count = 0;

void SampleViewer::BuildDepthImage(uint8* dest)
{
	using namespace openni;

	const auto* depth_row = (const DepthPixel*)m_depthFrame.getData();
	const int rowsize = m_depthFrame.getStrideInBytes() / sizeof(DepthPixel);
	int index = 0;
	for (int y=0; y<480; ++y)
	{
		const auto* src = depth_row + (mode.mirroring ? 639 : 0);

		for (int x=0; x<640; ++x, ++index)
		{
			int depth = *src;
			mode.mirroring ? --src : ++src;

#if SHOW_RAW_NEAR_AND_FAR
			far_value  = max(far_value, depth);
			near_value = (near_value > depth && depth!=0) ? depth : near_value;
#endif

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

#if 0
				if (y>=50 && y<=80)
				{
					if (x>=20 && x<=20+600)
					{
						depth = (x-20)*250/600;
					}
				}
#endif

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
	m_depthStream.readFrame(&m_depthFrame);

	if (m_depthFrame.isValid())
		drawDepthMode();

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

void sendStatus()
{
	std::string s;
	s += "STATUS ";
	s += Core::getComputerName();
	s += " ";
	s += (
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
	s += to_s(client_config.client_number);

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
#define COMMAND(CMD, PROC)    if (cmd.compare(CMD)==0) { PROC(arg); return true; }

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


void SampleViewer::display()
{
	while (doCommand())
	{
	}


	_CrtCheckMemory();

	// @display


#if 0
	int changedIndex = 0;
	//うごかない?
	m_device.setDepthColorSyncEnabled(mode.sync_enabled);
	//END

	openni::Status rc = openni::OpenNI::waitForAnyStream(m_streams, 2, &changedIndex);
	if (rc != openni::STATUS_OK)
	{
		printf("Wait failed\n");
		return;
	}
#endif

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, GL_WIN_SIZE_X, GL_WIN_SIZE_Y, 0, -1.0, 1.0);

	background_image.draw(0,0,640,480);



#if SHOW_RAW_NEAR_AND_FAR
	near_value = 10000;
	far_value = 0;
#endif


	switch (client_status)
	{
	case STATUS_BLACK:    displayBlackScreen();   break;
	case STATUS_PICTURE:  displayPictureScreen(); break;

	case STATUS_DEPTH:    displayDepthScreen();   break;
	
	case STATUS_GAMEREADY:
	case STATUS_GAME:
		displayDepthScreen();
		break;
	}


	// RED TEXT
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
		glColor3ub(255, 255, 255);
		glPushMatrix();
		glLoadIdentity();
		glRotatef(12*cnt, 0, 0, 1);
		glScalef(1.2f, 1.2f+0.3f*cosf(cnt/5), 1.f);
		glTranslatef(-180.f, 0.f, 0.f);
		int time_diff = (timeGetTime() - time_begin);
				
		freetype::print(monospace, 20, 160, "#%d Near(%dcm) Far(%dcm)",
				client_config.client_number,
				config.near_threshold,
				config.far_threshold);

		// @fps
		freetype::print(monospace, 20, 200, "%d, %d, %.1ffps, %.2ffps, %d, %d",
				frames,
				time_diff,
				1000.0f * frames/time_diff,
				fps_counter.getFps(),
				decomp_time,
				draw_time);

		freetype::print(monospace, 20, 240, "(n=%d,f=%d) raw(n=%d,f=%d) (TP:%d)",
				vody.near_d,
				vody.far_d,
				vody.raw_near_d,
				vody.raw_far_d,
				vody.total_pixels);

		freetype::print(monospace, 20, 280, "(%d,%d,%d,%d)",
				vody.body.top,
				vody.body.bottom,
				vody.body.left,
				vody.body.right);

		freetype::print(monospace, 20, 320, "far(%d,%d,%d,%d)",
				vody.far_box.top,
				vody.far_box.bottom,
				vody.far_box.left,
				vody.far_box.right);

		freetype::print(monospace, 20, 360, "near(%d,%d,%d,%d)",
				vody.near_box.top,
				vody.near_box.bottom,
				vody.near_box.left,
				vody.near_box.right);

		glPopMatrix();
	}

	fps_counter.update();

#if SHOW_RAW_NEAR_AND_FAR
	{
		// @far
		glColor3ub(255, 255, 255);
		glPushMatrix();
		glLoadIdentity();
		freetype::print(monospace, 20, 280, "near-far : %5d-%5d",
			near_value,	
			far_value);
		glPopMatrix();
	}
#endif


	const int VODY_RESO = 20; //pixels per 1 body-cel

	const int VODY_W = 640/VODY_RESO;
	const int VODY_H = 480/VODY_RESO;
	uint8 virtual_body[VODY_W * VODY_H];

	const uint8* const src = last_depth_image;
	const int step = 20;

	enum VodyCel
	{
		VODYCEL_NONE,
		VODYCEL_NEAR,
		VODYCEL_FAR,
		VODYCEL_MEDIUM,
	};


	{
		vody.body.top    = 9999;
		vody.body.bottom = 0;
		vody.body.left   = 9999;
		vody.body.right  = 0;

		int index = 0;
		for (int y=step/2; y<480; y+=step)
		{
			for (int x=step/2; x<640; x+=step)
			{
				int farvalue = src[x + y*640];
				virtual_body[index++] = (farvalue>0 ? VODYCEL_MEDIUM : VODYCEL_NONE);
				if (farvalue>10)
				{
					vody.body.top    = min(vody.body.top,    y-step/2);
					vody.body.bottom = max(vody.body.bottom, y+step/2);
					vody.body.left   = min(vody.body.left,   x-step/2);
					vody.body.right  = max(vody.body.right,  x+step/2);
				}
			}
		}
	}

	// VODYBOXのなかのヒストグラムをとる
	{
		const uint8* src = last_depth_image;
		int histgram[256] = {};
		for (int y=vody.body.top; y<=vody.body.bottom; ++y)
		{
			for (int x=vody.body.left; x<=vody.body.right; ++x)
			{
				++histgram[src[x + y*640]];
			}
		}

		// デプスが存在しているピクセル数
		vody.total_pixels = 0;
		for (int i=1; i<256; ++i)
		{
			vody.total_pixels += histgram[i];
		}

		// 最近最遠の閾値 (5%)
		const int LIMIT = vody.total_pixels*5/100;

		// depth:0が近い方向、depth:255は遠い方向
		vody.far_d = 0;
		vody.near_d = 0;
		vody.raw_far_d = 0;
		vody.raw_near_d = 0;
		{
			int pixels = 0;
			for (int i=1; i<256; ++i)
			{
				if (histgram[i]!=0 && vody.raw_far_d==0)
				{
					vody.raw_far_d = i;
				}
				pixels += histgram[i];
				if (pixels >= LIMIT)
				{
					vody.far_d = i;
					break;
				}
			}
		}

		{
			int pixels = 0;
			for (int i=1; i<256; ++i)
			{
				const int ii = 255-i;
				if (histgram[ii]!=0 && vody.raw_near_d==0)
				{
					vody.raw_near_d = ii;
				}
				pixels += histgram[ii];
				if (pixels >= LIMIT)
				{
					vody.near_d = ii;
					break;
				}
			}
		}
	}

	{
		// near box
		vody.near_box.set(9999,9999,0,0);
		for (int y=vody.body.top; y<=vody.body.bottom; ++y)
		{
			for (int x=vody.body.left; x<=vody.body.right; ++x)
			{
				if (src[x + y*640] >= vody.near_d)
				{
					vody.near_box.top    = min(vody.near_box.top,    y-step/2);
					vody.near_box.bottom = max(vody.near_box.bottom, y+step/2);
					vody.near_box.left   = min(vody.near_box.left,   x-step/2);
					vody.near_box.right  = max(vody.near_box.right,  x+step/2);
				}
			}
		}
	}

	{
		int index = 0;
		for (int y=0; y<VODY_H; ++y)
		{
			for (int x=0; x<VODY_W; ++x)
			{
				int value = virtual_body[index++];
				glRectangleFill(
					(value==VODYCEL_NONE) ? glRGBA(0,0,0,0) :
					(value==VODYCEL_NEAR) ? glRGBA(200,0,0,128) :
					(value==VODYCEL_FAR) ? glRGBA(0,0,200,128) :
					(value==VODYCEL_MEDIUM) ? glRGBA(0,200,0,128)
							: glRGBA(255,0,0,255),
					x*VODY_RESO, y*VODY_RESO,
					VODY_RESO-1,
					VODY_RESO-1);
			}
		}

		if (vody.body.left!=9999)
		{
			glRectangleFill(
				glRGBA(20,240,100, 64),
				vody.body.left,
				vody.body.top,
				vody.body.right  - vody.body.left,
				vody.body.bottom - vody.body.top);
		}

#if 0
		// Draw near-box
		if (vody.near_box.left!=9999)
		{
			glRectangleFill(
				glRGBA(200,20,60, 64),
				vody.near_box.left,
				vody.near_box.top,
				vody.near_box.right  - vody.near_box.left,
				vody.near_box.bottom - vody.near_box.top);
		}
#endif
	}

	if (hit_object_stage < hit_objects.size())
	{
		if (hit_objects[hit_object_stage].point.in(vody.near_box))
		{
			++hit_object_stage;
			printf("hit %d of %d\n", hit_object_stage, hit_objects.size());
		}
	}


	// Swap the OpenGL display buffers
	glutSwapBuffers();
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



void SampleViewer::onKey(int key, int /*x*/, int /*y*/)
{
	switch (key)
	{
	default:
		printf("[key %d]\n", key);
		break;
	case 27:
		m_depthStream.stop();
		m_colorStream.stop();
		m_depthStream.destroy();
		m_colorStream.destroy();
		m_device.close();
		openni::OpenNI::shutdown();
		exit(1);
	case '1':
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

	case '\t':
		saveFloorDepth();
		break;
	}
}

openni::Status SampleViewer::initOpenGL(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(GL_WIN_SIZE_X, GL_WIN_SIZE_Y);
	glutCreateWindow (m_strSampleName);
	// 	glutFullScreen();
	glutSetCursor(GLUT_CURSOR_NONE);

	initOpenGLHooks();

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	clam.createFromImageA("C:/Users/STM/Desktop/clam.jpg");

	return openni::STATUS_OK;
}

void SampleViewer::initOpenGLHooks()
{
	glutKeyboardFunc(glutKeyboard);
	glutDisplayFunc(glutDisplay);
	glutIdleFunc(glutIdle);
	glutSpecialFunc(glutKeyboardSpecial);
}
