#include "St3dData.h"
#include "vec4.h"
#include "ConstValue.h"
#include "mi/Libs.h"
#include <gl/glfw.h>


using namespace mgl;
using namespace stclient;
using namespace vector_and_matrix;


VoxGrafix::Static VoxGrafix::global;


bool VoxGrafix::DrawMovieFrame(const MovieData& mov, const VoxGrafix::DrawParam& param_, int frame_index, glRGBA inner, glRGBA outer, const char* movie_type, DrawStyle style, float add_x, Dots** dots_ref)
{
	if (mov.total_frames==0)
	{
		// Empty movie.
		return false;
	}

	if (frame_index >= mov.total_frames)
	{
		printf("movie is end. [%s] %d/%d\n",
			movie_type,
			frame_index,
			mov.total_frames);

		// ムービーおわり
		return false;
	}

	// 有効なフレームを探す
	int disp_frame = mov.getValidFrame(frame_index);
	if (frame_index != disp_frame)
	{
		fprintf(stderr, "フレーム補正 frame %d => %d\n",  frame_index, disp_frame);
	}
	if (disp_frame>=0)
	{
		// 描画用の独立したドット空間、デプスイメージをもっておく
		static RawDepthImage depth1, depth2;
		switch (mov.ver)
		{
		case MovieData::VER_1_0:
			Depth10b6b::playback(depth1, depth2, mov.frames.find(disp_frame)->second);
			break;
		case MovieData::VER_1_1:
			Depth10b6b_v1_1::playback(depth1, depth2, mov.frames.find(disp_frame)->second);
			break;
		}

		static Dots dots;
		dots.init();
		VoxGrafix::MixDepth(dots, depth1, mov.cam1);
		VoxGrafix::MixDepth(dots, depth2, mov.cam2);
		
		VoxGrafix::DrawParam param = param_;
		param.dot_size = mov.dot_size;
		VoxGrafix::DrawVoxels(dots, param, inner, outer, style);
	
		if (dots_ref!=nullptr)
		{
			*dots_ref = &dots;
		}
	}
	return true;
}

void VoxGrafix::MixDepth(Dots& dots, const RawDepthImage& src, const CamParam& cam)
{
	const mat4x4 trans = mat4x4::create(
			cam.rot.x, cam.rot.y, cam.rot.z,
			cam.pos.x, cam.pos.y, cam.pos.z,
			cam.scale.x, cam.scale.y, cam.scale.z);

	int index = 0;
	for (int y=0; y<480; ++y)
	{
		for (int x=0; x<640; ++x)
		{
			int z = src.image[index++];

			// no depth -- ignore
			if (z==0) continue;

			Point3D p;
			float fx = (320-x)/640.0f;
			float fy = (240-y)/640.0f;
			float fz = z/1000.0f; // milli-meter(mm) to meter(m)

			// -0.5 <= fx <= 0.5
			// -0.5 <= fy <= 0.5
			//  0.0 <= fz <= 10.0  (10m)

			// 四角錐にする
			fx = fx * fz;
			fy = fy * fz;

			// 回転、拡縮、平行移動
			vec4 point = trans * vec4(fx, fy, fz, 1.0f);
			p.x = point[0];
			p.y = point[1];
			p.z = point[2];
			dots.push(p);
		}
	}
}

bool VoxGrafix::DrawVoxels(const Dots& dots, const DrawParam& param, glRGBA inner, glRGBA outer, DrawStyle style)
{
	int& dot_count   = VoxGrafix::global.dot_count;
	int& atari_count = VoxGrafix::global.atari_count;

#if 0
	// Create histogram
	for (int i=0; i<dots.size(); ++i)
	{
		dots[i].
	}
#endif
	for (int i=0; i<dots.size(); ++i)
	{
		const float x = dots[i].x;
		const float y = dots[i].y;
		const float z = dots[i].z;

		const bool in_x = (x>=GROUND_LEFT && x<=GROUND_RIGHT);
		const bool in_y = (y>=0.0f && y<=GROUND_HEIGHT);
		const bool in_z = (z>=0.0f && z<=GROUND_DEPTH);

		if (in_x && in_y)
		{
			++dot_count;
			if (in_z)
			{
				++atari_count;
			}
		}
	}


	// 描画すべきボクセルが少ない場合、描画をとりやめる
	if (param.mute_if_veryfew)
	{
		if (atari_count < param.mute_threshould)
		{
			return false;
		}
	}

	// @voxel @dot
#if 0//#NO QUAD MODE
	const bool quad = false;
	if (quad)
	{
		gl::Texture(true);
		glBindTexture(GL_TEXTURE_2D, global.images.dot);
		glBegin(GL_QUADS);
	}
	else
#endif
	{
		gl::Texture(false);
		glPointSize(param.dot_size);
		glBegin(GL_POINTS);
	}

	const int inc = 
		(style==DRAW_VOXELS_PERSON)
			? mi::minmax(param.person_inc, MIN_VOXEL_INC, MAX_VOXEL_INC)
			: mi::minmax(param.movie_inc,  MIN_VOXEL_INC, MAX_VOXEL_INC); 
	const int SIZE16 = dots.size() << 4;

	const float add_y = 
		(style==DRAW_VOXELS_PERSON)
			? 0.0f
			: param.partner_y;
	const float add_x = param.add_x;
	
	for (int i16=0; i16<SIZE16; i16+=inc)
	{
		const int i = (i16 >> 4);

		const float x = dots[i].x;
		const float y = dots[i].y;
		const float z = dots[i].z;
		const bool in_x = (x>=GROUND_LEFT   && x<=GROUND_RIGHT);
		const bool in_y = (y>=GROUND_BOTTOM && y<=GROUND_TOP);
		const bool in_z = (z>=GROUND_NEAR   && z<=GROUND_FAR);

#if 0
		// Depth is alpha version
		float col = z/4;
		if (col<0.25f) col=0.25f;
		if (col>0.90f) col=0.90f;
		col = 1.00f - col;
		const int col255 = (int)(col*220);

		if (in_x && in_y && in_z)
		{
			inner_color.glColorUpdate(col255);
		}
		else
		{
			outer_color.glColorUpdate(col255>>2);
		}
#else
		// Depth is alpha version
		float col = (GROUND_DEPTH-z)/GROUND_DEPTH;
		if (col<0.25f) col=0.25f;
		if (col>0.90f) col=0.90f;
		col = 1.00f - col;
		const int col255 = 255;
		//(int)(col);// * param.person_base_alpha);
		if (param.is_calibration)
		{
			// キャリブレーション中
			if (in_x && in_y && in_z)
				inner.glColorUpdate(col255>>1);
			else
				outer.glColorUpdate(col255>>2);
		}
		else
		{
			// ゲーム中はエリア内だけ表示する
			if (!(in_x && in_y && in_z))
				continue;

			param.inner_color.glColorUpdate(col255);
		}
#endif

#if 0//# NO QUAD MODE
		if (quad)
		{
			const float K = 0.01f;
			glTexCoord2f(0,0); glVertex3f(x-K,y-K,-z);
			glTexCoord2f(1,0); glVertex3f(x+K,y-K,-z);
			glTexCoord2f(1,1); glVertex3f(x+K,y+K,-z);
			glTexCoord2f(0,1); glVertex3f(x-K,y+K,-z);
		}
		else
#endif
		{
			glVertex3f(
				x + add_x,
				y + add_y,
				-z);
		}
	}

	glEnd();
	return true;
}

void EyeCore::gluLookAt()
{
	const float eye_depth = LOOKAT_EYE_DEPTH;
	const float ex = x + cosf(rh) * eye_depth;
	const float ez = z + sinf(rh) * eye_depth;
	const float ey = y + v;

	::gluLookAt(x,y,z, ex,ey,ez, 0,1,0);
}
