#pragma once
#include "mi/Core.h"
#include "zlibpp.h"
#include <map>


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
	float x,y,z,rotx,roty,rotz,scale;

	CamParam()
	{
		x=y=z=rotx=roty=rotz=0.0f;
		scale=1.0f;
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

enum StColor
{
	STCOLOR_RED,
	STCOLOR_GREEN,
	STCOLOR_BLUE,
	STCOLOR_ORANGE,
	STCOLOR_LIME,
	STCOLOR_AQUA,
	STCOLOR_PINK,
	STCOLOR_VIOLET,
	STCOLOR_WHITE,
	STCOLOR_BLACK,
};

struct MovieData
{
	static const char* non_id()
	{
		return "NON-ID";
	}

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

	string    run_id;
	float     dot_size;
	StColor   player_color;
	CamParam  cam1;
	CamParam  cam2;
	int       total_frames;
	Frames    frames;

	MovieData()
	{
		clear();
	}

	void clear()
	{
		frames.clear();
		total_frames = 0;
		run_id       = non_id();
		dot_size     = 1.0f;
		player_color = STCOLOR_WHITE;
	}

	bool load(const string& id);
};

extern void saveToFile(mi::File& f, const MovieData& movie);
extern bool loadFromFile(mi::File& f, MovieData& movie);
