#include "ST_Client.h"

using namespace stclient;
using namespace mgl;

void StClient::display2()
{
	static float cnt = 0;
	static int frames = 0;
	++frames;
	cnt += 0.01;
	glPushMatrix();
	glLoadIdentity();

	glRGBA::white.glColorUpdate();

	const int H=15;
	int y = 10;

	auto nl = [&](){ y+=H/2; };
	auto pr = freetype::print;

	glRGBA heading(80,255,120);
	glRGBA text(200,200,200);
	glRGBA b(240,240,240);
	glRGBA p(150,150,150);
	
	auto color = [](bool status){
		status
			? glRGBA(240,200,50).glColorUpdate()
			: glRGBA(200,200,200).glColorUpdate();
	};


	
	{
		ChangeCalParamKeys keys;
		keys.init();
		heading.glColorUpdate();
		pr(monospace, 320, y,
			(keys.rot_xy) ? "<XY-rotation>" :
			(keys.rot_z)  ? "<Z-rotation>" :
			(keys.scale)  ? "<Scaling>" : "");
	}


	{
		int y2 = y;
		heading.glColorUpdate();
		pr(monospace, 200, y2+=H, "EYE");
		text.glColorUpdate();
		pr(monospace, 200, y2+=H, "x =%+9.4f [adsw]", eye.x);
		pr(monospace, 200, y2+=H, "y =%+9.4f [q/e]", eye.y);
		pr(monospace, 200, y2+=H, "z =%+9.4f [adsw]", eye.z);
		pr(monospace, 200, y2+=H, "rh=%+9.4f(rad)", eye.rh);
		pr(monospace, 200, y2+=H, "v =%+9.4f [q/e]", eye.v);
		y2+=H;
		pr(monospace, 200, y2+=H, "P-inc = %3d [g/h]", config.person_inc);
		pr(monospace, 200, y2+=H, "M-inc = %3d [n/m]", config.movie_inc);
	}

	heading.glColorUpdate();
	pr(monospace, 20, y+=H, "View Mode");
	text.glColorUpdate();
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
		heading.glColorUpdate();
		pr(monospace, 200, y2+=H, "RecCam A:");
		text.glColorUpdate();
		pr(monospace, 200, y2+=H, "pos x = %9.5f", cam.x);
		pr(monospace, 200, y2+=H, "pos y = %9.5f", cam.y);
		pr(monospace, 200, y2+=H, "pos z = %9.5f", cam.z);
		pr(monospace, 200, y2+=H, "rot x = %9.5f", cam.rotx);
		pr(monospace, 200, y2+=H, "rot y = %9.5f", cam.roty);
		pr(monospace, 200, y2+=H, "rot z = %9.5f", cam.rotz);
		pr(monospace, 200, y2+=H, "scale = %9.5f", cam.scale);
	}

	heading.glColorUpdate();
	pr(monospace, 20, y+=H, "Camera A:");
	text.glColorUpdate();
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
		heading.glColorUpdate();
		pr(monospace, 200, y2+=H, "RecCam B:");
		text.glColorUpdate();
		pr(monospace, 200, y2+=H, "pos x = %9.5f", cam.x);
		pr(monospace, 200, y2+=H, "pos y = %9.5f", cam.y);
		pr(monospace, 200, y2+=H, "pos z = %9.5f", cam.z);
		pr(monospace, 200, y2+=H, "rot x = %9.5f", cam.rotx);
		pr(monospace, 200, y2+=H, "rot y = %9.5f", cam.roty);
		pr(monospace, 200, y2+=H, "rot z = %9.5f", cam.rotz);
		pr(monospace, 200, y2+=H, "scale = %9.5f", cam.scale);
	}

	heading.glColorUpdate();
	pr(monospace, 20, y+=H, "Camera B:");
	text.glColorUpdate();
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
	heading.glColorUpdate();
	pr(monospace, 20, y+=H, "Profile:");
	text.glColorUpdate();
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


	glPopMatrix();
}

