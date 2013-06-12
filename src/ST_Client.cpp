#ifndef _CRT_SECURE_NO_DEPRECATE 
	#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include "mi/Image.h"
#include "mi/Udp.h"
#include "mi/Libs.h"
#include "FreeType.h"
#include "gl_funcs.h"
#include "file_io.h"
#include "gl_funcs.h"
#include "Config.h"
#include <map>
#pragma warning(disable:4366)
#define GL_GENERATE_MIPMAP_SGIS 0x8191
#include "ST_Client.h"


extern void load_config();


float eye_r = -0.35f;
float eye_d = 4.00f;
float ex,ey,ez;




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

mi::Image pic;
mi::Image background_image;



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

mi::UdpReceiver udp_recv;
mi::UdpSender   udp_send;

const int UDP_SERVER_RECV = 38702;
const int UDP_CLIENT_RECV = 38708;


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

	// @init @image
//	background_image.createFromImageA("C:/ST/Picture/Pretty-Blue-Heart-Design.jpg");
	background_image.createFromImageA("C:/ST/Picture/mountain-04.jpg");

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
//#const uint8* last_depth_image = nullptr;
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


//#
/*
void StClient::BuildDepthImage(uint8* const final_dest)
{
	using namespace openni;


	// Create 'depth_raw' from Kinect
	static uint8 depth_raw[640*480];
	{
		const auto* depth_row = (const DepthPixel*)dev1.depthFrame.getData();
		const int rowsize = dev1.depthFrame.getStrideInBytes() / sizeof(DepthPixel);
		int index = 0;
		uint8* dest = (mode.calibration_index==0) ? depth_raw : final_dest;
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


	if (mode.calibration_index!=0)
	{
		//     x
		//  A-----B          A-_g
		//  |     |         /   \_
		// y|     |  -->  e/      B
		//  |     |       /  h   /f
		//  C-----D      C------D
		const auto& kc = this->
			(mode.calibration_index==1)
				? config.kinect1_calibration
				: config.kinect2_calibration; 
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
*/


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

//#	BuildDepthImage(curr_pre);
//#	last_depth_image = curr_pre;

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
	if (pic.enabled())
	{
		pic.draw(0,0, 640,480, 255);
	}
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

static void commandReloadConfig(Args& arg)
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

static void commandStatus(Args& arg)
{
	arg_check(arg, 0);
	sendStatus();
}

static void commandStart(Args& arg)
{
	arg_check(arg, 0);

	client_status = STATUS_GAME;
	sendStatus();
}

static void commandBorderLine(Args& arg)
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

static void commandPing(Args& arg)
{
	arg_check(arg, 1);

	printf("PING received: server is '%s'\n", arg[0].to_s());

	std::string s;
	s += "PONG ";
	s += Core::getComputerName();
	s += " ";
	s += mi::Udp::getIpAddress();
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
	Lib::splitStringToLines(rawstring, lines);

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
	Lib::splitString(line, cmd, arg);

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


void drawFieldGrid()
{
	glBegin(GL_LINES);
	const float F = 1.2;

	glLineWidth(1.0f);
	glRGBA(200,200,200, 150).glColorUpdate();
	for (float i=0.0f; i<=F; i+=0.1f)
	{
		glVertex3f(-F, 0,  i);
		glVertex3f(+F, 0,  i);
		glVertex3f(-F, 0, -i);
		glVertex3f(+F, 0, -i);

		glVertex3f( i, 0, -F);
		glVertex3f( i, 0, +F);
		glVertex3f(-i, 0, -F);
		glVertex3f(-i, 0, +F);
	}

	// Centre line
	glRGBA(255,255,0).glColorUpdate();
	glLineWidth(5.0f);
	glEnable(GL_LINE_SMOOTH);
	glVertex3f(-5.0f, 0, 0);
	glVertex3f(+5.0f, 0, 0);
	glVertex3f(0, 0, -5.0f);
	glVertex3f(0, 0, +5.0f);
	glDisable(GL_LINE_SMOOTH);

	glLineWidth(1.0f);
	glEnd();
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
	gl::DepthTest(true);
	{
		glOrtho(0, 1, 0, 1, -1.0, 1.0);

		ex = cos(eye_r)*eye_d;
		ez = sin(eye_r)*eye_d;

		::glMatrixMode(GL_PROJECTION);
		::glLoadIdentity();
		::gluPerspective(40.0f, 4.0/3.0, 1.0f, 100.0f);
		::gluLookAt(
			ex,
			0.35,
			ez,
			
			0,
			0.65,
			0,
			0.0, 1.0, 0.0);

		::glMatrixMode(GL_MODELVIEW);
		::glLoadIdentity();


	{
		const float Z = 1.0f;
		gl::Texture(true);
		glPushMatrix();
		gl::LoadIdentity();
		glBindTexture(GL_TEXTURE_2D, background_image.getTexture());
		const float u = background_image.getTextureWidth();
		const float v = background_image.getTextureHeight();
		glBegin(GL_QUADS);
			for (int i=-5; i<=5; ++i)
			{
				glTexCoord2f(0,0); glVertex3f(-1.5f+3*i, 2.6f, Z);
				glTexCoord2f(u,0); glVertex3f( 1.5f+3*i, 2.6f, Z); //左上
				glTexCoord2f(u,v); glVertex3f( 1.5f+3*i, 0.0f, Z); //左下
				glTexCoord2f(0,v); glVertex3f(-1.5f+3*i, 0.0f, Z);
			}
		glEnd();
		glPopMatrix();
		gl::Texture(false);
	}


	drawFieldGrid();
		dev1.CreateRawDepthImage();

#define USE_TRI 0

#if USE_TRI
		glBegin(GL_TRIANGLES);
#else
		glBegin(GL_POINTS);
#endif
			const uint16* data = dev1.raw_depth.image.data();
			dev1.raw_depth.CalcDepthMinMax();
#if USE_TRI
			for (int y=0; y<480-1; ++y)
			{
				for (int x=0; x<640-1; ++x)
				{
					const int base = x + y*640;
					int val1 = data[base];
					int val2 = data[base+1];
					int val3 = data[base+1+640];
					int val4 = data[base+640];

					float z1 = (float)(val1-dev1.raw_depth.min_value)/dev1.raw_depth.range;
					float z2 = (float)(val2-dev1.raw_depth.min_value)/dev1.raw_depth.range;
					float z3 = (float)(val3-dev1.raw_depth.min_value)/dev1.raw_depth.range;
					float z4 = (float)(val4-dev1.raw_depth.min_value)/dev1.raw_depth.range;
					
					if (val1!=0 && val2!=0 && val3!=0)
					{
						// 1-2-3
						glColor4f(z1,z1,z1, z1+0.4);
						glVertex3f(
							 (x)/640.0f - 0.5,
							-(y)/480.0f + 1.0,
							z1 - 0.5);
						glColor4f(z2,z2,z2, z2+0.4);
						glVertex3f(
							 (x+1)/640.0f - 0.5,
							-(y  )/480.0f + 1.0,
							z2 - 0.5);
						glColor4f(z3,z3,z3, z3+0.4);
						glVertex3f(
							 (x+1)/640.0f - 0.5,
							-(y+1)/480.0f + 1.0,
							z3 - 0.5);
					}

					if (val1!=0 && val3!=0 && val4!=0)
					{
						// 1-3-4
						glColor4f(z1,z1,z1, z1+0.4);
						glVertex3f(
							 (x)/640.0f - 0.5,
							-(y)/480.0f + 1.0,
							z1 - 0.5);
						glColor4f(z3,z3,z3, z3+0.4);
						glVertex3f(
							 (x+1)/640.0f - 0.5,
							-(y+1)/480.0f + 1.0,
							z3 - 0.5);
						glColor4f(z4,z4,z4, z4+0.4);
						glVertex3f(
							 (x  )/640.0f - 0.5,
							-(y+1)/480.0f + 1.0,
							z4 - 0.5);
					}
#else
			for (int y=0; y<480; ++y)
			{
				for (int x=0; x<640; ++x)
				{
					int val = *data++;

					int alpha = (val-dev1.raw_depth.min_value)*255/dev1.raw_depth.range;
					if (alpha<0) continue;
					if (alpha>255) alpha=255;

					int c = alpha;
					glRGBA(
						c,
						c,
						c,
						255-alpha).glColorUpdate();
					float z = val/2000.0f;
					glVertex3f(
						x/640.0f-0.5,
						-y/480.0f + 1.0,
						z - 0.5);
#endif
				}
			}
		glEnd();
	}



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
// 		dev1.CreateRawDepthImage();
//		dev2.CreateRawDepthImage();
	}

//	drawKdev(dev1,   0,0, 320,240);
//	drawKdev(dev2, 320,0, 320,240);

	//dev1.RawDepthImageToRgbaTex3D(dev1.raw_depth);






	{
		ModelViewObject mo;
		glRGBA::white.glColorUpdate();
		freetype::print(monospace, 20,440, "RDI: %d,%d  (%.2f,%.2f,%.2f)",
				dev1.raw_depth.min_value,
				dev1.raw_depth.max_value,
				ex,
				ey,
				ez);
	}


//#	drawDepthMode();

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

	if (mode.calibration==0)
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
//#	if (last_depth_image==nullptr)
		//#return;

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

	case KEY_LEFT:
		eye_r -= shift ? 0.02 : 0.15;
		break;
	case KEY_RIGHT:
		eye_r += shift ? 0.02 : 0.15;
		break;
	case KEY_UP:
		eye_d -= shift ? 0.02 : 0.33;
		break;
	case KEY_DOWN:
		eye_d += shift ? 0.02 : 0.33;
		break;

	case 27:
		dev1.depth.stop();
		dev1.color.stop();
		dev1.depth.destroy();
		dev1.color.destroy();
		dev1.device.close();
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
	case 'C':  toggle(mode.auto_clipping); break;
	case 'k':  toggle(mode.sync_enabled);  break;
	case 'm':  toggle(mode.mixed_enabled); break;
	case 'M':  toggle(mode.mirroring); break;
	case 'z':  toggle(mode.zero255_show);  break;
	case 'a':  toggle(mode.alpha_mode);    break;
	case 'e':  toggle(mode.pixel_completion); break;
	case 'b':  toggle(mode.borderline);    break;

	case '1':  calibration_focus = &dev1.calibration.a;  break;
	case '2':  calibration_focus = &dev1.calibration.b;  break;
	case '3':  calibration_focus = &dev1.calibration.c;  break;
	case '4':  calibration_focus = &dev1.calibration.d;  break;
	case '5':  calibration_focus = &dev2.calibration.a;  break;
	case '6':  calibration_focus = &dev2.calibration.b;  break;
	case '7':  calibration_focus = &dev2.calibration.c;  break;
	case '8':  calibration_focus = &dev2.calibration.d;  break;

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
