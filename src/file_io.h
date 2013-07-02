#pragma once
#include "mi/Core.h"
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

struct FileHeader
{
	unsigned char        // "1234567890123456"
		signature[6];    // "STMV  "
	uint8  ver_major;
	uint8  ver_minor;
	uint32 total_frames;
	uint32 total_msec;
	//----16bytes---
	unsigned char
		format[16];      // "depth 2d 10b/6b "
	//----16bytes---
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

extern void saveToFile(mi::File& f, const MovieData& movie);
extern bool loadFromFile(mi::File& f, MovieData& movie);

}//namespace stclient
