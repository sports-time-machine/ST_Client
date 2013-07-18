#pragma once
#include "gl_funcs.h"

namespace stclient{

struct Point3D
{
	float x,y,z;

	Point3D() { }
	Point3D(float a, float b, float c) { x=a; y=b; z=c; }
};

struct Dots
{
	std::vector<Point3D> dots;
	int tail;

	int size() const
	{
		return tail;
	}

	void init(int new_size = 0)
	{
		if (dots.empty() || new_size!=0)
		{
			if (new_size==0)
			{
				new_size = 640*480*2;
			}
			printf("new size %d\n", new_size);

			dots.resize(new_size);
		}

		tail = 0;
	}

	void push(Point3D point)
	{
		dots[tail++] = point;
	}

	const Point3D& operator[](int n) const
	{
		return dots[n];
	}

	Point3D& operator[](int n)
	{
		return dots[n];
	}
};

struct RawDepthImage
{
	std::vector<uint16> image;
	uint16 min_value;
	uint16 max_value;
	uint16 range;        // max_value - min_value

	RawDepthImage()
	{
		image.resize(640*480);
		min_value = 0;
		max_value = 0;
		range = 0;
	}

	void CalcDepthMinMax();
};


struct Eye
{
	Eye()
	{
		go_pos = -1;
		fast_set = true;
	}

	float x,y,z;    // 視線の原点
	float rh;       // 視線の水平方向(rad)
	float v;        // 視線の垂直方向

	struct Go
	{
		float x,y,z,rh,v;
	} from,to;
	int go_pos;
	bool fast_set;

	enum { GO_FRAMES=25 };

	void set(float go_x, float go_y, float go_z, float go_rh, float go_v)
	{
		if (fast_set)
		{
			this->x  = go_x;
			this->y  = go_y;
			this->z  = go_z;
			this->rh = go_rh;
			this->v  = go_v;
		}
		else
		{
			this->from.x  = this->x;
			this->from.y  = this->y;
			this->from.z  = this->z;
			this->from.rh = this->rh;
			this->from.v  = this->v;
			this->to.x  = go_x;
			this->to.y  = go_y;
			this->to.z  = go_z;
			this->to.rh = go_rh;
			this->to.v  = go_v;
			this->go_pos = GO_FRAMES;
		}
	}

	void updateCameraMove()
	{
		if (go_pos>=0)
		{
			const float i =
				(go_pos==GO_FRAMES)
					?  1.0f
					: (1.0f * go_pos  / GO_FRAMES);
			const float j =
				(go_pos==0)
					?  1.0f
					: (1.0f * (GO_FRAMES-go_pos) / GO_FRAMES);
			x  = j*to.x  + i*from.x;
			y  = j*to.y  + i*from.y;
			z  = j*to.z  + i*from.z;
			rh = j*to.rh + i*from.rh;
			v  = j*to.v  + i*from.v;
			--go_pos;
		}
	}

	void gluLookAt();

	void view_2d_left();
	void view_2d_top();
	void view_2d_front();
	void view_2d_run();
	void view_3d_left();
	void view_3d_right();
	void view_3d_front();
};

struct CamParam
{
	struct XYZ
	{
		float x,y,z;
		XYZ() { x=y=z=0.0f; }
	};
	XYZ pos,rot,scale;

	CamParam()
	{
		scale.x = 1.0f;
		scale.y = 1.0f;
		scale.z = 1.0f;
	}
};

struct MovieData
{
	struct Frame
	{
		int voxel_count;
		std::vector<uint8> compressed;
	};

	// フレームスキップがないかぎりには0～(total_frames-1)までのインデクスの
	// 動画が収められる。フレームスキップがあった場合、そのFrameはインデクスが存在しない。
	typedef std::map<int,Frame> Frames;

	// frame以前の正常なフレームを得る
	// 失敗したら負
	int getValidFrame(int frame) const;

	enum Version
	{
		VER_INVALID = 0,
		VER_1_0 = 10,
		VER_1_1 = 11,
	};

	Version     ver;           // ver 1.0 == 10
	string      player_id;     // 0000ABCD
	string      game_id;       // 00000XYZ23
	float       dot_size;
	string      player_color;
	mgl::glRGBA player_color_rgba;
	CamParam    cam1;
	CamParam    cam2;
	int         total_frames;
	Frames      frames;

	MovieData()
	{
		clearAll();
	}

	void clearMovie();
	void clearAll();

	bool load(const string& id);
};


class AppCore
{
public:
	static bool initGraphics(bool full_screen, const std::string& title);
	static void MyGetKeyboardState(BYTE* kbd);
};


class VoxGrafix
{
public:
	enum DrawStyle
	{
		//DRAW_VOXELS_QUAD = 2,
		DRAW_VOXELS_PERSON = 1,
		DRAW_VOXELS_MOVIE  = 2,
	};

	struct OutData
	{
		int atari_count;
		int dot_count;
	};

	struct DrawParam
	{
		mgl::glRGBA  inner_color;
		mgl::glRGBA  outer_color;
		float        dot_size;
		bool         mute_if_veryfew;
		int          mute_threshould;
		int          person_inc;
		int          movie_inc;
		float        partner_y;
		float        person_base_alpha;
		bool         is_calibration;

		DrawParam()
		{
			inner_color.set(240,160,80, 160);
			outer_color.set(60,60,60, 120);
			dot_size          = 1.0f;
			mute_if_veryfew   = false;
			mute_threshould   = 0;
			person_inc        = 16;
			movie_inc         = 16;
			partner_y         = 0.0f;
			person_base_alpha = 1.0f;
			is_calibration    = false;
		}
	};

	struct Static
	{
		int   atari_count;
		int   dot_count;
	};

	static Static global;

	static bool DrawMovieFrame(const MovieData& mov, int frame_index, mgl::glRGBA inner, mgl::glRGBA outer, const char* movie_type, DrawStyle style);
	static void MixDepth(Dots& dots, const RawDepthImage& src, const CamParam& cam);
	static bool DrawVoxels(const Dots& dots, const DrawParam& param, mgl::glRGBA inner, mgl::glRGBA outer, DrawStyle style = DRAW_VOXELS_PERSON);
};



namespace Depth10b6b{
	void record(const RawDepthImage& depth1, const RawDepthImage& depth2, MovieData::Frame& dest_frame);
	void playback(RawDepthImage& dest1, RawDepthImage& dest2, const MovieData::Frame& frame);
}//namespace VoxelRecorder

namespace Depth10b6b_v1_1{
	void record(const RawDepthImage& depth1, const RawDepthImage& depth2, MovieData::Frame& dest_frame);
	void playback(RawDepthImage& dest1, RawDepthImage& dest2, const MovieData::Frame& frame);
}//namespace VoxelRecorder

namespace VoxelRecorder{
	void record(const Dots& dots, MovieData::Frame& dest_frame);
	void playback(Dots& dots, const MovieData::Frame& frame);
}//namespace VoxelRecorder

}//namespace stclient
