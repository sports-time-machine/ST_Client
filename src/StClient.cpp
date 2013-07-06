#include "mi/mi.h"
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


const int TEXTURE_SIZE = 512;




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
	cal_cam1.curr = config.cam1;
	cal_cam2.curr = config.cam2;
	udp_recv.init(UDP_CONTROLLER_TO_CLIENT);
	mode.mirroring   = config.mirroring;
}

StClient::~StClient()
{
}


bool StClient::init()
{
	Msg::BarMessage("Init StClient");

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
		printf("Init font...");
		const string font_folder = "C:/Windows/Fonts/";
		monospace.init(font_folder + "Cour.ttf", 12);
		puts("done!");
	}

	reloadResources();

	Msg::BarMessage("Init done!");
	Console::nl();


	{	
		auto& moi = global.moi_lib["CHEETAH-1"];
		moi.addFrame(20, "C:/ST/Picture/MovingObject/CHEETAH-1/Cheetah01.png");
		moi.addFrame(20, "C:/ST/Picture/MovingObject/CHEETAH-1/Cheetah02.png");
		moi.addFrame(20, "C:/ST/Picture/MovingObject/CHEETAH-1/Cheetah03.png");
		moi.addFrame(20, "C:/ST/Picture/MovingObject/CHEETAH-1/Cheetah04.png");
		moi.addFrame(20, "C:/ST/Picture/MovingObject/CHEETAH-1/Cheetah05.png");
		moi.addFrame(20, "C:/ST/Picture/MovingObject/CHEETAH-1/Cheetah06.png");
	}
	{	
		auto& moi = global.moi_lib["MUSAGI"];
		moi.addFrame(40, "C:/ST/Picture/MovingObject/MUSAGI/01.png");
		moi.addFrame(30, "C:/ST/Picture/MovingObject/MUSAGI/02.png");
		moi.addFrame(20, "C:/ST/Picture/MovingObject/MUSAGI/03.png");
		moi.addFrame(20, "C:/ST/Picture/MovingObject/MUSAGI/04.png");
		moi.addFrame(20, "C:/ST/Picture/MovingObject/MUSAGI/05.png");
		moi.addFrame(10, "C:/ST/Picture/MovingObject/MUSAGI/06.png");
		moi.addFrame(15, "C:/ST/Picture/MovingObject/MUSAGI/07.png");
	}

	return true;
}

void StClient::reloadResources()
{
	// @init @image @png @jpg
#define LOAD_IMAGE(NAME) \
	printf("Load '%s'...", #NAME);\
	if (global.images.NAME.createFromImageA(config.picture_folder + config.images.NAME)){\
		puts("done!");\
	}

	// アイドル画像のロード
	for (size_t i=0; i<config.idle_images.size(); ++i)
	{
		_rw_config.idle_images[i].reload("アイドル画像のロード");
	}

	// 走行環境のロード
	for (auto itr=_rw_config.run_env.begin(); itr!=_rw_config.run_env.end(); ++itr)
	{
		Config::RunEnv& env = itr->second;
		env.background.reload("走行環境:画像のロード");
	}


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




// 通常時の描画（キャリブレーション時をのぞく描画）
void StClient::drawNormalGraphics()
{
	// クライアントステータスによる描画の分岐
	// VRAMのクリア(glClearGraphics)はそれぞれに行う
	switch (clientStatus())
	{
	case STATUS_SLEEP:
		// 2Dスリープ画像
		gl::ClearGraphics(255,255,255);
		this->display2dSectionPrepare();
		global.images.sleep.draw(0,0,640,480);
		//# this->drawManyTriangles();
		break;

	case STATUS_BLACK:
		gl::ClearGraphics(0,0,0);
		break;

	case STATUS_SAVING:
		// セーブしている間は画面描画できないので、
		// アイドル画像を表示しておく
		// このとき、Kinect画像も停止するため、
		// 実映像は表示しない
		gl::ClearGraphics(255,255,255);
		this->display2dSectionPrepare();
		this->drawIdleImage();
		break;

	case STATUS_READY:
		// 出走準備ができても画面はアイドルのままとします
	case STATUS_IDLE:{
		// 2Dアイドル画像
		gl::ClearGraphics(255,255,255);
		this->display2dSectionPrepare();
		this->drawIdleImage();
		//# this->drawManyTriangles();

		// アイドル用ボディ
		global.gameinfo.movie.player_color_rgba.set(80,70,50);

		// 実映像の表示
		this->display3dSectionPrepare();
		static Dots dots;
		drawRealDots(dots, config.person_dot_px);
		break;}

	case STATUS_PICT:{
		// 画像
		puts("pict");
		gl::ClearGraphics(255,255,255);
		this->display2dSectionPrepare();
		global.images.pic[global.picture_number].draw(0,0,640,480);
		break;}

	case STATUS_INIT_FLOOR:{
		// 実映像の表示
		gl::ClearGraphics(255,255,255);
		this->display3dSectionPrepare();
		this->display3dSection();
		static Dots dots;
		drawRealDots(dots, config.person_dot_px);
		
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
		{
			mi::Timer tm(&time_profile.drawing.wall);
			this->display2dSectionPrepare();
			this->drawRunEnv();
		}
			
		if (global.partner_mo.enabled())
		{
			this->display2dSectionPrepare();
			drawMovingObject();
		}

		this->display3dSectionPrepare();
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


// 1フレで実行する内容
void StClient::processOneFrame()
{
	++global.total_frames;

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

	// 1フレ描画
	this->drawOneFrame();

	// 描画していなければ床消し追記する
	//   - キャリブレーションモード以外で、
	//   - 画面にボクセルが描画されていないとき
	if (!global.calibration.enabled)
	{
		if (global.dot_count < 500)
		{
			++global.auto_clear_floor_count;
			this->dev1.updateFloorDepth();
			this->dev2.updateFloorDepth();
		}
	}

	// デバッグ用のフレームオートインクリメント
	if (global.frame_auto_increment)
	{
		++global.frame_index;
	}
}

void StClient::drawOneFrame()
{
	// 描画したかのフラグ
	global.voxel_drew = false;

	if (global.calibration.enabled)
	{
		glfwEnable(GLFW_MOUSE_CURSOR);

		// スーパーモードとしてのキャリブレーションモード
		gl::ClearGraphics(255,255,255);
		this->display3dSectionPrepare();
		{
			mi::Timer tm(&time_profile.drawing.grid);
			this->drawFieldGrid(500);
		}

		// 実映像の表示 -- キャリブレーション時はドットサイズ固定
		static Dots dots;
		this->drawRealDots(dots, 1.5f);
	}
	else
	{
		glfwDisable(GLFW_MOUSE_CURSOR);

		this->drawNormalGraphics();
		this->drawNormalGraphicsObi();
	}

	if (global.show_debug_info)
	{
		this->display2dSectionPrepare();
		this->displayDebugInfo();
	}

	glfwSwapBuffers();
}

// ゲーム情報の初期化
//   INIT命令とSTART命令で実行される
void StClient::initGameInfo()
{
	startMovieRecordSettings();

	// 以下の情報は破棄しない
	// global.run_env
	// ボディの色
	//    ...など

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
	this->scalex = (kbd['J'] & 0x80)!=0;
}





void stclient::MixDepth(Dots& dots, const RawDepthImage& src, const CamParam& cam)
{
	const mat4x4 trans = mat4x4::create(
			cam.rot.x, cam.rot.y, cam.rot.z,
			cam.pos.x, cam.pos.y, cam.pos.z,
			cam.scale.x, cam.scale.y, cam.scale.z);

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

bool StClient::drawVoxels(const Dots& dots, float dot_size, glRGBA inner_color, glRGBA outer_color, DrawVoxelsStyle style)
{
#if 0
	// Create histogram
	for (int i=0; i<dots.size(); ++i)
	{
		dots[i].
	}
#endif
	int atari_count = 0;
	global.dot_count = 0;
	for (int i=0; i<dots.size(); ++i)
	{
		const float x = dots[i].x;
		const float y = dots[i].y;
		const float z = dots[i].z;

		const bool in_x = (x>=GROUND_LEFT && x<=GROUND_RIGHT);
		const bool in_y = (y>=0.0f && y<=GROUND_HEIGHT);
		const bool in_z = (z>=0.0f && z<=GROUND_DEPTH);

		if (in_x && in_y)
		{
			++global.dot_count;
			if (in_z)
			{
				++atari_count;
			}
		}
	}

	global.atari_count = atari_count;


	// GAME/IDLEモードで描画すべきボクセルが少ない場合、描画をとりやめる
	switch (clientStatus())
	{
	case STATUS_GAME:
	case STATUS_IDLE:
		if (atari_count < config.whitemode_voxel_threshould)
		{
			global.voxels_alpha = 0.0f;
			return false;
		}
		break;
	}



	// 描画しましたフラグ
	global.voxel_drew = true;



	if (global.voxels_alpha<1.0f)
	{
		global.voxels_alpha = minmax(global.voxels_alpha + 0.07f, 0.0f, 1.0f);
	}




	// @voxel @dot
	const bool quad = false;
	if (quad)
	{
		gl::Texture(true);
		glBindTexture(GL_TEXTURE_2D, global.images.dot);
		glBegin(GL_QUADS);
	}
	else
	{
		gl::Texture(false);
		glPointSize(dot_size);
		glBegin(GL_POINTS);
	}

	const int inc = 
		(style==DRAW_VOXELS_PERSON)
			? mi::minmax(config.person_inc, MIN_VOXEL_INC, MAX_VOXEL_INC)
			: mi::minmax(config.movie_inc,  MIN_VOXEL_INC, MAX_VOXEL_INC); 
	const int SIZE16 = dots.size() << 4;

	const float add_y = 
		(style==DRAW_VOXELS_PERSON)
			? 0.0f
			: config.partner_y;
	
	for (int i16=0; i16<SIZE16; i16+=inc)
	{
		const int i = (i16 >> 4);

		const float x = dots[i].x;
		const float y = dots[i].y;
		const float z = dots[i].z;

		const bool in_x = (x>=GROUND_LEFT && x<=GROUND_RIGHT);
		const bool in_y = (y>=0.0f && y<=GROUND_HEIGHT);
		const bool in_z = (z>=0.0f && z<=GROUND_DEPTH);

#if 0
		// Depth is alpha version
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
#else
		// Depth is alpha version
		float col = global.voxels_alpha * z/4;
		if (col<0.25f) col=0.25f;
		if (col>0.90f) col=0.90f;
		col = 1.00f - col;
		const int col255 = (int)(col * config.person_base_alpha);

		if (global.calibration.enabled)
		{
			// キャリブレーション中
			if (in_x && in_y && in_z)
				inner_color.glColorUpdate(col255>>1);
			else
				outer_color.glColorUpdate(col255>>2);
		}
		else
		{
			// ゲーム中はエリア内だけ表示する
			if (in_x && in_y && in_z)
				inner_color.glColorUpdate(col255);
		}
#endif

		if (quad)
		{
			const float K = 0.01f;
			glTexCoord2f(0,0); glVertex3f(x-K,y-K,-z);
			glTexCoord2f(1,0); glVertex3f(x+K,y-K,-z);
			glTexCoord2f(1,1); glVertex3f(x+K,y+K,-z);
			glTexCoord2f(0,1); glVertex3f(x-K,y+K,-z);
		}
		else
		{
			glVertex3f(x,y+add_y,-z);
		}
	}

	glEnd();
	return true;
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
		if (!(p.z>=GROUND_NEAR && p.z<=GROUND_FAR))
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

		// タイムオーバーしたときは最終アタリを送り
		// ゲームを強制的に終わらせる
		udp_send.send("HIT 9999 finish");
		changeStatus(STATUS_READY);
	}
	else
	{
		auto& mov = global.gameinfo.movie;
		mov.dot_size = config.person_dot_px;
		mov.cam1 = cal_cam1.curr;
		mov.cam2 = cal_cam2.curr;
		Depth10b6b::record(dev1.raw_cooked, dev2.raw_cooked, mov.frames[global.frame_index]);
		mov.total_frames = max(mov.total_frames, global.frame_index);
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
		Console::printf(CON_RED, "Cannot open file '%s' (save_Thumbnail)\n", path.c_str());
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
	printf("Folder: %s\n", folder.c_str());
	mi::Folder::createFolder(folder.c_str());

	// Base name: ${BaseFolder}/E/D/C/B/A/0/0/0/0/0/00000ABCDE
	this->basename = folder + game_id;

	// Open: 00000ABCDE-1.stmov
	const string filename = this->basename + "-" + to_s(config.client_number) + ".stmov";
	printf("Movie: %s\n", filename.c_str());
	if (!movie_file.openForWrite(filename))
	{
		Msg::ErrorMessage("Cannot open file (preapreForSave)", filename);
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
#if 0
	save_Thumbnail(basename,"1",0);
	save_Thumbnail(basename,"2",0);
	save_Thumbnail(basename,"3",0);
	save_Thumbnail(basename,"4",0);
	save_Thumbnail(basename,"5",0);
	save_Thumbnail(basename,"6",0);
#endif
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

	if (keys.scalex)
	{
		curr.scale.x = prev.scale.x - 0.2*(mx + my);
	}
	else if (keys.scale)
	{
		curr.scale.x = prev.scale.x - 0.2*(mx + my);
		curr.scale.y = prev.scale.y - 0.2*(mx + my);
		curr.scale.z = prev.scale.z - 0.2*(mx + my);
	}
	else if (keys.rot_xy)
	{
		curr.rot.x = prev.rot.x + 0.2*my;
		curr.rot.y = prev.rot.y + 0.2*mx;
	}
	else if (keys.rot_z)
	{
		curr.rot.z = prev.rot.z - 0.2*(mx + my);
	}
	else
	{
		switch (global.view_mode)
		{
		case VM_2D_TOP:// ウエキャリブレーション
			if (keys.ctrl)
			{
				curr.rot.y = prev.rot.y + mx - my;
			}
			else
			{
				curr.pos.x = prev.pos.x + mx;
				curr.pos.z = prev.pos.z - my;
			}
			break;
		case VM_2D_LEFT:// ヨコキャリブレーション
			if (keys.ctrl)
			{
				curr.rot.x = prev.rot.x - mx + my;
			}
			else
			{
				curr.pos.z = prev.pos.z - mx;
				curr.pos.y = prev.pos.y - my;
			}
			break;
		case VM_2D_FRONT:// マエキャリブレーション
		case VM_2D_RUN:  // 走り画面
			if (keys.ctrl)
			{
				curr.rot.z = prev.rot.z - mx - my;
			}
			else
			{
				curr.pos.x = prev.pos.x + mx;
				curr.pos.y = prev.pos.y - my;
			}
			break;
		default:
			if (keys.ctrl)
			{
				curr.pos.x = prev.pos.x + mx;
				curr.pos.y = prev.pos.y - my;
			}				
			else
			{
				curr.pos.x = prev.pos.x + mx;
				curr.pos.z = prev.pos.z - my;
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



string StClient::GetCamConfigPath()
{
	string filename;
	filename += "C:/ST/";
	filename += mi::Core::getComputerName();
	filename += "_cam.psl";
	return filename;
}

void StClient::SaveCamConfig()
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
			"rx:%+6.3f,"
			"ry:%+6.3f,"
			"rz:%+6.3f,"
			"sx:%+6.3f,"
			"sy:%+6.3f,"
			"sz:%+6.3f];\r\n",
				1+i,
				cam.pos.x,
				cam.pos.y,
				cam.pos.z,
				cam.rot.x,
				cam.rot.y,
				cam.rot.z,
				cam.scale.x,
				cam.scale.y,
				cam.scale.z);
		s += buffer;
	}

	File f;
	if (f.openForWrite(GetCamConfigPath()))
	{
		f.write(s);
	}
}


static void init_open_gl_params()
{
	glEnable(GL_TEXTURE_2D);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	gl::AlphaBlending(true);
}


 HWND GetConsoleHwnd(void)
{
	const int buffer_size = 1024;
	static char new_title[buffer_size];
	static char old_title[buffer_size];

	GetConsoleTitle(old_title, buffer_size);
	wsprintf(new_title,"%d/%d", GetTickCount(), GetCurrentProcessId());
	SetConsoleTitle(new_title);
	Sleep(40);
	HWND hwndFound = FindWindow(NULL, new_title);
	SetConsoleTitle(old_title);
	return hwndFound;
}

HWND GetGlfwHwnd(void)
{
	const int buffer_size = 1024;
	static char new_title[buffer_size];

	wsprintf(new_title,"%d/%d", GetTickCount(), GetCurrentProcessId());
	glfwSetWindowTitle(new_title);
	Sleep(40);
	return FindWindow(NULL, new_title);
}

void FullScreen()
{
	const HWND hwnd = GetGlfwHwnd();
	SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
	const int dispx = GetSystemMetrics(SM_CXSCREEN);
	const int dispy = GetSystemMetrics(SM_CYSCREEN);
	glfwSetWindowSize(dispx, dispy);
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
			GLFW_WINDOW);
	};

	if (glfwInit()==GL_FALSE)
	{
		return false;
	}
	if (open_window()==GL_FALSE)
	{
		return false;
	}

	if (config.initial_fullscreen)
	{
		FullScreen();
	}
	else
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
