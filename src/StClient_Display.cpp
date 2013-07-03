#include "StClient.h"

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


// 人物の中心とみなしている点の描画
static void DrawCenterOfPerson()
{
	static float inc = 0;
	inc += 1.0f;
	glRGBA(255,0,0)();
	gl::DrawSphere(
		global.person_center.x,
		global.person_center.y,
		global.person_center.z,
		0.25f,
		inc * 2.25f,
		sinf(inc*0.013f),
		cosf(inc*0.009f),
		1.0f);
}

// 単発ムービーの描画
static bool drawMovieFrame(const MovieData& mov, glRGBA inner, glRGBA outer, const char* movie_type)
{
	if (mov.total_frames==0)
	{
		// Empty movie.
		return false;
	}

	if (global.frame_index >= mov.total_frames)
	{
		printf("movie is end. [%s] %d/%d\n",
			movie_type,
			global.frame_index,
			mov.total_frames);

		// ムービーおわり
		return false;
	}

	// 描画用の独立したドット空間、デプスイメージをもっておく
	static RawDepthImage depth1, depth2;
	static Dots dots;
	dots.init();
	
	// 有効なフレームを探す
	int disp_frame = mov.getValidFrame(global.frame_index);
	if (global.frame_index != disp_frame)
	{
		printf("フレーム補正 frame %d => %d\n",  global.frame_index, disp_frame);
	}
	if (disp_frame>=0)
	{
		Depth10b6b::playback(depth1, depth2, mov.frames.find(disp_frame)->second);
		MixDepth(dots, depth1, mov.cam1);
		MixDepth(dots, depth2, mov.cam2);
		drawVoxels(dots, mov.dot_size, inner, outer, DRAW_VOXELS_MOVIE);
	}
	return true;
}

//========================
// 3D描画メイン
//========================
void StClient::display3dSection()
{
	// 　　　　　　 SLEEP START REPLAY OTHER
	// 実映像　       X     X    ---     X     リプレイ以外表示
	// 動画リプレイ  ---   ---    X     --- 
	// 並走表示　　  ---    X     X     ---    スタートとリプレイ
	const auto st = clientStatus();
	auto& gd = global.debug;
	gd.recording      = (st==STATUS_GAME);
	gd.show_realmovie = (st!=STATUS_REPLAY);
	gd.show_replay    = (st==STATUS_REPLAY);
	gd.show_partner   = (st==STATUS_GAME || st==STATUS_REPLAY);


	// 実映像の表示
	if (gd.show_realmovie)
	{
		static Dots dots;
		this->DrawRealMovie(dots, config.person_dot_px);

		// センター座標(@Center)の取得
		this->CreateAtari(dots);
		::DrawCenterOfPerson();
	}

	// 並走者
	if (gd.show_partner)
	{
		drawMovieFrame(
			global.gameinfo.partner1,
			config.color.movie1,
			glRGBA(50,50,50),
			"partner1");
	}

	if (gd.show_replay)
	{
		if (!drawMovieFrame(global.gameinfo.movie, glRGBA(255,0,0), glRGBA(50,200,0), "replay"))
		{
			// リプレイ終わり
			Msg::Notice("End replay");
			changeStatus(STATUS_READY);
		}
	}

	if (gd.recording)
	{
		this->MovieRecord();
	}
}

//========================
// 2D描画するための下準備
//========================
void StClient::display2dSectionPrepare()
{
	gl::Projection();
	gl::LoadIdentity();
	glOrtho(0, 640, 480, 0, -50.0, 50.0);

	gl::Texture(false);
	gl::DepthTest(false);
}

//========================
// 2D描画メイン
//========================
void StClient::display2dSection()
{
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

	if (global.show_debug_info)
	{
		// 当たり判定マトリクスの表示
		glBegin(GL_QUADS);
		for (int y=0; y<HitData::CEL_H; ++y)
		{
			for (int x=0; x<HitData::CEL_W; ++x)
			{
				int hit = hitdata.get(x,y);
				int p = minmax(hit*ATARI_INC/5, 0, 255);
				int q = 255-p;
				glRGBA(
					(180*p +  90*q)>>8,
					( 70*p + 110*q)>>8,
					( 30*p + 130*q)>>8,
					(240*p +  50*q)>>8).glColorUpdate();
				const int DOTSIZE = 5;
				const int V = DOTSIZE-1;
				const int M = 10;
				const int dx = x*DOTSIZE + 640 - M - HitData::CEL_W*DOTSIZE;
				const int dy = y*DOTSIZE +   0 + M;
				glVertex2i(dx,   dy);
				glVertex2i(dx+V, dy);
				glVertex2i(dx+V, dy+V);
				glVertex2i(dx,   dy+V);
			}
		}
		glEnd();
	}

	if (global.show_debug_info)
	{
		// 当たり判定オブジェクトの表示
		glBegin(GL_QUADS);
		for (size_t i=0; i<global.hit_objects.size(); ++i)
		{
			const HitObject& ho = global.hit_objects[i];
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

	// ゲーム中のみ「当たり判定」
	if (clientStatus()==STATUS_GAME)
	{
		for (size_t i=0; i<global.hit_objects.size(); ++i)
		{
			HitObject& ho = global.hit_objects[i];
			int value = hitdata.get(ho.point.x, ho.point.y);
		
			// 10cm3にNドット以上あったらヒットとする
			if (value>=config.hit_threshold)
			{
				string s;
				s += "HIT ";
				s += ho.text;
				s += " ";
				s += to_s(ho.next_id);
				udp_send.send(s);

				printf("HIT!! hit object %d, point (%d,%d)\n",
					i,
					ho.point.x,
					ho.point.y);
				this->flashing = 100;
				global.hit_stage = ho.next_id;
				global.hit_objects.clear();

				this->createSnapshot();
				break;
			}
		}
	}
}

void StClient::displayDebugInfo()
{
	const int H=15;
	int x = 0;
	int y = 10;

	auto nl = [&](){ y+=H/2; };
	auto pr = freetype::print;

	const glRGBA h1_  = config.color.text_h1;
	const glRGBA text = config.color.text_p;
	const glRGBA em   = config.color.text_em;
	const glRGBA dt   = config.color.text_dt;
	const glRGBA dd   = config.color.text_dd;

	auto color = [](bool status){
		(status
			? config.color.text_em
			: config.color.text_p)();
	};
	auto h1 = [&](const char* s){
		config.color.text_h1();
		freetype::print(monospace, x, y+=H, s);
		text();	
	};

	{
		text();
		pr(monospace, 700, 300, "[%s][%s][%s][%s][%s]",
			(global.debug.recording)      ? "recoding" : "-",
			(global.debug.show_realmovie) ? "realmovie"  : "",
			(global.debug.show_replay)    ? "replay" : "",
			(global.debug.show_partner)   ? "partner"  : "",
			getStatusName());
		pr(monospace, 700, 320, "[Atari=%5d]",
			global.debug.atari_voxels);
	}

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
		color(false);           pr(monospace, x, y+=H, "[F4] ChangeSpeed");
		color(vm==VM_2D_RUN);   pr(monospace, x, y+=H, "[F5] 2D run");
		color(vm==VM_3D_LEFT);  pr(monospace, x, y+=H, "[F6] 3D left");
		color(vm==VM_3D_RIGHT); pr(monospace, x, y+=H, "[F7] 3D right");
		color(vm==VM_3D_FRONT); pr(monospace, x, y+=H, "[F8] 3D front");
	}

	nl();

	{
		const int saved = y;
		x = 200;
		h1("Camera B:");
		(active_camera==CAM_B) ? em() : text();
		pr(monospace, x, y+=H, "pos x = %9.5f", cal_cam2.curr.pos.x);
		pr(monospace, x, y+=H, "pos y = %9.5f", cal_cam2.curr.pos.y);
		pr(monospace, x, y+=H, "pos z = %9.5f", cal_cam2.curr.pos.z);
		pr(monospace, x, y+=H, "rot x = %9.5f", cal_cam2.curr.rot.x);
		pr(monospace, x, y+=H, "rot y = %9.5f", cal_cam2.curr.rot.y);
		pr(monospace, x, y+=H, "rot z = %9.5f", cal_cam2.curr.rot.z);
		pr(monospace, x, y+=H, "scl x = %9.5f", cal_cam1.curr.scale.x);
		pr(monospace, x, y+=H, "scl y = %9.5f", cal_cam1.curr.scale.y);
		pr(monospace, x, y+=H, "scl z = %9.5f", cal_cam1.curr.scale.z);
		nl();
		y = saved;
	}

	{
		x = 20;
		h1("Camera A:");
		(active_camera==CAM_A) ? em() : text();
		pr(monospace, x, y+=H, "pos x = %9.5f", cal_cam1.curr.pos.x);
		pr(monospace, x, y+=H, "pos y = %9.5f", cal_cam1.curr.pos.y);
		pr(monospace, x, y+=H, "pos z = %9.5f", cal_cam1.curr.pos.z);
		pr(monospace, x, y+=H, "rot x = %9.5f", cal_cam1.curr.rot.x);
		pr(monospace, x, y+=H, "rot y = %9.5f", cal_cam1.curr.rot.y);
		pr(monospace, x, y+=H, "rot z = %9.5f", cal_cam1.curr.rot.z);
		pr(monospace, x, y+=H, "scl x = %9.5f", cal_cam1.curr.scale.x);
		pr(monospace, x, y+=H, "scl y = %9.5f", cal_cam1.curr.scale.y);
		pr(monospace, x, y+=H, "scl z = %9.5f", cal_cam1.curr.scale.z);
		nl();
	}

	text();
	pr(monospace, 20, y+=H,
		"#%d [frame-index:%d](%s)[hit-id:%d][%s] [%s] [fl=%d] (%.5f,%.5f)",
			config.client_number,
			global.frame_index,
			global.frame_auto_increment ? "auto" : "-",
			global.hit_stage,
			global.hit_objects.size() ? global.hit_objects[0].text.c_str() : "-",
			eye.fast_set ? "fast" : "slow",
			flashing,
			global.person_center.x,
			global.person_center.y);
	pr(monospace, 20, y+=H,
		"TOTAL [%d frames] [WVTac=%d][WVT=%d][auto-CF=%d]",
			global.total_frames,
			global.atari_count,
			config.whitemode_voxel_threshould,
			global.auto_clear_floor_count);

	// @fps
	pr(monospace, 20, y+=H, "%.2ffps", fps_counter.getFps());
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
	
	const bool REC = recordingNow();
	REC?em():dt(); pr(monospace, x, y+=H, " Recording    %6.2f [%d]", time_profile.record.total, global.gameinfo.movie.frames.size());
	REC?em():dd(); pr(monospace, x, y+=H, "  enc_stage1  %6.2f", time_profile.record.enc_stage1);
	REC?em():dd(); pr(monospace, x, y+=H, "  enc_stage2  %6.2f", time_profile.record.enc_stage2);
	REC?em():dd(); pr(monospace, x, y+=H, "  enc_stage3  %6.2f", time_profile.record.enc_stage3);

	const bool PLAY = replayingNow();
	PLAY?em():dt(); pr(monospace, x, y+=H, " Playback     %6.2f [%d]", time_profile.playback.total, global.frame_index);
	PLAY?em():dd(); pr(monospace, x, y+=H, "  dec_stage1  %6.2f", time_profile.playback.dec_stage1);
	PLAY?em():dd(); pr(monospace, x, y+=H, "  dec_stage2  %6.2f", time_profile.playback.dec_stage2);
	PLAY?em():dd(); pr(monospace, x, y+=H, "  draw        %6.2f", time_profile.playback.draw);
}
