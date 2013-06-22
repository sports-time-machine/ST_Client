#include "ST_Client.h"

using namespace stclient;
using namespace mgl;
using namespace mi;


//========================
// 3D描画するための下準備
//========================
void StClient::display3dSectionPrepare()
{
	// PROJECTION
	gl::Projection();
	gl::LoadIdentity();

	// 2D or 3D
	if (global.view.is_2d_view)
	{
		const double w = global.view.view2d.width;
		glOrtho(-w/2, +w/2, 0, (w*3/4), -3.0, +120.0);
	}
	else
	{
		gluPerspective(30.0f, 4.0f/3.0f, 1.0f, 100.0f);
	}

	eye.gluLookAt();

	// MODEL
	gl::Texture(false);
	gl::DepthTest(true);
	gl::ModelView();
	gl::LoadIdentity();
}

//========================
// 3D描画メイン
//========================
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

//========================
// 2D描画するための下準備
//========================
void StClient::display2dSectionPrepare()
{
	gl::Projection();
	gl::LoadIdentity();
	glOrtho(0, 640, 480, 0, -1.0, 1.0);

	gl::Texture(false);
	gl::DepthTest(false);
}

//========================
// 2D描画メイン
//========================
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

void StClient::display2()
{
	static int frames = 0;
	++frames;

	const int H=15;
	int y = 10;

	auto nl = [&](){ y+=H/2; };
	auto pr = freetype::print;

	const glRGBA heading = global_config.text.heading_color;
	const glRGBA text    = global_config.text.normal_color;
	const glRGBA b       = global_config.text.dt_color;
	const glRGBA p       = global_config.text.dd_color;
	
	auto color = [](bool status){
		(status
			? global_config.text.dt_color
			: global_config.text.dd_color)();
	};
	
	{
		ChangeCalParamKeys keys;
		keys.init();
		heading();
		pr(monospace, 320, y,
			(keys.rot_xy) ? "<XY-rotation>" :
			(keys.rot_z)  ? "<Z-rotation>" :
			(keys.scale)  ? "<Scaling>" : "");
	}

	{
		int y2 = y;
		heading();
		pr(monospace, 200, y2+=H, "EYE");
		text();
		pr(monospace, 200, y2+=H, "x =%+9.4f [adsw]", eye.x);
		pr(monospace, 200, y2+=H, "y =%+9.4f [q/e]", eye.y);
		pr(monospace, 200, y2+=H, "z =%+9.4f [adsw]", eye.z);
		pr(monospace, 200, y2+=H, "rh=%+9.4f(rad)", eye.rh);
		pr(monospace, 200, y2+=H, "v =%+9.4f [q/e]", eye.v);
		y2+=H;
		pr(monospace, 200, y2+=H, "P-inc = %3d [g/h]", config.person_inc);
		pr(monospace, 200, y2+=H, "M-inc = %3d [n/m]", config.movie_inc);
	}

	heading();
	pr(monospace, 20, y+=H, "View Mode");
	text();
	{
		const auto vm = global.view_mode;
		color(vm==VM_2D_LEFT);  pr(monospace, 20, y+=H, "[F1] 2D left");
		color(vm==VM_2D_TOP);   pr(monospace, 20, y+=H, "[F2] 2D top");
		color(vm==VM_2D_FRONT); pr(monospace, 20, y+=H, "[F3] 2D front");
		color(false);           pr(monospace, 20, y+=H, "[F4] ----");
		color(vm==VM_2D_RUN);   pr(monospace, 20, y+=H, "[F5] 2D run");
		color(vm==VM_3D_LEFT);  pr(monospace, 20, y+=H, "[F6] 3D left");
		color(vm==VM_3D_RIGHT); pr(monospace, 20, y+=H, "[F7] 3D right");
		color(vm==VM_3D_FRONT); pr(monospace, 20, y+=H, "[F8] 3D front");
	}

	nl();

	{
		const auto cam = curr_movie.cam1;
		int y2 = y;
		heading();
		pr(monospace, 200, y2+=H, "RecCam A:");
		text();
		pr(monospace, 200, y2+=H, "pos x = %9.5f", cam.x);
		pr(monospace, 200, y2+=H, "pos y = %9.5f", cam.y);
		pr(monospace, 200, y2+=H, "pos z = %9.5f", cam.z);
		pr(monospace, 200, y2+=H, "rot x = %9.5f", cam.rotx);
		pr(monospace, 200, y2+=H, "rot y = %9.5f", cam.roty);
		pr(monospace, 200, y2+=H, "rot z = %9.5f", cam.rotz);
		pr(monospace, 200, y2+=H, "scale = %9.5f", cam.scale);
	}

	heading();
	pr(monospace, 20, y+=H, "Camera A:");
	text();
	pr(monospace, 20, y+=H, "pos x = %9.5f", cal_cam1.curr.x);
	pr(monospace, 20, y+=H, "pos y = %9.5f", cal_cam1.curr.y);
	pr(monospace, 20, y+=H, "pos z = %9.5f", cal_cam1.curr.z);
	pr(monospace, 20, y+=H, "rot x = %9.5f", cal_cam1.curr.rotx);
	pr(monospace, 20, y+=H, "rot y = %9.5f", cal_cam1.curr.roty);
	pr(monospace, 20, y+=H, "rot z = %9.5f", cal_cam1.curr.rotz);
	pr(monospace, 20, y+=H, "scale = %9.5f", cal_cam1.curr.scale);
	nl();

	{
		const auto cam = curr_movie.cam2;
		int y2 = y;
		heading();
		pr(monospace, 200, y2+=H, "RecCam B:");
		text();
		pr(monospace, 200, y2+=H, "pos x = %9.5f", cam.x);
		pr(monospace, 200, y2+=H, "pos y = %9.5f", cam.y);
		pr(monospace, 200, y2+=H, "pos z = %9.5f", cam.z);
		pr(monospace, 200, y2+=H, "rot x = %9.5f", cam.rotx);
		pr(monospace, 200, y2+=H, "rot y = %9.5f", cam.roty);
		pr(monospace, 200, y2+=H, "rot z = %9.5f", cam.rotz);
		pr(monospace, 200, y2+=H, "scale = %9.5f", cam.scale);
	}

	heading();
	pr(monospace, 20, y+=H, "Camera B:");
	text();
	pr(monospace, 20, y+=H, "pos x = %9.5f", cal_cam2.curr.x);
	pr(monospace, 20, y+=H, "pos y = %9.5f", cal_cam2.curr.y);
	pr(monospace, 20, y+=H, "pos z = %9.5f", cal_cam2.curr.z);
	pr(monospace, 20, y+=H, "rot x = %9.5f", cal_cam2.curr.rotx);
	pr(monospace, 20, y+=H, "rot y = %9.5f", cal_cam2.curr.roty);
	pr(monospace, 20, y+=H, "rot z = %9.5f", cal_cam2.curr.rotz);
	pr(monospace, 20, y+=H, "scale = %9.5f", cal_cam2.curr.scale);
	nl();

	pr(monospace, 20, y+=H,
		"#%d Near(%dmm) Far(%dmm) [%s][%s]",
			config.client_number,
			config.near_threshold,
			config.far_threshold,
			mode.borderline ? "border" : "no border",
			mode.auto_clipping ? "auto clipping" : "no auto clip");

	// @fps
	pr(monospace, 20, y+=H, "%d, %.2ffps",
			frames,
			fps_counter.getFps());
	nl();

	// @profile
	heading();
	pr(monospace, 20, y+=H, "Profile:");
	text();
	b(); pr(monospace, 20, y+=H, "Frame         %7.3fms/frame", time_profile.frame);

	b(); pr(monospace, 20, y+=H, " Environment  %6.2f", time_profile.environment.total);
	p(); pr(monospace, 20, y+=H, "  read1       %6.2f", time_profile.environment.read1);
	p(); pr(monospace, 20, y+=H, "  read2       %6.2f", time_profile.environment.read2);

	b(); pr(monospace, 20, y+=H, " Drawing      %6.2f", time_profile.drawing.total);
	p(); pr(monospace, 20, y+=H, "  grid        %6.2f", time_profile.drawing.grid);
	p(); pr(monospace, 20, y+=H, "  wall        %6.2f", time_profile.drawing.wall);
	p(); pr(monospace, 20, y+=H, "  mix1        %6.2f", time_profile.drawing.mix1);
	p(); pr(monospace, 20, y+=H, "  mix2        %6.2f", time_profile.drawing.mix2);
	p(); pr(monospace, 20, y+=H, "  draw        %6.2f", time_profile.drawing.drawvoxels);
	
	b(); pr(monospace, 20, y+=H, " Atari        %6.2f", time_profile.atari);
	
	b(); pr(monospace, 20, y+=H, " Recording    %6.2f [%d]", time_profile.record.total, curr_movie.total_frames);
	p(); pr(monospace, 20, y+=H, "  enc_stage1  %6.2f", time_profile.record.enc_stage1);
	p(); pr(monospace, 20, y+=H, "  enc_stage2  %6.2f", time_profile.record.enc_stage2);
	p(); pr(monospace, 20, y+=H, "  enc_stage3  %6.2f", time_profile.record.enc_stage3);

	b(); pr(monospace, 20, y+=H, " Playback     %6.2f [%d]", time_profile.playback.total, movie_index);
	p(); pr(monospace, 20, y+=H, "  dec_stage1  %6.2f", time_profile.playback.dec_stage1);
	p(); pr(monospace, 20, y+=H, "  dec_stage2  %6.2f", time_profile.playback.dec_stage2);
	p(); pr(monospace, 20, y+=H, "  draw        %6.2f", time_profile.playback.draw);
}
