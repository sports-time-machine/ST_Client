#include "ST_Client.h"
#include <gl/glfw.h>

using namespace mgl;
using namespace stclient;

void StClient::processKeyInput()
{
	static bool press[256] = {};
	static bool down[256] = {};
	int key = 0;

	{
		const int KEYS = 256;
		static bool prev[KEYS] = {};
		BYTE curr_kbd[KEYS] = {};
		GetKeyboardState(curr_kbd);
		for (int i=0; i<KEYS; ++i)
		{
			down[i] = ((curr_kbd[i] & 0x80)!=0);

			if (!prev[i] && down[i])
			{
				press[i] = true;
				key = i;
			}

			prev[i] = down[i];
		}
	}



	const bool shift     = down[VK_SHIFT];
	const bool ctrl      = down[VK_CONTROL];
	const bool key_left  = down[VK_LEFT ];
	const bool key_right = down[VK_RIGHT];
	const bool key_up    = down[VK_UP   ];
	const bool key_down  = down[VK_DOWN ];
	const float movespeed = shift ? 0.01 : 0.1;

	// ADSW move, QE, PageUp/Down
	const bool A = down['A'];
	const bool D = down['D'];
	const bool S = down['S'];
	const bool W = down['W'];
	const bool Q = down['Q'];
	const bool E = down['E'];
	const bool PU = down[VK_NEXT];
	const bool PD = down[VK_PRIOR];
	if (A || D || S || W || Q || E || PD || PU)
	{
		auto eye_move = [&](float rad){
			eye.x += movespeed * cosf(eye.rh - rad*PI/180);
			eye.z += movespeed * sinf(eye.rh - rad*PI/180);
		};
		auto move_yv = [&](float sign){
			eye.y += sign * movespeed;
			eye.v -= sign * movespeed;
		};
		auto move_y = [&](float sign){
			eye.y += sign * movespeed;
		};

		if (A) eye_move( 90.0f);
		if (D) eye_move(270.0f);
		if (S) eye_move(180.0f);
		if (W) eye_move(  0.0f);
		if (Q) move_yv(-1.0f);
		if (E) move_yv(+1.0f);
		if (PD) move_y(+1.0f);
		if (PU) move_y(-1.0f);
		return;
	}

	// Cursor move
	if (key_left || key_right || key_up || key_down)
	{
		const float U = shift ? 0.001 : 0.01;
		const float mx =
				(key_left  ? -U : 0.0f) +
				(key_right ? +U : 0.0f);
		const float my =
				(key_up   ? -U : 0.0f) +
				(key_down ? +U : 0.0f);
		do_calibration(mx, my);
		return;
	}

	const bool G = down['G'];
	const bool H = down['H'];
	const bool N = down['N'];
	const bool M = down['M'];
	if (G || H || N || M)
	{
		if (G) --config.person_inc;
		if (H) ++config.person_inc;
		if (N) --config.movie_inc;
		if (M) ++config.movie_inc;
		config.person_inc = mi::minmax(config.person_inc, MIN_VOXEL_INC, MAX_VOXEL_INC);
		config.movie_inc  = mi::minmax(config.movie_inc,  MIN_VOXEL_INC, MAX_VOXEL_INC);
		return;
	}



	enum { SK_SHIFT=0x10000 };
	enum { SK_CTRL =0x20000 };


	// A           'S'
	// Shift+A     'A' | SHIFT
	// F1          F1
	// Shift+F1    F1 | SHIFT
	key += (shift ? SK_SHIFT : 0);
	key += (ctrl  ? SK_CTRL  : 0);

	switch (key)
	{
	case '1':
		active_camera = CAM_A;
		break;
	case '2':
		active_camera = CAM_B;
		break;
	case '3':
		active_camera = CAM_BOTH;
		break;

	case VK_F1: eye.view_2d_left();  break;
	case VK_F2: eye.view_2d_top();   break;
	case VK_F3: eye.view_2d_front(); break;
	case VK_F4: break;
	
	case VK_F5: eye.view_2d_run();   break;
	case VK_F6: eye.view_3d_left();  break;
	case VK_F7: eye.view_3d_right(); break;
	case VK_F8: eye.view_3d_front(); break;

	case VK_HOME:
		config.far_threshold -= shift ? 1 : 10;
		break;
	case VK_END:
		config.far_threshold += shift ? 1 : 10;
		break;

	case VK_ESCAPE:
		dev1.depth.stop();
		dev1.color.stop();
		dev1.depth.destroy();
		dev1.color.destroy();
		dev1.device.close();
		openni::OpenNI::shutdown();
		exit(1);
	case 'z':
		global.client_status = STATUS_DEPTH;
		break;
	case SK_CTRL + VK_F1:
		if (movie_mode!=MOVIE_RECORD)
		{
			curr_movie.clear();
			movie_mode = MOVIE_RECORD;
			movie_index = 0;
		}
		else
		{
			printf("recoding stop. %d frames recorded.\n", curr_movie.total_frames);

			size_t total_bytes = 0;
			for (int i=0; i<curr_movie.total_frames; ++i)
			{
			//s	total_bytes += curr_movie.frames[i].getFrameBytes();
			}
			printf("total %d Kbytes.\n", total_bytes/1000);
			movie_mode = MOVIE_READY;
			movie_index = 0;
		}
		break;
	case SK_CTRL + VK_F2:
		printf("playback movie.\n");
		movie_mode = MOVIE_PLAYBACK;
		movie_index = 0;
		break;
	
	case VK_F9:
		load_config();
		reloadResources();
		break;

	case SK_CTRL | 'C':
		set_clipboard_text();
		break;

	case 'C':  toggle(mode.auto_clipping);    break;
	case 'm':  toggle(mode.mixed_enabled);    break;
	case 'M':  toggle(mode.mirroring);        break;
	case 'b':  toggle(mode.borderline);       break;

	case ':':
		clearFloorDepth();
		break;
	case 'X':
		dev1.saveFloorDepth();
		break;
	case 13:
		gl::ToggleFullScreen();
		break;
	}
}



struct MousePos
{
	struct Pos
	{
		int x,y;
	};
	Pos pos,old,diff;

	struct Button
	{
		bool down,press,prev;
	};
	Button left,right;
} mouse;

void StClient::processMouseInput_aux()
{
	static float eye_rh_base, eye_rv_base, eye_y_base;

	if (mouse.right.press)
	{
		puts("RIGHT PRESS");
		eye_rh_base = eye.rh;
		eye_rv_base = eye.v;
		eye_y_base  = eye.y;

		// 現在値を保存しておく
		cal_cam1.prev = cal_cam1.curr;
		cal_cam2.prev = cal_cam2.curr;		
	}
	BYTE kbd[256];
	GetKeyboardState(kbd);

	const bool shift = (kbd[VK_SHIFT  ] & 0x80)!=0;

	bool move_eye = false;

	if (mouse.right.down)
	{
		move_eye = true;
	}
	else if (mouse.left.down)
	{
		// First
		if (mouse.left.press)
		{
			cal_cam1.prev = cal_cam1.curr;
			cal_cam2.prev = cal_cam2.curr;
		}

		printf("%d, %d\n", mouse.diff.x, mouse.diff.y);
		const float mx = (mouse.diff.x) * 0.01f * (shift ? 0.1f : 1.0f);
		const float my = (mouse.diff.y) * 0.01f * (shift ? 0.1f : 1.0f);
		do_calibration(mx, my);
	}

	// キャリブレーションのときは視点移動ができない
	switch (global.view_mode)
	{
	case VM_2D_TOP:
	case VM_2D_LEFT:
	case VM_2D_FRONT:
		//move_eye = false;
		break;
	case VM_2D_RUN:
		// ゲーム画面等倍時はOK
		break;
	}

	if (move_eye)
	{
		const float x_move = mouse.diff.x * 0.0010;
		const float y_move = mouse.diff.y * 0.0050;
		eye.rh -= x_move;
		eye. v += y_move;
		
		if (!shift)
		{
			eye. y = eye_y_base  - y_move;
		}
	}
}

void StClient::processMouseInput()
{
	// Update button
	{
		mouse.left.prev  = mouse.left.down;
		mouse.right.prev = mouse.right.down;

		mouse.left.down   = (glfwGetMouseButton(GLFW_MOUSE_BUTTON_1)==GLFW_PRESS);
		mouse.right.down  = (glfwGetMouseButton(GLFW_MOUSE_BUTTON_2)==GLFW_PRESS);
		mouse.left.press  = (mouse.left.down  && !mouse.left.prev);
		mouse.right.press = (mouse.right.down && !mouse.right.prev);
	}

	// Update position
	{
		// update old mouse pos
		mouse.old = mouse.pos;

		// Convert screen position to internal position
		int x = 0;
		int y = 0;
		glfwGetMousePos(&x, &y);
		mouse.pos.x = x * 640 / global.window_w;
		mouse.pos.y = y * 480 / global.window_h;

		// Diff
		mouse.diff.x = mouse.pos.x - mouse.old.x;
		mouse.diff.y = mouse.pos.y - mouse.old.y;
	}

	// main
	processMouseInput_aux();
}
