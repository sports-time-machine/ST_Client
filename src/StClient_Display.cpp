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


	glRGBA(255,0,0)();
	gl::DrawSphere(
		global.person_center_x,
		global.person_center_y,
		0.0f,
		0.25f);
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
	int x = 0;
	int y = 10;

	auto nl = [&](){ y+=H/2; };
	auto pr = freetype::print;

	const glRGBA h1_  = global_config.color.text_h1;
	const glRGBA text = global_config.color.text_p;
	const glRGBA em   = global_config.color.text_em;
	const glRGBA dt   = global_config.color.text_dt;
	const glRGBA dd   = global_config.color.text_dd;

	auto color = [](bool status){
		(status
			? global_config.color.text_em
			: global_config.color.text_p)();
	};
	auto h1 = [&](const char* s){
		global_config.color.text_h1();
		freetype::print(monospace, x, y+=H, s);
		text();	
	};

	{
		ChangeCalParamKeys keys;
		keys.init();
		em();
		pr(monospace, 320, y,
			(keys.rot_xy) ? "<XY-rotation>" :
			(keys.rot_z)  ? "<Z-rotation>" :
			(keys.scale)  ? "<Scaling>" : "");
	}

	{
		const int saved = y;
		x = 200;
		h1("EYE");
		pr(monospace, x, y+=H, "x =%+9.4f [adsw]", eye.x);
		pr(monospace, x, y+=H, "y =%+9.4f [q/e]", eye.y);
		pr(monospace, x, y+=H, "z =%+9.4f [adsw]", eye.z);
		pr(monospace, x, y+=H, "rh=%+9.4f(rad)", eye.rh);
		pr(monospace, x, y+=H, "v =%+9.4f [q/e]", eye.v);
		y+=H;
		pr(monospace, x, y+=H, "P-inc = %3d [g/h]", config.person_inc);
		pr(monospace, x, y+=H, "M-inc = %3d [n/m]", config.movie_inc);
		y = saved;
	}

	{
		x = 20;
		h1("View Mode");
		const auto vm = global.view_mode;
		color(vm==VM_2D_LEFT);  pr(monospace, x, y+=H, "[F1] 2D left");
		color(vm==VM_2D_TOP);   pr(monospace, x, y+=H, "[F2] 2D top");
		color(vm==VM_2D_FRONT); pr(monospace, x, y+=H, "[F3] 2D front");
		color(false);           pr(monospace, x, y+=H, "[F4] ----");
		color(vm==VM_2D_RUN);   pr(monospace, x, y+=H, "[F5] 2D run");
		color(vm==VM_3D_LEFT);  pr(monospace, x, y+=H, "[F6] 3D left");
		color(vm==VM_3D_RIGHT); pr(monospace, x, y+=H, "[F7] 3D right");
		color(vm==VM_3D_FRONT); pr(monospace, x, y+=H, "[F8] 3D front");
	}

	nl();

	{
		const int saved = y;
		x = 200;
		const auto cam = curr_movie.cam1;
		h1("RecCam A:");
		pr(monospace, x, y+=H, "pos x = %9.5f", cam.x);
		pr(monospace, x, y+=H, "pos y = %9.5f", cam.y);
		pr(monospace, x, y+=H, "pos z = %9.5f", cam.z);
		pr(monospace, x, y+=H, "rot x = %9.5f", cam.rotx);
		pr(monospace, x, y+=H, "rot y = %9.5f", cam.roty);
		pr(monospace, x, y+=H, "rot z = %9.5f", cam.rotz);
		pr(monospace, x, y+=H, "scale = %9.5f", cam.scale);
		y = saved;
	}

	{
		x = 20;
		h1("Camera A:");
		pr(monospace, x, y+=H, "pos x = %9.5f", cal_cam1.curr.x);
		pr(monospace, x, y+=H, "pos y = %9.5f", cal_cam1.curr.y);
		pr(monospace, x, y+=H, "pos z = %9.5f", cal_cam1.curr.z);
		pr(monospace, x, y+=H, "rot x = %9.5f", cal_cam1.curr.rotx);
		pr(monospace, x, y+=H, "rot y = %9.5f", cal_cam1.curr.roty);
		pr(monospace, x, y+=H, "rot z = %9.5f", cal_cam1.curr.rotz);
		pr(monospace, x, y+=H, "scale = %9.5f", cal_cam1.curr.scale);
		nl();
	}

	{
		const int saved = y;
		x = 200;
		const auto cam = curr_movie.cam2;
		h1("RecCam B:");
		pr(monospace, x, y+=H, "pos x = %9.5f", cam.x);
		pr(monospace, x, y+=H, "pos y = %9.5f", cam.y);
		pr(monospace, x, y+=H, "pos z = %9.5f", cam.z);
		pr(monospace, x, y+=H, "rot x = %9.5f", cam.rotx);
		pr(monospace, x, y+=H, "rot y = %9.5f", cam.roty);
		pr(monospace, x, y+=H, "rot z = %9.5f", cam.rotz);
		pr(monospace, x, y+=H, "scale = %9.5f", cam.scale);
		y = saved;
	}

	{
		x = 20;
		h1("Camera B:");
		pr(monospace, x, y+=H, "pos x = %9.5f", cal_cam2.curr.x);
		pr(monospace, x, y+=H, "pos y = %9.5f", cal_cam2.curr.y);
		pr(monospace, x, y+=H, "pos z = %9.5f", cal_cam2.curr.z);
		pr(monospace, x, y+=H, "rot x = %9.5f", cal_cam2.curr.rotx);
		pr(monospace, x, y+=H, "rot y = %9.5f", cal_cam2.curr.roty);
		pr(monospace, x, y+=H, "rot z = %9.5f", cal_cam2.curr.rotz);
		pr(monospace, x, y+=H, "scale = %9.5f", cal_cam2.curr.scale);
		nl();
	}

	pr(monospace, 20, y+=H,
		"#%d [%s][%s] (%.5f,%.5f)",
			config.client_number,
			mode.borderline ? "border" : "no border",
			mode.auto_clipping ? "auto clipping" : "no auto clip",
			global.person_center_x,
			global.person_center_y);

	// @fps
	pr(monospace, 20, y+=H, "%d, %.2ffps",
			frames,
			fps_counter.getFps());
	nl();

	// @profile
	x = 20;
	h1("Profile:");
	dt(); pr(monospace, x, y+=H, "Frame         %7.3fms/frame", time_profile.frame);

	dt(); pr(monospace, x, y+=H, " Environment  %6.2f", time_profile.environment.total);
	dd(); pr(monospace, x, y+=H, "  read1       %6.2f", time_profile.environment.read1);
	dd(); pr(monospace, x, y+=H, "  read2       %6.2f", time_profile.environment.read2);

	dt(); pr(monospace, x, y+=H, " Drawing      %6.2f", time_profile.drawing.total);
	dd(); pr(monospace, x, y+=H, "  grid        %6.2f", time_profile.drawing.grid);
	dd(); pr(monospace, x, y+=H, "  wall        %6.2f", time_profile.drawing.wall);
	dd(); pr(monospace, x, y+=H, "  mix1        %6.2f", time_profile.drawing.mix1);
	dd(); pr(monospace, x, y+=H, "  mix2        %6.2f", time_profile.drawing.mix2);
	dd(); pr(monospace, x, y+=H, "  draw        %6.2f", time_profile.drawing.drawvoxels);
	
	dt(); pr(monospace, x, y+=H, " Atari        %6.2f", time_profile.atari);
	
	dt(); pr(monospace, x, y+=H, " Recording    %6.2f [%d]", time_profile.record.total, curr_movie.total_frames);
	dd(); pr(monospace, x, y+=H, "  enc_stage1  %6.2f", time_profile.record.enc_stage1);
	dd(); pr(monospace, x, y+=H, "  enc_stage2  %6.2f", time_profile.record.enc_stage2);
	dd(); pr(monospace, x, y+=H, "  enc_stage3  %6.2f", time_profile.record.enc_stage3);

	dt(); pr(monospace, x, y+=H, " Playback     %6.2f [%d]", time_profile.playback.total, movie_index);
	dd(); pr(monospace, x, y+=H, "  dec_stage1  %6.2f", time_profile.playback.dec_stage1);
	dd(); pr(monospace, x, y+=H, "  dec_stage2  %6.2f", time_profile.playback.dec_stage2);
	dd(); pr(monospace, x, y+=H, "  draw        %6.2f", time_profile.playback.draw);
}
