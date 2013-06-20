#pragma once
#include "mi/Core.h"
#include "zlibpp.h"
#include <map>


struct Point3D
{
	float x,y,z;
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


struct FileHeader
{
	unsigned __int8
		signature[4],  // "stm "
		compress[4],   // "zip "
		graphic[4];    // "dpth"
	int total_frames;
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

struct MovieData
{
	struct Frame
	{
		int voxel_count;
		std::vector<uint8> compressed;
	};

	CamParam cam1;
	CamParam cam2;
	int total_frames;
	std::map<int,Frame> frames;

	MovieData()
	{
		clear();
	}

	void clear()
	{
		total_frames = 0;
		frames.clear();
	}
};

extern void saveToFile(FILE* fp, const MovieData& movie);
extern bool loadFromFile(FILE* fp, MovieData& movie);
