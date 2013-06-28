#include "mi/mi.h"
#include "file_io.h"
#include "Config.h"
#pragma warning(disable:4366)
#define GL_GENERATE_MIPMAP_SGIS 0x8191
#include "StClient.h"
#include <GL/glfw.h>
#pragma comment(lib,"GLFW_x32.lib")
#pragma warning(disable:4244) // conversion

#define local static
#pragma warning(disable:4996) // unsafe function


using namespace mgl;
using namespace mi;
using namespace stclient;
using namespace vector_and_matrix;

//const int FRAMES_PER_SECOND = 30;
//const int MAX_TOTAL_SECOND  = 50;
//const int MAX_TOTAL_FRAMES  = MAX_TOTAL_SECOND * FRAMES_PER_SECOND;



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








const int TEXTURE_SIZE = 512;



static void _Msg(int color, const string& s, const string& param)
{
	Console::printf(color, "%s - %s\n", s.c_str(), param.c_str());
}

void Msg::Notice       (const string& s)                       { Console::puts(CON_CYAN,  s); }
void Msg::SystemMessage(const string& s)                       { Console::puts(CON_GREEN, s); }
void Msg::ErrorMessage (const string& s)                       { Console::puts(CON_RED,   s); }
void Msg::Notice       (const string& s, const string& param)  { _Msg(CON_CYAN,  s, param); }
void Msg::SystemMessage(const string& s, const string& param)  { _Msg(CON_GREEN, s, param); }
void Msg::ErrorMessage (const string& s, const string& param)  { _Msg(CON_RED,   s, param); }



void stclient::myGetKeyboardState(BYTE* kbd)
{
	if (glfwGetWindowParam(GLFW_ACTIVE))
	{
		GetKeyboardState(kbd);
	}
	else
	{
		memset(kbd, 0, 256);
	}
}


typedef std::vector<openni::RGB888Pixel> RgbScreen;
typedef std::map<int,RgbScreen> RgbScreenMovie;

local openni::RGB888Pixel* moviex = nullptr;


// @constructor, @init
StClient::StClient(Kdev& dev1_, Kdev& dev2_) :
	dev1(dev1_),
	dev2(dev2_),
	active_camera(CAM_BOTH),
	snapshot_life(0),
	flashing(0),
	_private_client_status(STATUS_SLEEP)
{
	eye.view_2d_run();

	// コンフィグデータからのロード
	cal_cam1.curr = config.cam1;
	cal_cam2.curr = config.cam2;

	udp_recv.init(UDP_CONTROLLER_TO_CLIENT);
	printf("host: %s\n", Core::getComputerName().c_str());
	printf("ip: %s\n", mi::Udp::getIpAddress().c_str());

	mode.mirroring   = config.mirroring;
}

StClient::~StClient()
{
}


bool StClient::init()
{
	Msg::SystemMessage("Init StClient");

#if 0//#
	if (config.enable_kinect)
	{
		openni::VideoMode depthVideoMode;
		openni::VideoMode colorVideoMode;

		depthVideoMode = dev1.depth.getVideoMode();
		colorVideoMode = dev1.color.getVideoMode();
		const int depth_w = depthVideoMode.getResolutionX();
		const int depth_h = depthVideoMode.getResolutionY();
		const int color_w = colorVideoMode.getResolutionX();
		const int color_h = colorVideoMode.getResolutionY();
		printf("Kinect Input: depth[%dx%d], color[%dx%d]\n", depth_w, depth_h, color_w, color_h);
	}
#endif

	if (!initGraphics())
	{
		return false;
	}

	dev1.initRam();
	dev2.initRam();

	// Init routine @init
	{
		puts("Init font...");
		const string font_folder = "C:/Windows/Fonts/";
		monospace.init(font_folder + "Cour.ttf", 12);

		// Consolas
		//monospace.init(font_folder + "trebuc.ttf", 10);
		puts("Init font...done!");
	}

	reloadResources();

	Msg::SystemMessage("Init done!");
	Console::nl();

	return true;
}

void StClient::reloadResources()
{
	// @init @image @png @jpg
#define LOAD_IMAGE(NAME) global.images.NAME.createFromImageA(config.images.NAME)
	LOAD_IMAGE(idle);
	LOAD_IMAGE(background);
	LOAD_IMAGE(sleep);
	LOAD_IMAGE(dot);
	for (int i=0; i<MAX_PICT_NUMBER; ++i)
	{
		LOAD_IMAGE(pic[i]);
	}
}

static void window_resized(int width, int height)
{
	global.window_w = width;
	global.window_h = height;
	glViewport(0, 0, width, height);
}


void StClient::changeStatus(ClientStatus new_status)
{
	// ステータスチェンジとともにフレーム数もゼロにする
	Msg::Notice("Change status to", getStatusName(new_status));
	global.frame_index = 0;
	this->_private_client_status = new_status;

	string s;
	s += "STATUS ";
	s += Core::getComputerName();
	s += " ";
	s += getStatusName(new_status);
	this->udp_send.send(s);
}

const char* StClient::getStatusName(int present) const
{
	const auto st = (present==-1) ? clientStatus() : (ClientStatus)present;
	switch (st)
	{
	case STATUS_SLEEP:       return "SLEEP";
	case STATUS_IDLE:        return "IDLE";
	case STATUS_PICT:        return "PICT";
	case STATUS_BLACK:       return "BLACK";
	case STATUS_READY:       return "READY";
	case STATUS_GAME:        return "GAME";
	case STATUS_REPLAY:      return "REPLAY";
	case STATUS_SAVING:      return "SAVING";
	case STATUS_LOADING:     return "LOADING";
	case STATUS_INIT_FLOOR:  return "INIT-FLOOR";
	}
	return "UNKNOWN-STATUS";
}

bool StClient::recordingNow() const
{
	return clientStatus()==STATUS_GAME;
}

bool StClient::replayingNow() const
{
	return clientStatus()==STATUS_REPLAY;
}

void StClient::recordingStart()
{
	Msg::SystemMessage("Recording start!");
	changeStatus(STATUS_GAME);
	global.gameinfo.movie.clearMovie();
}

void StClient::recordingStop()
{
	Msg::SystemMessage("Recording stop!");
	changeStatus(STATUS_READY);
}

void StClient::recordingReplay()
{
	Msg::SystemMessage("Replay!");
	changeStatus(STATUS_REPLAY);
}


void CreateHitWall(float meter, int id, const char* text)
{
	float point = meter - config.getScreenLeftMeter();
	if (point<0.0f || point>=GROUND_WIDTH)
	{
		// 画面外 -- ignore!
	}
	else
	{
		Console::printf(CON_YELLOW, "Create hit wall %.1fm '%s'\n", meter, text);
		const int x = (int)(HitData::CEL_W * point / GROUND_WIDTH);
		for (int xdiff=-1; xdiff<=+1; ++xdiff)
		{
			for (int i=0; i<HitData::CEL_H; ++i)
			{
				HitObject ho;
				ho.point   = Point(x + xdiff, i);
				ho.color   = glRGBA(255, 200, 120);
				ho.next_id = id;
				ho.text    = text;
				global.hit_objects.push_back(ho);
			}
		}
	}
}



// @gcls
static void glClearGraphics(int r, int g, int b)
{
	glClearColor(
		r / 255.0f,
		g / 255.0f,
		b / 255.0f,
		1.00f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


// 1フレで実行する内容
void StClient::processOneFrame()
{
	// フレーム開始時にUDPコマンドの処理をする
	this->processUdpCommands();

	// Nフレでのスナップショット
	if (config.auto_snapshot_interval>0)
	{
		static int frame_count;
		++frame_count;
		if ((frame_count % config.auto_snapshot_interval)==0)
		{
			this->createSnapshot();
		}
	}

	// カメラ移動の適用
	eye.updateCameraMove();

	this->processKeyInput();
	this->processMouseInput();

	// キネクト情報はつねにもらっておく
	this->displayEnvironment();

	if (global.calibration.enabled)
	{
		// スーパーモードとしてのキャリブレーションモード

		glClearGraphics(255,255,255);
		this->display3dSectionPrepare();
		{
			mi::Timer tm(&time_profile.drawing.grid);
			this->drawFieldGrid(500);
		}
		this->display3dSection();
	}
	else
	{
		// クライアントステータスによる描画の分岐
		// VRAMのクリア(glClearGraphics)はそれぞれに行う
		switch (clientStatus())
		{
		case STATUS_SLEEP:
			// 2Dアイドル画像
			glClearGraphics(255,255,255);
			this->display2dSectionPrepare();
			global.images.sleep.draw(0,0,640,480);
			break;

		case STATUS_BLACK:
			glClearGraphics(0,0,0);
			break;

		case STATUS_IDLE:{
			// 2Dアイドル画像
			glClearGraphics(255,255,255);
			this->display2dSectionPrepare();
			global.images.idle.draw(0,0,640,480);

			// 実映像の表示
			this->display3dSectionPrepare();
			this->display3dSection();
			static Dots dots;
			DrawRealMovie(dots);
			break;}

		case STATUS_PICT:{
			// 画像
			puts("pict");
			glClearGraphics(255,255,255);
			this->display2dSectionPrepare();
			global.images.pic[global.picture_number].draw(0,0,640,480);
			break;}

		case STATUS_INIT_FLOOR:{
			// 実映像の表示
			glClearGraphics(255,255,255);
			this->display3dSectionPrepare();
			this->display3dSection();
			static Dots dots;
			DrawRealMovie(dots);
		
			// 床消しのアップデート
			dev1.updateFloorDepth();
			dev2.updateFloorDepth();

			// テキスト
			this->display2dSectionPrepare();
			this->display2dSection();
			glRGBA::black();
			freetype::print(monospace, 100, 100, "Initializing floor image...");
			break;}

		default:
			glClearGraphics(
				config.color.ground.r,
				config.color.ground.g,
				config.color.ground.b);
			this->display2dSectionPrepare();
			{
				mi::Timer tm(&time_profile.drawing.wall);
				this->draw2dWall();
			}
			this->display3dSectionPrepare();
			{
				mi::Timer tm(&time_profile.drawing.grid);
				this->drawFieldGrid(500);
			}
			this->display3dSection();
			this->display2dSectionPrepare();
			this->display2dSection();
			if (global.color_overlay.a>0)
			{
				global.color_overlay();
				glBegin(GL_QUADS);
				glVertex2i(0,0);
				glVertex2i(640,0);
				glVertex2i(640,480);
				glVertex2i(0,480);
				glEnd();
			}
			break;
		}
	}

	if (global.show_debug_info)
	{
		this->display2dSectionPrepare();
		this->displayDebugInfo();
	}

	glfwSwapBuffers();

	// デバッグ用のフレームオートインクリメント
	if (global.frame_auto_increment)
	{
		++global.frame_index;
	}
}

void StClient::initGameInfo()
{
	startMovieRecordSettings();
//######	global.gameinfo.init();

	// 最初の当たり判定を作る
	const int FIRST_HIT_NUMBER = 0;
	global.hit_stage = FIRST_HIT_NUMBER;
	global.hit_objects.clear();
	global.on_hit_setup(global.hit_stage);
}

bool StClient::run()
{
	global.pslvm.addFunction("CreateHitWall", CreateHitWall);

	int window_w = 0;
	int window_h = 0;
	glfwGetWindowSize(&window_w, &window_h);
	window_resized(window_w, window_h);

	// @main @loop
	for (;;)
	{
		mi::Timer mi(&time_profile.frame);
		glfwPollEvents();

		if (!glfwGetWindowParam(GLFW_OPENED))
		{
			// WM_CLOSE
			break;
		}
		
		if (glfwGetWindowParam(GLFW_ICONIFIED))
		{
			continue;
		}

		// Detect window resize
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

		this->processOneFrame();
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
	if (global.picture_number>=1 && global.picture_number<=MAX_PICT_NUMBER)
	{
		mi::Image& img = global.images.pic[global.picture_number-1];
		img.draw(0,0, 640,480, 255);
	}
}

void ChangeCalParamKeys::init()
{
	BYTE kbd[256]={};
	myGetKeyboardState(kbd);

	this->ctrl   = (kbd[VK_CONTROL] & 0x80)!=0;
	this->rot_xy = (kbd['T'] & 0x80)!=0;
	this->rot_z  = (kbd['Y'] & 0x80)!=0;
	this->scale  = (kbd['U'] & 0x80)!=0;
}




void StClient::drawFieldGrid(int size_cm)
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
			config.color.grid(0.40f);
		}

		glVertex3f(-F, 0, f);
		glVertex3f(+F, 0, f);

		glVertex3f( f, 0, -F);
		glVertex3f( f, 0, +F);
	}

	glEnd();

	const float BOX_WIDTH  = GROUND_WIDTH;
	const float BOX_HEIGHT = GROUND_HEIGHT;
	const float BOX_DEPTH  = GROUND_DEPTH;
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


// 平面に壁を描画する @wall
void StClient::draw2dWall()
{
	auto& img = global.images.background;

	img.draw(0,0,640,480);
	glColor3f(255,0,0);
	glBegin(GL_QUADS);
	glVertex2f(0,0);
	glVertex2f(20,0);
	glVertex2f(20,480);
	glVertex2f(0,480);
	glEnd();
}

// 三次元上に壁を描画する @wall
#if 0//# OBSOLETE CODE: void StClient::draw3dWall()
void StClient::draw3dWall()
{
	auto& img = global.images.background;

	const float Z = !;
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
#endif


void stclient::MixDepth(Dots& dots, const RawDepthImage& src, const CamParam& cam)
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

void stclient::drawVoxels(const Dots& dots, glRGBA inner_color, glRGBA outer_color, DrawVoxelsStyle style, float add_z)
{
	// @voxel @dot
	if (style & DRAW_VOXELS_QUAD)
	{
		gl::Texture(true);
		glBindTexture(GL_TEXTURE_2D, global.images.dot);
		glBegin(GL_QUADS);
	}
	else
	{
		gl::Texture(false);
		glPointSize(config.person_dot_px);
		glBegin(GL_POINTS);
	}

	const int inc = 
		(style & DRAW_VOXELS_HALF)
			? mi::minmax(config.movie_inc,  MIN_VOXEL_INC, MAX_VOXEL_INC)
			: mi::minmax(config.person_inc, MIN_VOXEL_INC, MAX_VOXEL_INC);
	const int SIZE16 = dots.size() << 4;

	for (int i16=0; i16<SIZE16; i16+=inc)
	{
		const int i = (i16 >> 4);

		const float x = dots[i].x;
		const float y = dots[i].y;
		const float z = dots[i].z + add_z;

		const bool in_x = (x>=GROUND_LEFT && x<=GROUND_RIGHT);
		const bool in_y = (y>=0.0f && y<=GROUND_HEIGHT);
		const bool in_z = (z>=0.0f && z<=GROUND_DEPTH);
		
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
			const float K = 0.01f;
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

// ゲーム情報の破棄、初期化
void GameInfo::init()
{
	movie.clearAll();
	partner1.clearAll();
	partner2.clearAll();
	partner3.clearAll();
	movie.frames.clear();
	movie.cam1 = CamParam();
	movie.cam2 = CamParam();
}

void StClient::createSnapshot()
{
	this->snapshot_life = config.snapshot_life_frames;
	dev1.raw_snapshot = dev1.raw_cooked;
	dev2.raw_snapshot = dev2.raw_cooked;
	puts("Create snapshot!");
}

void StClient::DrawRealMovie(Dots& dots)
{
	const glRGBA color_body = global.gameinfo.movie.player_color_rgba;
	const glRGBA color_cam1(80,190,250);
	const glRGBA color_cam2(250,190,80);
	const glRGBA color_other(170,170,170);
	const glRGBA color_outer(120,130,200);
	
	CamParam cam1 = cal_cam1.curr;
	CamParam cam2 = cal_cam2.curr;

	RawDepthImage& image1 = dev1.raw_cooked;
	RawDepthImage& image2 = dev2.raw_cooked;

	dev1.CreateCookedImage();
	dev2.CreateCookedImage();

	if (this->snapshot_life>0)
	{
		--snapshot_life;

		if (snapshot_life>0)
		{
			dots.init();
			MixDepth(dots, dev1.raw_snapshot, cam1);
			MixDepth(dots, dev2.raw_snapshot, cam2);

			glRGBA color = config.color.snapshot;
			color.a = color.a * snapshot_life / config.snapshot_life_frames;
			drawVoxels(dots, color, color_outer, DRAW_VOXELS_NORMAL, +0.5f);
		}
	}


	if (active_camera==CAM_BOTH)
	{
		Timer tm(&time_profile.drawing.total);
		{
			Timer tm(&time_profile.drawing.mix1);
			dots.init();
			MixDepth(dots, image1, cam1);
		}
		{
			Timer tm(&time_profile.drawing.mix2);
			MixDepth(dots, image2, cam2);
		}

		float avg_x = 0.0f;
		float avg_y = 0.0f;
		float avg_z = 0.0f;
		int count = 0;
		for (int i=0; i<dots.size(); ++i)
		{
			Point3D p = dots[i];
			if (p.x>=ATARI_LEFT && p.x<=ATARI_RIGHT && p.y>=ATARI_BOTTOM && p.y<=ATARI_TOP && p.z>=GROUND_NEAR && p.z<=GROUND_FAR)
			{
				++count;
				avg_x += p.x;
				avg_y += p.y;
				avg_z += p.z;
			}
		}

		// 安定して捉えていると判断するボクセルの数
		if (count>=config.center_atari_voxel_threshould)
		{
			avg_x = avg_x/count;
			avg_y = avg_y/count;
			avg_z = avg_z/count;
		//#	printf("%6d %5.1f %5.1f %5.1f\n", count, avg_x, avg_y, avg_z);
		}
		else
		{
			avg_x = 0.0f;
			avg_y = 0.0f;
			avg_z = 0.0f;
		}
		global.person_center.x = avg_x;
		global.person_center.y = avg_y;
		global.person_center.z = avg_z;
		global.debug.atari_voxels = count;
		
		{
			Timer tm(&time_profile.drawing.drawvoxels);
			drawVoxels(dots, color_body, color_outer);
		}
	}
	else
	{
		if (active_camera==CAM_A)
		{
			dots.init();
			MixDepth(dots, image1, cam1);
			drawVoxels(dots, color_cam1, color_outer);
			dots.init();
			MixDepth(dots, image2, cam2);
			drawVoxels(dots, color_other, color_other);
		}
		else
		{
			dots.init();
			MixDepth(dots, image1, cam1);
			drawVoxels(dots, color_other, color_other);
			dots.init();
			MixDepth(dots, image2, cam2);
			drawVoxels(dots, color_cam2, color_outer);
		}
	}
}

void StClient::CreateAtariFromBodyCenter()
{
	// 体幹アタリ
	int body_x = (-GROUND_LEFT  + global.person_center.x * 1.15f) / GROUND_WIDTH  * HitData::CEL_W;
	int body_y = (GROUND_HEIGHT - global.person_center.y        ) / GROUND_HEIGHT * HitData::CEL_H;

	body_x = minmax(body_x, 0, HitData::CEL_W);
	body_y = minmax(body_y, 0, HitData::CEL_H);

	const int MAX_HIT_VALUE = 9999;
	hitdata.clear();
	hitdata.inc(body_x, body_y, MAX_HIT_VALUE);
}

void StClient::CreateAtariFromDepthMatrix(const Dots& dots)
{
	for (int i=0; i<dots.size(); i+=ATARI_INC)
	{
		// デプスはGreenのなかだけ
		Point3D p = dots[i];
		if (!(p.z>=0.0f && p.z<=2.0f))
		{
			// ignore: too far, too near
			continue;
		}

		// (x,z)を、大きさ1の正方形におさめる
		// はみ出すこともあるが、hitdata.incは有効な部分のみをアタリに使う
		float fx = ((p.x-GROUND_LEFT)/GROUND_WIDTH);
		float fy = ((GROUND_HEIGHT-p.y)/GROUND_HEIGHT);

		const int x = (int)(fx * HitData::CEL_W);
		const int y = (int)(fy * HitData::CEL_H);

		hitdata.inc(x, y);
	}
}

void StClient::CreateAtari(const Dots& dots)
{
	Timer tm(&time_profile.atari);
	hitdata.clear();

#if 1
	// ドットの中央を身体の座標としてアタリを得る
	(void)dots;
	CreateAtariFromBodyCenter();
#else
	// デプスから濃度的にアタリを得る
	CreateAtariFromDepthMatrix(dots);
#endif
}


void StClient::MovieRecord()
{
	if (global.gameinfo.movie.total_frames >= MOVIE_MAX_FRAMES)
	{
		puts("time over! record stop.");
		changeStatus(STATUS_READY);
	}
	else
	{
		auto& mov = global.gameinfo.movie;
#if 0
		VoxelRecorder::record(dot_set, mov.frames[mov.total_frames++]);
#else
		mov.cam1 = cal_cam1.curr;
		mov.cam2 = cal_cam2.curr;
		Depth10b6b::record(dev1.raw_cooked, dev2.raw_cooked, mov.frames[global.frame_index]);
		mov.total_frames = max(mov.total_frames, global.frame_index);
#endif
	}
}



string GameInfo::GetFolderName(const string& id)
{
	string folder = string("//")+config.server_name+"/ST/Movie/";

	// 逆順で追加
	// 0000012345 => '5/4/3/2/1/0/0/0/0/0/'
	const size_t LEN = id.size();	
	for (size_t i=0; i<LEN; ++i)
	{
		folder += id[LEN-1-i];
		folder += '/';
	}

	return folder;
}

string GameInfo::GetMovieFileName(const string& id)
{
	return GetFolderName(id) + id + ".stmov";
}

// サムネ保存
//  - ファイル名: "0000012345-1.jpg"
void GameInfo::save_Thumbnail(const string& basename, const string& suffix, int)
{
	string path = basename + "-" + suffix + ".jpg";

	File f;
	if (!f.openForWrite(path.c_str()))
	{
		Console::printf(CON_RED, "Cannot open file '%s'\n", path.c_str());
		return;
	}
}

bool GameInfo::prepareForSave(const string& player_id, const string& game_id)
{
	this->movie.game_id   = game_id;
	this->movie.player_id = player_id;

	Msg::SystemMessage("Prepare for save!");
	printf("Game-ID: %s\n", game_id.c_str());

	// Folder name: ${BaseFolder}/E/D/C/B/A/0/0/0/0/0/
	const string folder = GetFolderName(game_id);
	mi::Folder::createFolder(folder.c_str());
	printf("Folder: %s\n", folder.c_str());

	// Base name: ${BaseFolder}/E/D/C/B/A/0/0/0/0/0/00000ABCDE
	this->basename = folder + game_id;

	// Open!
	const string filename = this->basename + ".stmov";
	if (!movie_file.openForWrite(filename))
	{
		Msg::ErrorMessage("Cannot open file", filename);
		return false;
	}

	return true;
}

void GameInfo::save()
{
	Msg::SystemMessage("Save to file!");

	// Save Movie
	saveToFile(movie_file, global.gameinfo.movie);
	movie_file.close();
	Msg::Notice("Saved!");

	// Save Thumbnail
	save_Thumbnail(basename,"1",0);
	save_Thumbnail(basename,"2",0);
	save_Thumbnail(basename,"3",0);
	save_Thumbnail(basename,"4",0);
	save_Thumbnail(basename,"5",0);
	save_Thumbnail(basename,"6",0);
}

static void CreateDummyDepth(RawDepthImage& depth)
{
	static int no = 0;
	for (int i=0; i<640*480; ++i)
	{
		int v = 0;
		if (((i*2930553>>3)^((i*39920>>4)+no))%3==0)
		{
			v = (i+no*3)%6500 + (i+no)%2000 + 500;
		}
		depth.image[i] = v;
	}
	no += 6;
}

//====================================================================
// VRAMのクリアやKinectからのデプス取得など環境に関する雑多なことを行う
//====================================================================
void StClient::displayEnvironment()
{
	mi::Timer tm(&time_profile.environment.total);

	// @fps
	this->fps_counter.update();

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
		CreateDummyDepth(dev1.raw_depth);
	}

	if (dev2.device.isValid())
	{
		mi::Timer tm(&time_profile.environment.read2);
		dev2.CreateRawDepthImage_Read();
		dev2.CreateRawDepthImage();
	}
}


static void change_cal_param(Calset& set, float mx, float my, const ChangeCalParamKeys& keys)
{
	auto& curr = set.curr;
	auto& prev = set.prev;

	if (keys.scale)
	{
		curr.scale = prev.scale - 0.2*(mx + my);
	}
	else if (keys.rot_xy)
	{
		curr.rotx = prev.rotx + 0.2*my;
		curr.roty = prev.roty + 0.2*mx;
	}
	else if (keys.rot_z)
	{
		curr.rotz = prev.rotz - 0.2*(mx + my);
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
	string s;
	
	for (int i=0; i<2; ++i)
	{
		const auto& cam = (i==0) ? cal_cam1.curr : cal_cam2.curr;
		char buffer[1024];
		sprintf(buffer,
			"global camera%d = ["
			"x:%+6.3f,"
			"y:%+6.3f,"
			"z:%+6.3f,"
			"rotx:%+6.3f,"
			"roty:%+6.3f,"
			"rotz:%+6.3f,"
			"scale:%+6.3f];\r\n",
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

bool StClient::initGraphics()
{
	auto open_window = [&]()->int{
		printf("Initial Fullscreen: %d\n", config.initial_fullscreen);
		return glfwOpenWindow(
			640,
			480,
			0, 0, 0,
			0, 0, 0,
			(config.initial_fullscreen ? GLFW_FULLSCREEN : GLFW_WINDOW));
	};

	if (glfwInit()==GL_FALSE)
	{
		return false;
	}
	if (open_window()==GL_FALSE)
	{
		return false;
	}

	glfwEnable(GLFW_MOUSE_CURSOR);
	
	{
		string name;
		name += "スポーツタイムマシン クライアント";
		name += " (";
		name += Core::getComputerName();
		name += ")";
		glfwSetWindowTitle(name.c_str());
	}

	init_open_gl_params();
	return true;
}
