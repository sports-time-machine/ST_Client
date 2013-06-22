#include "mi/mi.h"
#include "file_io.h"
#include "Config.h"
#include <map>
#pragma warning(disable:4366)
#define GL_GENERATE_MIPMAP_SGIS 0x8191
#include "ST_Client.h"
#include <GL/glfw.h>
#pragma comment(lib,"GLFW_x32.lib")
#pragma warning(disable:4244) // conversion

#define local static
#pragma warning(disable:4996) // unsafe function


using namespace mgl;
using namespace mi;
using namespace stclient;
using namespace vector_and_matrix;

const int FRAMES_PER_SECOND = 30;
const int MAX_TOTAL_SECOND  = 50;
const int MAX_TOTAL_FRAMES  = MAX_TOTAL_SECOND * FRAMES_PER_SECOND;
const int ATARI_INC = 20;


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

local HitData hitdata;
local int flashing = 0;



void Kdev::initRam()
{
	glGenTextures(1, &this->vram_tex);
	glGenTextures(1, &this->vram_floor);
}


struct HitObject
{
	bool enable;
	Point point;
	glRGBA color;
	int hit_id;

	HitObject():
		enable(true)
	{
	}
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








const int INITIAL_WIN_SIZE_X = 1024;
const int INITIAL_WIN_SIZE_Y = 768;
const int TEXTURE_SIZE = 512;

const int MOVIE_MAX_SECS = 50;
const int MOVIE_FPS = 30;
const int MOVIE_MAX_FRAMES = MOVIE_MAX_SECS * MOVIE_FPS;







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
	log(mode.mixed_enabled,    'm', "mixed");
	log(mode.mirroring,        '?', "mirroring");
	log(mode.borderline,       'b', "borderline");
	puts("-----------------------------");
}


typedef std::vector<openni::RGB888Pixel> RgbScreen;
typedef std::map<int,RgbScreen> RgbScreenMovie;

local openni::RGB888Pixel* moviex = nullptr;


void init_hit_objects()
{
	{
		HitObject ho;
		ho.point = Point(27,16);
		ho.color = glRGBA(250, 100, 150);
		ho.hit_id = 1;
		hit_objects.push_back(ho);
	}

	{
		HitObject ho;
		ho.point = Point(5,10);
		ho.color = glRGBA(20, 100, 250);
		ho.hit_id = 2;
		hit_objects.push_back(ho);
	}

	{
		HitObject ho;
		ho.point = Point(5,20);
		ho.color = glRGBA(250, 50, 50);
		ho.hit_id = 3;
		hit_objects.push_back(ho);
	}

	{
		HitObject ho;
		ho.point = Point(30,10);
		ho.color = glRGBA(255, 200, 120);
		ho.hit_id = 4;
		hit_objects.push_back(ho);
	}
}


// @constructor, @init
StClient::StClient(Kdev& dev1_, Kdev& dev2_) :
	dev1(dev1_),
	dev2(dev2_),
	active_camera(CAM_BOTH),
	movie_mode(MOVIE_READY)
{
	eye.view_3d_left();

	// コンフィグデータからのロード
	cal_cam1.curr = config.cam1;
	cal_cam2.curr = config.cam2;

	udp_recv.init(UDP_CLIENT_RECV);
	printf("host: %s\n", Core::getComputerName().c_str());
	printf("ip: %s\n", mi::Udp::getIpAddress().c_str());

	mode.mirroring   = config.mirroring;

	init_hit_objects();
}

StClient::~StClient()
{
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
//	global.background_image.createFromImageA("C:/ST/Picture/mountain-04.jpg");
	global.background_image.createFromImageA("C:/ST/Picture/whity.jpg");
	global.dot_image.createFromImageA("C:/ST/Picture/dot.png");

	return true;
}

static void window_resized(int width, int height)
{
	global.window_w = width;
	global.window_h = height;
	glViewport(0, 0, width, height);
}

bool StClient::run()
{
	int window_w = 0;
	int window_h = 0;
	glfwGetWindowSize(&window_w, &window_h);
	window_resized(window_w, window_h);

	for (;;)
	{
		mi::Timer mi(&time_profile.frame);

		if (!glfwGetWindowParam(GLFW_OPENED))
		{
			break;
		}

		// フレーム開始時にUDPコマンドの処理をする
		while (doCommand())
		{
		}

		{
			int w = 0;
			int h = 0;
			glfwGetWindowSize(&w, &h);
			if (!(w==window_w && h==window_h))
			{
				window_w = w;
				window_h = h;
				window_resized(window_w, window_h);
			}
		}

		this->processKeyInput();
		this->processMouseInput();

		{
			this->displayEnvironment();
			this->display3dSectionPrepare();
			this->display3dSection();
			this->display2dSectionPrepare();
			this->display2dSection();
			this->display2();
		}

		glfwSwapBuffers();
	}
	glfwTerminate();
	return true;
}

void buildBitmap(int tex, RGBA_raw* bitmap, int bw, int bh)
{
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bw, bh, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap);
}

void drawBitmap(int dx, int dy, int dw, int dh, float u1, float v1, float u2, float v2)
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


local uint8 floor_depth[640*480];
local uint8 depth_cook[640*480];
local int floor_depth_count = 0;
local uint16 floor_depth2[640*480];


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



void ChangeCalParamKeys::init()
{
	BYTE kbd[256];
	GetKeyboardState(kbd);

	this->ctrl   = (kbd[VK_CONTROL] & 0x80)!=0;
	this->rot_xy = (kbd['T'] & 0x80)!=0;
	this->rot_z  = (kbd['Y'] & 0x80)!=0;
	this->scale  = (kbd['U'] & 0x80)!=0;
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
			global_config.grid_color(0.40f);
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
	glRGBA::white();
	gl::LoadIdentity();
	glBindTexture(GL_TEXTURE_2D, img.getTexture());
	const float u = img.getTextureWidth();
	const float v = img.getTextureHeight();
	glBegin(GL_QUADS);
	const float SZ = Z/2;
	for (int i=-5; i<=5; ++i)
	{
		glTexCoord2f(0,0); glVertex3f(-SZ+Z*i, Z*1.5, -Z);
		glTexCoord2f(u,0); glVertex3f( SZ+Z*i, Z*1.5, -Z); //左上
		glTexCoord2f(u,v); glVertex3f( SZ+Z*i,  0.0f, -Z); //左下
		glTexCoord2f(0,v); glVertex3f(-SZ+Z*i,  0.0f, -Z);
	}
	glEnd();
	glPopMatrix();
	gl::Texture(false);
}


void MixDepth(Dots& dots, const RawDepthImage& src, const CamParam& cam)
{
	const mat4x4 trans = mat4x4::create(
			cam.rotx, cam.roty, cam.rotz,
			cam.x,    cam.y,    cam.z,
			cam.scale);

	int index = 0;
	for (int y=0; y<480; ++y)
	{
		for (int x=0; x<640; ++x)
		{
			int z = src.image[index++];

			// no depth -- ignore
			if (z==0) continue;

			Point3D p;
			float fx = (320-x)/640.0f;
			float fy = (240-y)/640.0f;
			float fz = z/1000.0f; // milli-meter(mm) to meter(m)

			// -0.5 <= fx <= 0.5
			// -0.5 <= fy <= 0.5
			//  0.0 <= fz <= 10.0  (10m)

			// 四角錐にする
			fx = fx * fz;
			fy = fy * fz;

			// 回転、拡縮、平行移動
			vec4 point = trans * vec4(fx, fy, fz, 1.0f);
			p.x = point[0];
			p.y = point[1];
			p.z = point[2];
			dots.push(p);
		}
	}
}

enum DrawVoxelsStyle
{
	DRAW_VOXELS_NORMAL = 0,
	DRAW_VOXELS_HALF = 1,
	DRAW_VOXELS_QUAD = 2,
};

void drawVoxels(const Dots& dots, glRGBA inner_color, glRGBA outer_color, DrawVoxelsStyle style = DRAW_VOXELS_NORMAL)
{
	// @voxel @dot
	if (style & DRAW_VOXELS_QUAD)
	{
		gl::Texture(true);
		glBindTexture(GL_TEXTURE_2D, global.dot_image);
		glBegin(GL_QUADS);
	}
	else
	{
		gl::Texture(false);
		glPointSize(global_config.person_dot_px);
		glBegin(GL_POINTS);
	}

	const int inc = 
		(style & DRAW_VOXELS_HALF)
			? minmax(config.movie_inc,  16, 256)
			: minmax(config.person_inc, 16, 256);
	const int SIZE16 = dots.size() << 4;

	for (int i16=0; i16<SIZE16; i16+=inc)
	{
		const int i = (i16 >> 4);

		const float x = dots[i].x;
		const float y = dots[i].y;
		const float z = dots[i].z;

		const bool in_x = (x>=-2.0f && x<=+2.0f);
		const bool in_y = (y>=+0.0f && y<=+4.0f);
		const bool in_z = (z>=+0.0f && z<=+3.0f);
		
		float col = z/4;
		if (col<0.25f) col=0.25f;
		if (col>0.90f) col=0.90f;
		col = 1.00f - col;
		const int col255 = (int)(col*220);

		if (in_x && in_y && in_z)
		{
			inner_color.glColorUpdate(col255);
		}
		else
		{
			outer_color.glColorUpdate(col255>>2);
		}

		if (style & DRAW_VOXELS_QUAD)
		{
			const float K = 0.01;
			glTexCoord2f(0,0); glVertex3f(x-K,y-K,-z);
			glTexCoord2f(1,0); glVertex3f(x+K,y-K,-z);
			glTexCoord2f(1,1); glVertex3f(x+K,y+K,-z);
			glTexCoord2f(0,1); glVertex3f(x-K,y+K,-z);
		}
		else
		{
			glVertex3f(x,y,-z);
		}
	}

	glEnd();
}



void StClient::MoviePlayback()
{
	if (curr_movie.total_frames==0)
	{
		movie_mode = MOVIE_READY;
		puts("Movie empty.");
		return;
	}

	if (movie_index >= curr_movie.total_frames)
	{
#if 1
		movie_mode = MOVIE_READY;
		puts("Movie end.");
		return;
#else
		// rewind!
		puts("Rewind!");
		movie_index = 0;
#endif
	}

	auto& mov = curr_movie;
	static Dots dots;
	dots.init();

	// @playback
	Depth10b6b::playback(dev1.raw_depth, dev2.raw_depth, mov.frames[movie_index++]);
	MixDepth(dots, dev1.raw_depth, mov.cam1);
	MixDepth(dots, dev2.raw_depth, mov.cam2);
	drawVoxels(dots,
		global_config.movie1_color,	
		glRGBA(50,50,50),
		DRAW_VOXELS_HALF);
}


void StClient::DrawVoxels(Dots& dots)
{
	const glRGBA color_cam1(80,190,250);
	const glRGBA color_cam2(250,190,80);
	const glRGBA color_other(120,120,120);
	const glRGBA color_outer(120,130,200);
	
	CamParam cam1 = cal_cam1.curr;
	CamParam cam2 = cal_cam2.curr;

	if (active_camera==CAM_BOTH)
	{
		Timer tm(&time_profile.drawing.total);
		{
			Timer tm(&time_profile.drawing.mix1);
			dots.init();
			MixDepth(dots, dev1.raw_depth, cam1);
		}
		{
			Timer tm(&time_profile.drawing.mix2);
			MixDepth(dots, dev2.raw_depth, cam2);
		}
		{
			Timer tm(&time_profile.drawing.drawvoxels);
			drawVoxels(dots, global_config.person_color, color_outer);
		}
	}
	else
	{
		if (active_camera==CAM_A)
		{
			dots.init();
			MixDepth(dots, dev1.raw_depth, cam1);
			drawVoxels(dots, color_cam1, color_outer);
			dots.init();
			MixDepth(dots, dev2.raw_depth, cam2);
			drawVoxels(dots, color_other, color_other);
		}
		else
		{
			dots.init();
			MixDepth(dots, dev1.raw_depth, cam1);
			drawVoxels(dots, color_other, color_other);
			dots.init();
			MixDepth(dots, dev2.raw_depth, cam2);
			drawVoxels(dots, color_cam2, color_outer);
		}
	}
}

void StClient::CreateAtari(const Dots& dots)
{
	Timer tm(&time_profile.atari);
	hitdata.clear();
	for (int i=0; i<dots.size(); i+=ATARI_INC)
	{
		// デプスはGreenのなかだけ
		Point3D p = dots[i];
		if (!(p.z>=0.0f && p.z<=2.0f))
		{
			// ignore: too far, too near
			continue;
		}

		// (x,z)を、大きさ1の正方形におさめる（はみ出すこともある）
		float fx = ((p.x+2.0)/4.0);
		float fy = ((2.8-p.y)/2.8);

		const int x = (int)(fx * HitData::CEL_W);
		const int y = (int)(fy * HitData::CEL_H);

		hitdata.inc(x, y);
	}
}


void StClient::MovieRecord()
{
	if (curr_movie.total_frames >= MAX_TOTAL_FRAMES)
	{
		puts("time over! record stop.");
		movie_mode = MOVIE_READY;
	}
	else
	{
		auto& mov = curr_movie;
#if 0
		VoxelRecorder::record(dot_set, mov.frames[mov.total_frames++]);
#else
		mov.cam1 = cal_cam1.curr;
		mov.cam2 = cal_cam2.curr;
		Depth10b6b::record(dev1.raw_depth, dev2.raw_depth, mov.frames[mov.total_frames++]);
#endif
	}
}


void StClient::displayEnvironment()
{
	mi::Timer tm(&time_profile.environment.total);

	// @fps
	this->fps_counter.update();

	// @display
	glClearColor(
		global_config.ground_color.r / 255.0f,
		global_config.ground_color.g / 255.0f,
		global_config.ground_color.b / 255.0f,
		1.00f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Kinectから情報をもらう
	if (dev1.device.isValid())
	{
		mi::Timer tm(&time_profile.environment.read1);
		dev1.CreateRawDepthImage_Read();
		dev1.CreateRawDepthImage();
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
		mi::Timer tm(&time_profile.environment.read2);
		dev2.CreateRawDepthImage_Read();
		dev2.CreateRawDepthImage();
	}
}

void StClient::display3dSectionPrepare()
{
	// PROJECTION
	gl::Projection();
	gl::LoadIdentity();

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

	// MODEL
	gl::Texture(false);
	gl::DepthTest(true);
	gl::ModelView();
	gl::LoadIdentity();
}

void StClient::display3dSection()
{
	{mi::Timer tm(&time_profile.drawing.wall);
		drawWall();
	}
	{mi::Timer tm(&time_profile.drawing.grid);
		drawFieldGrid(500);
	}


#if 1
	{
		static Dots dots;
		DrawVoxels(dots);
		CreateAtari(dots);
	}
#endif

	if (movie_mode==MOVIE_PLAYBACK)
	{
		MoviePlayback();
	}



#if 0
	// 記録と即時再生のテスト
	{
		Dots dots;
		dots.init();
		MovieData::Frame f;
		VoxelRecorder::record(dots, f);
		
		dots.init();
		VoxelRecorder::playback(dots, f);
		drawVoxels(dots, glRGBA(200,240,255), glRGBA(200,70,30),
			DRAW_VOXELS_NORMAL);
//			DRAW_VOXELS_HALF_AND_QUAD);
	}
#endif


	// 記録と即時再生のテスト
#if 0
	{
		MovieData::Frame f;
		Depth10b6b::record(dev1.raw_depth, dev2.raw_depth, f);
		Depth10b6b::playback(dev1.raw_depth, dev2.raw_depth, f);

		{
			CamParam cam = cal_cam1.curr;
			cam.scale = 2.0f;


			Dots dots;
			dots.init();
			dots.push(Point3D( 0, 0, 0));
			dots.push(Point3D(-1, 0, 0));
			dots.push(Point3D(-2, 0, 0));

			MixDepth(dots, dev1.raw_depth, cam);
			MixDepth(dots, dev2.raw_depth, cam);
			//drawVoxels(dots, glRGBA(200,240,255), glRGBA(200,70,30));
			drawVoxels(dots, glRGBA(255,255,255), glRGBA(200,70,30));
		}
	}
#endif

	if (movie_mode==MOVIE_RECORD)
	{
		MovieRecord();
	}
}

void StClient::display2dSectionPrepare()
{
	gl::Projection();
	gl::LoadIdentity();
	glOrtho(0, 640, 480, 0, -1.0, 1.0);

	gl::Texture(false);
	gl::DepthTest(false);
}

void StClient::display2dSection()
{
#if 0//#no flashing
	if (flashing>0)
	{
		flashing -= 13;
		const int fll = minmax(flashing,0,255);
		glRGBA(255,255,255, fll).glColorUpdate();
		glBegin(GL_QUADS);
			glVertex2i(0,0);
			glVertex2i(640,0);
			glVertex2i(640,480);
			glVertex2i(0,480);
		glEnd();
	}
#endif


	{
		// 当たり判定オブジェクト(hitdata)の描画
		glBegin(GL_QUADS);
		for (int y=0; y<HitData::CEL_H; ++y)
		{
			for (int x=0; x<HitData::CEL_W; ++x)
			{
				int hit = hitdata.get(x,y);
				int p = minmax(hit*ATARI_INC/5, 0, 255);
				int q = 255-p;
				glRGBA(
					(240*p +  50*q)>>8,
					(220*p +  70*q)>>8,
					( 60*p + 110*q)>>8,
					180).glColorUpdate();
				const int S = 5;
				const int V = S-1;
				const int M = 10;
				const int dx = x*S + 640 - M - HitData::CEL_W*S;
				const int dy = y*S +   0 + M;
				glVertex2i(dx,   dy);
				glVertex2i(dx+V, dy);
				glVertex2i(dx+V, dy+V);
				glVertex2i(dx,   dy+V);
			}
		}
		glEnd();
	}
	{
		glBegin(GL_QUADS);
		for (size_t i=0; i<hit_objects.size(); ++i)
		{
			const HitObject& ho = hit_objects[i];
			ho.color.glColorUpdate(ho.enable ? 1.0f : 0.33f);
			const int S = 5;
			const int V = S-1;
			const int M = 10;
			const int dx = ho.point.x*S + 640 - M - HitData::CEL_W*S;
			const int dy = ho.point.y*S +   0 + M;
			glVertex2i(dx-1, dy-1);
			glVertex2i(dx+V, dy-1);
			glVertex2i(dx+V, dy+V);
			glVertex2i(dx-1, dy+V);
		}
		glEnd();
	}

	// 当たり判定
	if (flashing<=0)
	{
		for (size_t i=0; i<hit_objects.size(); ++i)
		{
			HitObject& ho = hit_objects[i];
			if (!ho.enable)
				continue;

			int value = hitdata.get(ho.point.x, ho.point.y);
		
			// 10cm3にNドット以上あったらヒットとする
			if (value>=5)
			{
				printf("HIT!! hit object %d, point (%d,%d)\n",
					i,
					ho.point.x,
					ho.point.y);
				flashing = 200;
				ho.enable = false;
				break;
			}
		}
	}


	switch (global.client_status)
	{
	case STATUS_BLACK:        displayBlackScreen();   break;
	case STATUS_PICTURE:      displayPictureScreen(); break;
	}
}


void StClient::saveAgent(int slot)
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

void StClient::loadAgent(int slot)
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

void StClient::clearFloorDepth()
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
	else if (keys.rot_xy)
	{
		curr.rotx = prev.rotx + my;
		curr.roty = prev.roty + mx;
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


void StClient::do_calibration(float mx, float my)
{
	ChangeCalParamKeys keys;
	keys.init();
	switch (active_camera)
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



void StClient::set_clipboard_text()
{
	std::string s;
	
	for (int i=0; i<2; ++i)
	{
		const auto& cam = (i==0) ? cal_cam1.curr : cal_cam2.curr;
		char buffer[1024];
		sprintf(buffer,
			"global camera%d = [\n"
			"	x:     %+6.3f,\n"
			"	y:     %+6.3f,\n"
			"	z:     %+6.3f,\n"
			"	rotx:  %+6.3f,\n"
			"	roty:  %+6.3f,\n"
			"	rotz:  %+6.3f,\n"
			"	scale: %+6.3f];\n",
				1+i,
				cam.x,
				cam.y,
				cam.z,
				cam.rotx,
				cam.roty,
				cam.rotz,
				cam.scale);
		s += buffer;
	}

	mi::Clipboard::setText(s);
}



static void init_open_gl_params()
{
	glEnable(GL_TEXTURE_2D);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	gl::AlphaBlending(true);
}

bool StClient::initOpenGL(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	glfwInit();
	glfwOpenWindow(
		INITIAL_WIN_SIZE_X,
		INITIAL_WIN_SIZE_Y,
		0, 0, 0,
		0, 0, 0,
		(config.initial_fullscreen ? GLFW_FULLSCREEN : GLFW_WINDOW));
	glfwEnable(GLFW_MOUSE_CURSOR);

	{
		std::string name;
		name += "スポーツタイムマシン クライアント";
		name += " (";
		name += Core::getComputerName();
		name += ")";
		glfwSetWindowTitle(name.c_str());
	}

	init_open_gl_params();
	return true;
}
