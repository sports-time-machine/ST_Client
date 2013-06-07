#ifndef _CRT_SECURE_NO_DEPRECATE 
	#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include "miImage.h"
#include "FreeType.h"
#include "gl_funcs.h"
#include "file_io.h"
#include <GL/glut.h>
#include "miUdpReceiver.h"
#include "Config.h"
#include <map>
#pragma warning(disable:4366)
#define GL_GENERATE_MIPMAP_SGIS 0x8191
#include "ST_Client.h"


/*extern*/ Mode mode;



struct Global
{
	int window_w;
	int window_h;
} global;


extern void load_config();


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
	log(mode.calibration,      '_', "calibration");
	log(mode.view4test,        '$', "view4test");
	puts("-----------------------------");
}

//#define DEFAULT_DISPLAY_MODE	DISPLAY_MODE_OVERLAY
#define DEFAULT_DISPLAY_MODE	DISPLAY_MODE_DEPTH


StClient* StClient::ms_self = nullptr;

typedef std::vector<openni::RGB888Pixel> RgbScreen;
typedef std::map<int,RgbScreen> RgbScreenMovie;

size_t movie_index = 0;

openni::RGB888Pixel* moviex = nullptr;

miUdpReceiver udp_recv;
miUdpSender   udp_send;

const int UDP_SERVER_RECV = 38702;
const int UDP_CLIENT_RECV = 38708;


// @constructor, @init
StClient::StClient(openni::Device& device, openni::VideoStream& depth, openni::VideoStream& color) :
	m_device(device), m_depthStream(depth), m_colorStream(color), m_streams(NULL),
	m_eViewState(DEFAULT_DISPLAY_MODE),
	video_ram(nullptr),
	video_ram2(nullptr)
{
	ms_self = this;

	udp_recv.init(UDP_CLIENT_RECV);
	printf("host: %s\n", Core::getComputerName().c_str());
	printf("ip: %s\n", miUdp::getIpAddress().c_str());

	mode.mirroring   = config.mirroring;
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

StClient::~StClient()
{
	delete[] video_ram;
	delete[] video_ram2;

	ms_self = nullptr;

	if (m_streams!=nullptr)
	{
		delete[] m_streams;
	}
}


freetype::font_data monospace;





bool StClient::init(int argc, char **argv)
{
#if !WITHOUT_KINECT
	openni::VideoMode depthVideoMode;
	openni::VideoMode colorVideoMode;

	if (m_depthStream.isValid() && m_colorStream.isValid())
	{
		depthVideoMode = m_depthStream.getVideoMode();
		colorVideoMode = m_colorStream.getVideoMode();

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
	else if (m_depthStream.isValid())
	{
		depthVideoMode = m_depthStream.getVideoMode();
		m_width  = depthVideoMode.getResolutionX();
		m_height = depthVideoMode.getResolutionY();
	}
	else if (m_colorStream.isValid())
	{
		colorVideoMode = m_colorStream.getVideoMode();
		m_width  = colorVideoMode.getResolutionX();
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

	if (!initOpenGL(argc, argv))
	{
		return false;
	}

	glGenTextures(1, &vram_tex);
	glGenTextures(1, &vram_tex2);
	glGenTextures(1, &vram_floor);

	img_rawdepth.create(640,480);
	img_floor   .create(640,480);
	img_cooked  .create(640,480);
	img_transformed.create(640,480);

	video_ram     = new RGBA_raw[m_nTexMapX * m_nTexMapY];
	video_ram2    = new RGBA_raw[m_nTexMapX * m_nTexMapY];


	// Init routine @init
	{
		puts("Init font...");
		const std::string font_folder = "C:/Windows/Fonts/";
		monospace.init(font_folder + "Courbd.ttf", 12);
		puts("Init font...done!");
	}

	// @init @image
	background_image.createFromImageA("C:/ST/Picture/Pretty-Blue-Heart-Design.jpg");

	return true;
}

bool StClient::run()	//Does not return
{
	glutMainLoop();
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
uint16 floor_depth2[640*480];





void StClient::RawDepthImageToRgbaTex(const RawDepthImage& raw, RgbaTex& dimg)
{
	const uint16* src = raw.image.data();
	const int range = max(1, raw.range);
	for (int y=0; y<480; ++y)
	{
		RGBA_raw* dest = dimg.vram + y*dimg.pitch;
		for (int x=0; x<640; ++x, ++dest, ++src)
		{
			const uint16 d = *src;
			const uint v = 255 * (d - raw.min_value) / range;
			if (v==0)
			{
				dest->set(80,50,0,100);
			}
			else if (v>255)
			{
				dest->set(255,50,0,100);
			}
			else
			{
				dest->set(
					(255-v)*256>>8,
					(255-v)*230>>8,
					(255-v)*50>>8,
					255);
			}
		}
	}
}


void DrawRgbaTex(const RgbaTex& img, int dx, int dy, int dw, int dh)
{
	gl::Texture(true);
	glBindTexture(GL_TEXTURE_2D, img.tex);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		img.ram_width, img.ram_height,
		0, GL_RGBA, GL_UNSIGNED_BYTE, img.vram);

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


void StClient::BuildDepthImage(uint8* const final_dest)
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
					else if (floor_depth[index] &&
							depth >= floor_depth[index]-25 &&
							depth <= floor_depth[index]+25)
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
			drawPlaybackMovie();
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
	buildBitmap(vram_tex, video_ram, m_nTexMapX, m_nTexMapY);

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


bool StClient::doCommand()
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

bool StClient::doCommand2(const std::string& line)
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
	glRGBA(0,0,0).glColorUpdate();
	glPushMatrix();
	glLoadIdentity();
	int time_diff = (timeGetTime() - time_begin);

	freetype::print(monospace, 20, 160, "#%d Near(%dmm) Far(%dmm)",
			config.client_number,
			config.near_threshold,
			config.far_threshold);

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





size_t hit_object_stage = 0;
void StClient::display()
{
	while (doCommand())
	{
	}

	// @display
	glClearColor(0.25, 0.50, 0.15, 1.00);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	gl::Texture(false);
	gl::DepthTest(false);
	glOrtho(0, 640, 480, 0, -1.0, 1.0);

	gl::Projection();
	gl::LoadIdentity();

	switch (client_status)
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

	// Mode 1: Raw depth
	{
 		CreateRawDepthImage(raw_depth);
		if (mode.view4test){
			CalcDepthMinMax(raw_depth);
			RawDepthImageToRgbaTex(raw_depth, img_rawdepth);
			DrawRgbaTex(img_rawdepth, 0,0, 320,240);
		}
	}

	// Mode 2: Floor
	{
		if (mode.view4test){
			DrawRgbaTex(img_floor, 320,0, 320,240);
		}
	}

	// Mode 3: Floor filtered depth (cooked depth)
	{
		CreateCoockedDepth(raw_cooked, raw_depth, raw_floor);
		if (mode.view4test){
			RawDepthImageToRgbaTex(raw_cooked, img_cooked);
			DrawRgbaTex(img_cooked, 0,240, 320,240);
		}
	}

	// Mode 4: Transformed depth
	{
		CreateTransformed(raw_transformed, raw_cooked);
		RawDepthImageToRgbaTex(raw_transformed, img_transformed);
		if (mode.view4test){
			DrawRgbaTex(img_transformed, 320,240, 320,240);
		}else{
			DrawRgbaTex(img_transformed, 0,0,640,480);
		}
	}




	{
		ModelViewObject mo;
		glRGBA::white.glColorUpdate();
		freetype::print(monospace, 20,80, "RDI: %d,%d", raw_depth.min_value, raw_depth.max_value);
	}


//	drawDepthMode();

	glRGBA::white.glColorUpdate();

	{
		ModelViewObject mo;
		display2();
	}

	{
		ModelViewObject mo;
		glRGBA::white.glColorUpdate();
		freetype::print(monospace,  20,  20, "1. Raw depth view");
		freetype::print(monospace, 340,  20, "2. Flooer filtered view");
		freetype::print(monospace,  20, 260, "3. Flooer view");
		freetype::print(monospace, 340, 260, "4. Transformed view");
	}

	glRGBA(255,255,255,100).glColorUpdate();
	gl::Line2D(Point2i(0, 40), Point2i(640, 40));
	gl::Line2D(Point2i(0,240), Point2i(640,240));
	gl::Line2D(Point2i(0,440), Point2i(640,440));
	gl::Line2D(Point2i(320,0), Point2i(320,480));

	{
		ModelViewObject mo;
		glRGBA(255,255,255).glColorUpdate();
		freetype::print(monospace, 580,  40, "2400mm");
		freetype::print(monospace, 580, 240, "1200mm");
		freetype::print(monospace, 580, 440, "   0mm");
		freetype::print(monospace, 320,  10, "2m");
	}

	if (!mode.calibration)
	{
		displayCalibrationInfo();
	}

	glutSwapBuffers();
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
		gl::RectangleFill(
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

void StClient::saveFloorDepth()
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
	if (last_depth_image==nullptr)
		return;

	for (int i=0; i<640*480; ++i)
	{
		floor_depth[i] = 0;
	}
}


void StClient::onMouse(int button, int state, int x, int y)
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


void StClient::onKey(int key, int /*x*/, int /*y*/)
{
	BYTE kbd[256]={};
	GetKeyboardState(kbd);

	const bool shift = (kbd[VK_SHIFT] & 0x80)!=0;
	switch (key)
	{
	default:
		printf("[key %d]\n", key);
		break;

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

	case '$':
		toggle(mode.view4test);
		break;
	case 'T':
		clearFloorDepth();
		break;
	case 't':
		saveFloorDepth();
		break;
	case 13:
		gl::ToggleFullScreen();
		break;
	}
}

bool StClient::initOpenGL(int argc, char **argv)
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
		gl::ToggleFullScreen();
	}


	glutKeyboardFunc(glutKeyboard);
	glutDisplayFunc(glutDisplay);
	glutIdleFunc(glutIdle);
	glutSpecialFunc(glutKeyboardSpecial);
	glutMouseFunc(glutMouse);
	glutReshapeFunc(glutReshape);
	glEnable(GL_TEXTURE_2D);

	gl::ModelView();

	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	gl::AlphaBlending();
	return true;
}

void StClient::glutIdle()
{
	glutPostRedisplay();
}
void StClient::glutDisplay()
{
	StClient::ms_self->display();
}
void StClient::glutKeyboard(unsigned char key, int x, int y)
{
	StClient::ms_self->onKey(key, x, y);
}
void StClient::glutKeyboardSpecial(int key, int x, int y)
{
	StClient::ms_self->onKey(key+1000, x, y);
}
void StClient::glutMouse(int button, int state, int x, int y)
{
	StClient::ms_self->onMouse(button, state, x, y);
}
void StClient::glutReshape(int width, int height)
{
	global.window_w = width;
	global.window_h = height;
	glViewport(0, 0, width, height);
}
