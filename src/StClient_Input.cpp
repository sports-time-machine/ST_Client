#include "ST_Client.h"
#include <gl/glfw.h>

using namespace mgl;
using namespace stclient;

bool narashi = false;

// �u�L�����u���[�V�����v�Ɓu�����v���[�h�����̓���
void StClient::processKeyInput_BothMode(const bool* down)
{
	if (down[VK_TAB])
	{
		dev1.updateFloorDepth();
		dev2.updateFloorDepth();
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

	// ADSW move, QE, PageUp/Down
	const bool A = down['A'];
	const bool D = down['D'];
	const bool S = down['S'];
	const bool W = down['W'];
	const bool Q = down['Q'];
	const bool E = down['E'];
	const bool PU = down[VK_NEXT];
	const bool PD = down[VK_PRIOR];
	const bool shift     = down[VK_SHIFT];
	const float movespeed = shift ? 0.01 : 0.1;
	if (A || D || S || W || Q || E || PD || PU)
	{
		auto eye_move_xz = [&](float rad){
			eye.x += movespeed * cosf(eye.rh - rad*PI/180);
			eye.z += movespeed * sinf(eye.rh - rad*PI/180);
		};
		auto eye_move_yv = [&](float sign){
			eye.y += sign * movespeed;
			eye.v -= sign * movespeed;
		};
		auto eye_move_y = [&](float sign){
			eye.y += sign * movespeed;
		};

		if (A)  eye_move_xz( 90.0f);
		if (D)  eye_move_xz(270.0f);
		if (S)  eye_move_xz(180.0f);
		if (W)  eye_move_xz(  0.0f);
		if (Q)  eye_move_yv( -1.0f);
		if (E)  eye_move_yv( +1.0f);
		if (PD) eye_move_y ( +1.0f);
		if (PU) eye_move_y ( -1.0f);
		return;
	}
}

void StClient::processKeyInput_RunMode(const bool* down)
{
	if (global_config.auto_snapshot_interval>0)
	{
		static int frame_count;
		++frame_count;
		if (frame_count%global_config.auto_snapshot_interval==0)
		{
			this->createSnapshot();
			return;
		}
	}
}

void StClient::processKeyInput_CalibrateMode(const bool* down)
{
	const bool shift     = down[VK_SHIFT];
	const bool key_left  = down[VK_LEFT ];
	const bool key_right = down[VK_RIGHT];
	const bool key_up    = down[VK_UP   ];
	const bool key_down  = down[VK_DOWN ];
//#	const float movespeed = shift ? 0.01 : 0.1;

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
}

void StClient::startMovieRecordSettings()
{
	global.gameinfo.movie.clear();
}

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


	processKeyInput_BothMode(down);

	if (global.calibrating_now())
	{
		// ONLY calibrating
		processKeyInput_CalibrateMode(down);
	}
	else
	{
		// ONLY *NOT* calibrating
		processKeyInput_RunMode(down);
	}




	enum { SK_SHIFT=0x10000 };
	enum { SK_CTRL =0x20000 };

	// A           'S'
	// Shift+A     'A' | SHIFT
	// F1          F1
	// Shift+F1    F1 | SHIFT
	key += (down[VK_SHIFT]   ? SK_SHIFT : 0);
	key += (down[VK_CONTROL] ? SK_CTRL  : 0);

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
		break;
	case VK_END:
		break;

	case 'P':
		global.show_debug_info = !global.show_debug_info;
		break;
	case 'I':
		eye.fast_set = !eye.fast_set;
		break;

	case VK_ESCAPE:
		dev1.depth.stop();
		dev1.color.stop();
		dev1.depth.destroy();
		dev1.color.destroy();
		dev1.device.close();
		openni::OpenNI::shutdown();
		exit(1);
#if 0//#!
	case SK_CTRL + VK_F1:
		if (movie_mode!=MOVIE_RECORD)
		{
			startMovieRecordSettings();
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
		}
		break;
#endif
	case SK_CTRL + VK_F1:
		recordingStart();
		break;
	case SK_CTRL + VK_F2:
		recordingReplay();
		break;

	case VK_F9:
		load_config();
		reloadResources();
		break;

	case SK_CTRL | 'C':
		set_clipboard_text();
		break;

	case 'm':  toggle(mode.mixed_enabled);    break;
	case 'M':  toggle(mode.mirroring);        break;

	case VK_BACK:
		dev1.clearFloorDepth();
		break;
	case VK_RETURN:
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

		// ���ݒl��ۑ����Ă���
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

		const float mx = (mouse.diff.x) * 0.01f * (shift ? 0.1f : 1.0f);
		const float my = (mouse.diff.y) * 0.01f * (shift ? 0.1f : 1.0f);
		do_calibration(mx, my);
	}

	// �L�����u���[�V�����̂Ƃ��͎��_�ړ����ł��Ȃ�
	switch (global.view_mode)
	{
	case VM_2D_TOP:
	case VM_2D_LEFT:
	case VM_2D_FRONT:
		//move_eye = false;
		break;
	case VM_2D_RUN:
		// �Q�[����ʓ��{����OK
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
