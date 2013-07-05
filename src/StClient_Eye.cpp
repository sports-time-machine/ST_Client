#include "StClient.h"
#include <gl/glu.h>

using namespace stclient;


void Eye::gluLookAt()
{
	const float eye_depth = LOOKAT_EYE_DEPTH;
	const float ex = x + cos(rh) * eye_depth;
	const float ez = z + sin(rh) * eye_depth;
	const float ey = y + v;

	::gluLookAt(x,y,z, ex,ey,ez, 0,1,0);
}

void Eye::view_2d_left()
{
	global.view.is_2d_view = true;
	global.view.view2d.width = GROUND_DEPTH * 1.5f;
	global.view_mode = VM_2D_LEFT;
	set(-10.0f, -0.20f, -GROUND_DEPTH/2, 0.0f, 0.0f);
}

void Eye::view_2d_top()
{
	global.view.is_2d_view = true;
	global.view.view2d.width = GROUND_WIDTH * 1.1f;
	global.view_mode = VM_2D_TOP;
	set(0.0f, 10.0, 0.8f, -PI/2, -100.0f);
}

void Eye::view_2d_front()
{
	global.view.is_2d_view = true;
	global.view.view2d.width = GROUND_WIDTH * 1.1f;  // è≠ÇµçLÇ≠
	global.view_mode = VM_2D_FRONT;
	set(0.0f, -0.3f, 10.0f, -PI/2, 0.0f);
}

void Eye::view_2d_run()
{
	global.view.is_2d_view = true;
	global.view.view2d.width = GROUND_WIDTH;
	global.view_mode = VM_2D_RUN;
	set(0.0f, -0.4f, 5.0f, -PI/2, 0.0f);
}

void Eye::view_3d_left()
{
	global.view.is_2d_view = false;
	global.view_mode = VM_3D_LEFT;
	set(-2.9f, 1.5f, 3.6f, -1.03f, -0.82f);
}

void Eye::view_3d_right()
{
	global.view.is_2d_view = false;
	global.view_mode = VM_3D_RIGHT;
	set(2.9f, 1.5f, 3.6f, -2.11f, -0.82f);
}

void Eye::view_3d_front()
{
	global.view.is_2d_view = false;
	global.view_mode = VM_3D_FRONT;
	set(0.0f, 1.5f, 4.50f, -PI/2, -0.60f);
}
