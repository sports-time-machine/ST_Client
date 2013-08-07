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


const int TEXTURE_SIZE = 512;





void StClient::initDrawParamFromGlobal(VoxGrafix::DrawParam& param, float dot_size)
{
	param = VoxGrafix::DrawParam();
	param.dot_size          = dot_size;
	param.is_calibration    = global.calibration.enabled;
	param.movie_inc         = config.movie_inc;
	param.person_inc        = config.person_inc;
	param.mute_if_veryfew   = (clientStatus()==STATUS_GAME || clientStatus()==STATUS_IDLE);
	param.mute_threshould   = config.whitemode_voxel_threshould;
	param.partner_y         = config.partner_y;
	param.person_base_alpha = config.person_base_alpha;
}


typedef std::vector<openni::RGB888Pixel> RgbScreen;
typedef std::map<int,RgbScreen> RgbScreenMovie;


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
	udp_recv.init(UDP_CONTROLLER_TO_CLIENT);
	global.mirroring = config.mirroring;
}

StClient::~StClient()
{
}


bool StClient::init2()
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

	// �A�C�h���摜�̃��[�h
	for (size_t i=0; i<config.idle_images.size(); ++i)
	{
		_rw_config.idle_images[i].reload("�A�C�h���摜�̃��[�h");
	}

	// ���s���̃��[�h
	for (auto itr=_rw_config.run_env.begin(); itr!=_rw_config.run_env.end(); ++itr)
	{
		Config::RunEnv& env = itr->second;
		env.background.reload("���s��:�摜�̃��[�h");
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
	// �X�e�[�^�X�`�F���W�ƂƂ��Ƀt���[�������[���ɂ���
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
		// ��ʊO -- ignore!
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




// �ʏ펞�̕`��i�L�����u���[�V���������̂����`��j
void StClient::drawNormalGraphics()
{
	// �N���C�A���g�X�e�[�^�X�ɂ��`��̕���
	// VRAM�̃N���A(glClearGraphics)�͂��ꂼ��ɍs��
	switch (clientStatus())
	{
	case STATUS_SLEEP:
		// 2D�X���[�v�摜
		gl::ClearGraphics(255,255,255);
		this->display2dSectionPrepare();
		global.images.sleep.draw(0,0,640,480);
		//# this->drawManyTriangles();
		break;

	case STATUS_BLACK:
		gl::ClearGraphics(0,0,0);
		break;

	case STATUS_SAVING:
		// �Z�[�u���Ă���Ԃ͉�ʕ`��ł��Ȃ��̂ŁA
		// �A�C�h���摜��\�����Ă���
		// ���̂Ƃ��AKinect�摜����~���邽�߁A
		// ���f���͕\�����Ȃ�
		gl::ClearGraphics(255,255,255);
		this->display2dSectionPrepare();
		this->drawIdleImage();
		break;

	case STATUS_READY:
		// �o���������ł��Ă���ʂ̓A�C�h���̂܂܂Ƃ��܂�
	case STATUS_IDLE:{
		// 2D�A�C�h���摜
		gl::ClearGraphics(255,255,255);
		this->display2dSectionPrepare();
		this->drawIdleImage();
		//# this->drawManyTriangles();

		// ���f���̕\��
		this->display3dSectionPrepare();
		static Dots dots;
		drawRealDots(
			dots,
			glRGBA(80,70,50, 160),
			config.person_dot_px);
		break;}

	case STATUS_PICT:{
		// �摜
		puts("pict");
		gl::ClearGraphics(255,255,255);
		this->display2dSectionPrepare();
		global.images.pic[global.picture_number].draw(0,0,640,480);
		break;}

	case STATUS_INIT_FLOOR:{
		// ���f���̕\��
		gl::ClearGraphics(0,0,0);
		this->display3dSectionPrepare();
		this->display3dSection();
		static Dots dots;
		drawRealDots(
			dots,
			glRGBA(255,255,255,64),
			config.person_dot_px);
		
		// �������̃A�b�v�f�[�g
		dev1.updateFloorDepth();
		dev2.updateFloorDepth();

		// �e�L�X�g
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


// 1�t���Ŏ��s������e
void StClient::processOneFrame()
{
	++global.total_frames;

	// �t���[���J�n����UDP�R�}���h�̏���������
	this->processUdpCommands();

	// N�t���ł̃X�i�b�v�V���b�g
	if (config.auto_snapshot_interval>0)
	{
		static int frame_count;
		++frame_count;
		if ((frame_count % config.auto_snapshot_interval)==0)
		{
			this->createSnapshot();
		}
	}

	// �J�����ړ��̓K�p
	eye.updateCameraMove();

	this->processKeyInput();
	this->processMouseInput();

	// �L�l�N�g���͂˂ɂ�����Ă���
	this->displayEnvironment();

	// �`�悵���h�b�g�̐�
	VoxGrafix::global.atari_count = 0;
	VoxGrafix::global.dot_count   = 0;

	// 1�t���`��
	this->drawOneFrame();

	// �`�悵�Ă��Ȃ���Ώ������ǋL����
	//   - �A�C�h�����ŁA
	//   - ��ʂɃ{�N�Z�����`�悳��Ă��Ȃ��Ƃ�
	if (clientStatus()==STATUS_IDLE && config.auto_cf_enabled)
	{
		if (VoxGrafix::global.dot_count < config.auto_cf_threshould)
		{
			++global.auto_clear_floor_count;
			this->dev1.updateFloorDepth();
			this->dev2.updateFloorDepth();
		}
	}

	// �f�o�b�O�p�̃t���[���I�[�g�C���N�������g
	if (global.frame_auto_increment)
	{
		++global.frame_index;
	}
}

void StClient::drawOneFrame()
{
	if (global.calibration.enabled)
	{
		glfwEnable(GLFW_MOUSE_CURSOR);

		// �X�[�p�[���[�h�Ƃ��ẴL�����u���[�V�������[�h
		gl::ClearGraphics(255,255,255);
		this->display3dSectionPrepare();
		{
			mi::Timer tm(&time_profile.drawing.grid);
			this->drawFieldGrid(500);
		}

		// ���f���̕\�� -- �L�����u���[�V�������̓h�b�g�T�C�Y�Œ�
		static Dots dots;
		this->drawRealDots(dots, glRGBA(70,80,100), 1.5f);
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

// �Q�[�����̏�����
//   INIT���߂�START���߂Ŏ��s�����
void StClient::initGameInfo()
{
	startMovieRecordSettings();

	// �ȉ��̏��͔j�����Ȃ�
	// global.run_env
	// �{�f�B�̐F
	//    ...�Ȃ�

	// �ŏ��̓����蔻������
	const int FIRST_HIT_NUMBER = 0;
	global.hit_stage = FIRST_HIT_NUMBER;
	global.hit_objects.clear();
	global.on_hit_setup(global.hit_stage);
}

bool StClient::run()
{
	this->cal_cam1.curr    = config.cam1;
	this->cal_cam2.curr    = config.cam2;

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
	AppCore::MyGetKeyboardState(kbd);

	this->ctrl   = (kbd[VK_CONTROL] & 0x80)!=0;
	this->rot_xy = (kbd['T'] & 0x80)!=0;
	this->rot_z  = (kbd['Y'] & 0x80)!=0;
	this->scale  = (kbd['U'] & 0x80)!=0;
	this->scalex = (kbd['J'] & 0x80)!=0;
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
	// �̊��A�^��
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
	for (int i=0; i<dots.length(); i+=ATARI_INC)
	{
		// �f�v�X��Green�̂Ȃ�����
		Point3D p = dots[i];
		if (!(p.z>=GROUND_NEAR && p.z<=GROUND_FAR))
		{
			// ignore: too far, too near
			continue;
		}

		// (x,z)���A�傫��1�̐����`�ɂ����߂�
		// �͂ݏo�����Ƃ����邪�Ahitdata.inc�͗L���ȕ����݂̂��A�^���Ɏg��
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
	// �h�b�g�̒�����g�̂̍��W�Ƃ��ăA�^���𓾂�
	(void)dots;
	CreateAtariFromBodyCenter();
#else
	// �f�v�X����Z�x�I�ɃA�^���𓾂�
	CreateAtariFromDepthMatrix(dots);
#endif
}


void StClient::MovieRecord()
{
	if (global.gameinfo.movie.total_frames >= config.getMaxMovieFrames())
	{
		puts("time over! record stop.");

		// �^�C���I�[�o�[�����Ƃ��͍ŏI�A�^���𑗂�
		// �Q�[���������I�ɏI��点��
		udp_send.send("GAME-TIMEOUT");
		changeStatus(STATUS_READY);
	}
	else
	{
		auto& mov = global.gameinfo.movie;
		mov.dot_size = config.person_dot_px;
		mov.cam1 = cal_cam1.curr;
		mov.cam2 = cal_cam2.curr;
		
		// �^��͂˂ɍŐV�`���ōs��
		mov.ver = MovieData::VER_1_1;
		Depth10b6b_v1_1::record(dev1.raw_cooked, dev2.raw_cooked, mov.frames[global.frame_index]);

		mov.total_frames = max(mov.total_frames, global.frame_index);
	}
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
// VRAM�̃N���A��Kinect����̃f�v�X�擾�ȂǊ��Ɋւ���G���Ȃ��Ƃ��s��
//====================================================================
void StClient::displayEnvironment()
{
	mi::Timer tm(&time_profile.environment.total);

	// @fps
	this->fps_counter.update();

	// Kinect����������炤
	if (dev1.device.isValid())
	{
		mi::Timer tm(&time_profile.environment.read1);
		dev1.CreateRawDepthImage_Read();
		dev1.CreateRawDepthImage();
	}
	else
	{
		// �_�~�[�̏�� @random @noise
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
		case VM_2D_TOP:// �E�G�L�����u���[�V����
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
		case VM_2D_LEFT:// ���R�L�����u���[�V����
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
		case VM_2D_FRONT:// �}�G�L�����u���[�V����
		case VM_2D_RUN:  // ������
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

bool StClient::init1_GraphicsOnly()
{
	string name;
	name += "�X�|�[�c�^�C���}�V�� �N���C�A���g";
	name += " (";
	name += Core::getComputerName();
	name += ")";
	glfwSetWindowTitle(name.c_str());

	return AppCore::initGraphics(config.initial_fullscreen, name);
}
