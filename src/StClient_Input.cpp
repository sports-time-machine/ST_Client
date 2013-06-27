#include "StClient.h"
#include <gl/glfw.h>

using namespace mgl;
using namespace stclient;


void toggle(bool& ref, const char* s)
{
	ref = !ref;
	Msg::Notice(s, mi::boolToYesNo(ref));
}


#include <FreeImage.h>

static void quitApplication()
{
	puts("QUIT!!");
	exit(0);
}

static void saveScreenShot()
{
	FIBITMAP* bmp = FreeImage_Allocate(640, 480, 24);

	// バックバッファを読む
	glReadBuffer(GL_BACK);

	// バッファの内容を
	// bmpオブジェクトのピクセルデータが格納されている領域に直接コピーする。
#if 0 
	glReadPixels(0, 0, 640, 480, GL_BGRA, GL_UNSIGNED_BYTE, 
			GL_BGRA,           //取得したい色情報の形式
			GL_UNSIGNED_BYTE,  //読み取ったデータを保存する配列の型
			bmpData.Scan0      //ビットマップのピクセルデータ（実際にはバイト配列）へのポインタ
			);
#endif

	FreeImage_Save(FIF_PNG, bmp, "C:/ST/picture.png");


	FreeImage_Unload(bmp);
}






enum
{
	SK_SHIFT=0x10000,
	SK_CTRL =0x20000,
	SK_ALT  =0x40000,
};

static float getCalibrationSpeed(bool shift)
{
	const float base = (global.calibration.fast) ? 0.1f : 0.01f;
	return base * (shift ? 2.5f : 1.0f);
}

void StClient::startMovieRecordSettings()
{
	global.gameinfo.movie.clear();
}

void StClient::processKeyInput()
{
	static bool press[256] = {};
	static bool down[256]  = {};
	int key = 0;

	{
		const int KEYS = 256;
		static bool prev[KEYS] = {};
		BYTE curr_kbd[KEYS] = {};
		myGetKeyboardState(curr_kbd);
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

	if (down[VK_F12])
	{
		dev1.updateFloorDepth();
		dev2.updateFloorDepth();
		return;
	}

	// いつでも
	if (key!=0)
	{
		key += (down[VK_SHIFT  ] ? SK_SHIFT : 0);
		key += (down[VK_CONTROL] ? SK_CTRL  : 0);
		key += (down[VK_MENU   ] ? SK_ALT   : 0);
	}
	switch (key)
	{
	case VK_ESCAPE:
		if (!global.calibration.enabled)
		{
			Msg::Notice("キャリブレーションモードへの移行");
			global.calibration.enabled = true;
			global.show_debug_info     = true;
		}
		else
		{
			Msg::Notice("キャリブレーションモードの終了");
			global.calibration.enabled = false;
			global.show_debug_info     = false;
		}
		return;
	case '0':            saveScreenShot();  return;
	case VK_TAB:         toggle(global.calibration.fast, "高速キャリブレーションモード");   return;
	case 'P':            toggle(global.show_debug_info, "デバッグ情報表示");               return;
	case VK_F9:          load_config(); reloadResources();         return;
	case SK_CTRL|VK_F4:  quitApplication(); return;
	case SK_ALT |VK_F4:  quitApplication(); return;
	}

	if (global.calibration.enabled)
	{
		if (processKeyInput_Calibration(key))
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
	const bool A     = down['A'];
	const bool D     = down['D'];
	const bool S     = down['S'];
	const bool W     = down['W'];
	const bool Q     = down['Q'];
	const bool E     = down['E'];
	const bool PU    = down[VK_NEXT];
	const bool PD    = down[VK_PRIOR];
	const bool shift = down[VK_SHIFT];
	const float movespeed = getCalibrationSpeed(shift);
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

	if (global.calibrating_now())
	{
		// ONLY calibrating
		const bool shift     = down[VK_SHIFT];
		const bool key_left  = down[VK_LEFT ];
		const bool key_right = down[VK_RIGHT];
		const bool key_up    = down[VK_UP   ];
		const bool key_down  = down[VK_DOWN ];

		if (key_left || key_right || key_up || key_down)
		{
			const float U = shift ? 0.004 : 0.001;
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
}

bool StClient::processKeyInput_Calibration(int key)
{
	switch (key)
	{
	case 0:
		// nop
		return false;
	default:
		printf("Key: %X\n", key);
		return false;

	case '1':    active_camera=CAM_A;     break;
	case '2':    active_camera=CAM_B;     break;
	case '3':    active_camera=CAM_BOTH;  break;
	case VK_F1:  eye.view_2d_left();      break;
	case VK_F2:  eye.view_2d_top();       break;
	case VK_F3:  eye.view_2d_front();     break;
	case VK_F5:  eye.view_2d_run();       break;
	case VK_F6:  eye.view_3d_left();      break;
	case VK_F7:  eye.view_3d_right();     break;
	case VK_F8:  eye.view_3d_front();     break;

	case VK_F12:         /* init-floor */                          break;
	case VK_HOME:                                                  break;
	case VK_END:                                                   break;
	case 'I':            toggle(eye.fast_set,   "視点高速移動");    break;
	case 'M':            toggle(mode.mirroring, "ミラー");         break;
	case SK_CTRL | 'C':  set_clipboard_text();                     break;
	case VK_BACK:        this->clearFloorDepth();                  break;
	case VK_RETURN:      gl::ToggleFullScreen();                   break;

	case SK_CTRL + 'S'://Ctrl+S
		if (!recordingNow())
		{
			Msg::Notice("録画スタート!!");
			global.frame_auto_increment = true;
			global.frame_index = 0;
			recordingStart();
		}
		else
		{
			Msg::Notice("録画終了。");
			global.frame_auto_increment = false;
			global.frame_index = 0;
			recordingStop();
		}
		break;
	case SK_CTRL + 'T'://Ctrl+T
		global.gameinfo.prepareForSave("0000ABCD", "00000QWERT");
		global.gameinfo.save();
		break;
	case SK_CTRL + 'L'://Ctrl+L
		global.gameinfo.movie.load("0000099N3B");
		break;
	case SK_CTRL + 'K'://Ctrl+K
		global.gameinfo.partner1.load("0000099N3B");
		break;
	case SK_CTRL + 'R'://Ctrl+R
		Msg::Notice("リプレイ!!");
		global.frame_auto_increment = true;
		global.frame_index = 0;
		recordingReplay();
		break;

#if 0
	case SK_CTRL + 'L'://Ctrl+L
		global.frame_auto_increment = false;
		global.frame_index = 0;
		global.gameinfo.partner1.load("0000054321");
		break;

	case 'J':
		global.gameinfo.movie.run_id = "0000054321";
		global.gameinfo.save();
		break;
#endif
	}
	return true;
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
	myGetKeyboardState(kbd);

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

		const float spd = getCalibrationSpeed(shift);
		const float mx = (mouse.diff.x) * spd;
		const float my = (mouse.diff.y) * spd;
		do_calibration(mx, my);
	}

	// 視点移動
	if (move_eye)
	{
		const float x_move = mouse.diff.x * 0.001f;
		const float y_move = mouse.diff.y * 0.005f;
		eye.rh -= x_move;
		eye. v += y_move;
		
		if (!shift)
		{
			eye. y = eye_y_base  - y_move;
		}
	}
}
