#include "mi/mi.h"
#include "StClient.h"

#define local static
#pragma warning(disable:4996) // unsafe function


using namespace mgl;
using namespace mi;
using namespace stclient;
using namespace vector_and_matrix;



//================================
// フィールドのグリッド描画
//================================
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

	// Left and right box
	for (int i=0; i<2; ++i)
	{
		const float x = (i==0) ? GROUND_LEFT : GROUND_RIGHT;
		glBegin(GL_LINE_LOOP);
			glVertex3f(x, GROUND_HEIGHT, -GROUND_NEAR);
			glVertex3f(x, GROUND_HEIGHT, -GROUND_FAR);
			glVertex3f(x,          0.0f, -GROUND_FAR);
			glVertex3f(x,          0.0f, -GROUND_NEAR);
		glEnd();
	}

	// Ceil bar
	glBegin(GL_LINES);
		glVertex3f(GROUND_LEFT,  GROUND_HEIGHT, -GROUND_NEAR);
		glVertex3f(GROUND_RIGHT, GROUND_HEIGHT, -GROUND_NEAR);
		glVertex3f(GROUND_LEFT,  GROUND_HEIGHT, -GROUND_FAR);
		glVertex3f(GROUND_RIGHT, GROUND_HEIGHT, -GROUND_FAR);
	glEnd();



	// run space, @green
	glRGBA(0.25f, 1.00f, 0.25f, 0.25f).glColorUpdate();
	glBegin(GL_QUADS);
		glVertex3f(GROUND_LEFT,  0, -GROUND_NEAR);
		glVertex3f(GROUND_LEFT,  0, -GROUND_FAR);
		glVertex3f(GROUND_RIGHT, 0, -GROUND_FAR);
		glVertex3f(GROUND_RIGHT, 0, -GROUND_NEAR);
	glEnd();
}

//========================================
// アイドル画像の描画と、時間による切り替え
//========================================
void StClient::drawIdleImage()
{
	const int TRANSITION = 5;
	const int NaN = -1;
	static int curr_image = NaN;
	static int prev_image = NaN;
	static int transition = 0;
	
	prev_image = curr_image;
	curr_image = global.idle_select % config.idle_images.size();

	if (prev_image != curr_image)
	{
		transition = 0;
		printf("アイドル画像の変更: %d\n", curr_image);
	}

	if (transition<TRANSITION)
	{
		// トランジションあり
		++transition;
		auto itr2 = config.idle_images.find(prev_image);
		if (itr2!=config.idle_images.end())
		{
			itr2->second.image.drawDepth(0,0,640,480, IDLE_IMAGE_Z, 255);
		}

		auto itr = config.idle_images.find(curr_image);
		if (itr!=config.idle_images.end())
		{
			itr->second.image.drawDepth(0,0,640,480, IDLE_IMAGE_Z, 255*transition/TRANSITION);
		}
	}
	else
	{
		// そのまま表示
		auto itr = config.idle_images.find(curr_image);
		if (itr!=config.idle_images.end())
		{
			itr->second.image.drawDepth(0,0,640,480, IDLE_IMAGE_Z);
		}
	}
}

//===============================================
// 壁や背景など、走行環境を描画する @runenv @wall
//===============================================
void StClient::drawRunEnv()
{
	gl::ClearGraphics(255,255,255);

	if (global.run_env==nullptr)
	{
		// 環境がありませんでした
		// なんらかの問題あり?
		Msg::ErrorMessage("drawRunEnv - no run env (bug?)");
		return;
	}

	// デフォルト環境 (BACKGROUND命令がこなかった）
	if (global.run_env==Config::getDefaultRunEnv())
	{
		static int t = 0;
		++t;
		const int N = 20;
		bool color = false;
		const int x = -(t % (2*N));
		for (int i=0; i<640+480+2*N; i+=N)
		{
			color
				? glRGBA(220,220,188)()
				: glRGBA(255,252,243)();
			color = !color;
			glBegin(GL_QUADS);
				glVertex3f(x+i,         0, 10);
				glVertex3f(x+i+N,       0, 10);
				glVertex3f(x+i+N-480, 480, 10);
				glVertex3f(x+i  -480, 480, 10);
			glEnd();
		}
		return;
	}

	auto& img = global.run_env->background.image;
	img.drawDepth(0,0,640,480,10);
}


//================================
// 三次元上に壁を描画する @wall
//================================
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


#undef rad2
struct Trianglev
{
	float x;
	float y;
	float rad;
	float rad2;
	float size;
};

std::vector<Trianglev> triangle;


void StClient::drawManyTriangles()
{
	static float tm = 0.0f;
	tm += 0.001f;

	const float PI = 3.1415923f;
	const int COUNT = 24;
	if (triangle.empty())
	{
		for (int i=0; i<COUNT; ++i)
		{
			Trianglev t;
			float rad = 2*PI*i/COUNT;
			t.rad  = rad;
			t.rad2 = 0.0f;
			t.x    = cosf(rad)*120+320;
			t.y    = sinf(rad)*120+240;
			t.size = 12.0f;
			triangle.push_back(t);
		}
	}

	gl::DepthTest(false);
	gl::Texture(false);
	glRGBA(100,50,0, 80)();
	glBegin(GL_TRIANGLES);
	const float R1 = 2*PI * 0 / 3;
	const float R2 = 2*PI * 1 / 3;
	const float R3 = 2*PI * 2 / 3;
	for (int i=0; i<COUNT; ++i)
	{
		Trianglev& t = triangle[i];
		const float rad = t.rad + t.rad2;
		glVertex2f(
			t.x + cosf(rad+R1)*t.size,
			t.y + sinf(rad+R1)*t.size);
		glVertex2f(
			t.x + cosf(rad+R2)*t.size,
			t.y + sinf(rad+R2)*t.size);
		glVertex2f(
			t.x + cosf(rad+R3)*t.size,
			t.y + sinf(rad+R3)*t.size);
	
		t.rad += 0.01f;
		t.rad2 = tm;
		t.x    = cosf(t.rad)*120+320;
		t.y    = sinf(t.rad)*120+240;
	}
	glEnd();
}


//====================================
// キャリブレーション時以外の画面上下の帯
//====================================
void StClient::drawNormalGraphicsObi()
{
	const int top_line = 95*480/768;
	const int bottom_line = 701*480/768;
	this->display2dSectionPrepare();
	glRGBA::black();
	glBegin(GL_QUADS);
		glVertex2i(0,0);
		glVertex2i(640,0);
		glVertex2i(640,top_line);
		glVertex2i(0,top_line);

		glVertex2i(0,bottom_line);
		glVertex2i(640,bottom_line);
		glVertex2i(640,480);
		glVertex2i(0,480);
	glEnd();
}


//================================
// チーターなどのMovingObjectの描画
//================================
void StClient::drawMovingObject()
{
	volatile int frame = global.frame_index - global.game_start_frame;

	// ゲーム中だけ、フレーム時間が進行する
	const int mo_frame = global.in_game_or_replay
		? frame
		: 0;

	MovingObject& mo = global.partner_mo;
	auto& image = mo.getFrameImage(mo_frame);
	float real_meter = mo.getDistance();

	const float TURN = mo.getTurnPosition();
	bool forward = (real_meter<TURN);
	float virtual_meter = forward ? real_meter : (TURN-(real_meter-TURN));

	//体長110-140センチメートル
	//
//	int mo_size_cm = 125;
//	int mo_pixel = 640/4 * mo_size_cm;

	const int w = 320 * (forward ? +1 : -1);
	const int h = 320;

	// Xの中央
	const float x = virtual_meter - config.getScreenLeftMeter();

	// このYの「上」に表示される
	const int y = 390;

	const float dx = x/4.0*640;
	image.drawDepth(dx-w/2, y-h, w, h, -1.0f);
	image.draw(0,0, 50,50);


	if (global.in_game_or_replay)
	{
		// INIT > START > GAME-START > (ゲーム中) > STOP
		// ゲーム中のみ時間がながれます
		mo.updateDistance();
	}
}

//============
// Dotsの描画
//============
void StClient::drawRealDots(Dots& dots, float dot_size)
{
	const glRGBA color_body = global.gameinfo.movie.player_color_rgba;
	const glRGBA color_cam1(80,190,250);
	const glRGBA color_cam2(250,190,80);
	const glRGBA color_other(170,170,170);
	const glRGBA color_outer(120,130,200);
	
	const CamParam cam1 = cal_cam1.curr;
	const CamParam cam2 = cal_cam2.curr;

	const RawDepthImage& image1 = dev1.raw_cooked;
	const RawDepthImage& image2 = dev2.raw_cooked;

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
			drawVoxels(dots, dot_size, color, color_outer, DRAW_VOXELS_PERSON);
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
			drawVoxels(dots, dot_size, color_body, color_outer, DRAW_VOXELS_PERSON);
		}
	}
	else
	{
		if (active_camera==CAM_A)
		{
			dots.init();
			MixDepth(dots, image1, cam1);
			drawVoxels(dots, dot_size, color_cam1, color_outer, DRAW_VOXELS_PERSON);
			dots.init();
			MixDepth(dots, image2, cam2);
			drawVoxels(dots, dot_size, color_other, color_other, DRAW_VOXELS_PERSON);
		}
		else
		{
			dots.init();
			MixDepth(dots, image1, cam1);
			drawVoxels(dots, dot_size, color_other, color_other, DRAW_VOXELS_PERSON);
			dots.init();
			MixDepth(dots, image2, cam2);
			drawVoxels(dots, dot_size, color_cam2, color_outer, DRAW_VOXELS_PERSON);
		}
	}
}
