#include "ST_Client.h"
#include "gl_funcs.h"
#include "Config.h"


const int far_clipping = 5000;


void Kdev::CreateRawDepthImage()
{
	if (!depth.isValid())
	{
		// Uninitialized (without kinect mode)
		return;
	}

	using namespace openni;

	// Read depth image from Kinect
	depth.readFrame(&depthFrame);

	const bool mirroring = mode.mirroring ^ config.mirroring;

	// Create raw depth image
	const auto* depth_row = (const DepthPixel*)depthFrame.getData();
	const int rowsize = depthFrame.getStrideInBytes() / sizeof(DepthPixel);
	uint16* dest = raw_depth.image.data();
	const int src_inc = mirroring ? -1 : +1;

	raw_depth.max_value = 0;
	raw_depth.min_value = 0;
	raw_depth.range     = 0;
	for (int y=0; y<480; ++y)
	{
		const auto* src = depth_row + (mirroring ? 639 : 0);
		for (int x=0; x<640; ++x)
		{
			uint16 v = *src;
			if (v > far_clipping)
			{
				// too far
				v = 0;
			}
			*dest++ = v;
			src += src_inc;
		}
		depth_row += rowsize;
	}
}

void StClient::CreateCoockedDepth(RawDepthImage& raw_cooked, const RawDepthImage& raw_depth, const RawDepthImage& raw_floor)
{
	// Part 1: Mix depth and floor
	for (int i=0; i<640*480; ++i)
	{
		const int src   = raw_depth.image[i];
		const int floor = raw_floor.image[i];

		if (src < floor-20 || floor==0)
		{
			if (src>=config.far_cropping)
			{
				raw_cooked.image[i] = 0;
			}
			else
			{
				raw_cooked.image[i] = (uint16)src;
			}
		}
		else
		{
			raw_cooked.image[i] = 0;
		}
	}
#if 0

	// Part 2: 
	// ......    ......    ......
	// .IIIII    .12321    ..232.
	// ...I.. -> ...2.. -> ...2..
	// ..IIII    ..2321    ..232.
	// ..I...    ..1...    ......
	auto get = [&](int x, int y)->bool{
		if ((uint)x>=640 || (uint)y>=480)
		{
			return false;
		}
		return raw_cooked[x + y*640]!=0;
	};

	for (int y=0; y<480; ++y)
	{
		for (int x=0; x<640; ++x)
		{
			int count =
				(get(x-1,y)!=false)+
				(get(x+1,y)!=false)+
				(get(x,y-1)!=false)+
				(get(x,y+1)!=false);
			if (count
//			raw_
		}
	}
#endif

	raw_cooked.CalcDepthMinMax();
}

void RawDepthImage::CalcDepthMinMax()
{
	const uint16* src = this->image.data();
	this->max_value = 0;
	this->min_value = 65535;
	for (int i=0; i<640*480; ++i)
	{
		uint16 v = src[i];
		if (v!=0)
		{
			this->max_value = max(this->max_value, v);
			this->min_value = min(this->min_value, v);
		}
	}
	this->range = max(1, this->max_value - this->min_value);

	if (!mode.auto_clipping)
	{
		this->max_value = 3000;
		this->min_value = 500;
		this->range = this->max_value - this->min_value;
	}
}

void Kdev::CreateTransformed()
{
	//     x
	//  A-----B          A-_
	//  |     |         /   \_
	// e|-----|f -->  e/-__   B
	//  |     |       /    --/f
	//  C-----D      C------D
	const Point2i a = calibration.a;
	const Point2i b = calibration.b;
	const Point2i c = calibration.c;
	const Point2i d = calibration.d;
	int index = 0;
	for (int y=0; y<480; ++y)
	{
		Point2i e(
			a.x*(480-y)/480 + c.x*(y)/480,
			a.y*(480-y)/480 + c.y*(y)/480);
		Point2i f(
			b.x*(480-y)/480 + d.x*(y)/480,
			b.y*(480-y)/480 + d.y*(y)/480);
		for (int x=0; x<640; ++x)
		{
			Point2i k(
				e.x*(640-x)/640 + f.x*(x)/640,
				e.y*(640-x)/640 + f.y*(x)/640);
			raw_transformed.image[index] = raw_cooked.image[k.y*640 + k.x];
			++index;
		}
	}
	raw_transformed.CalcDepthMinMax();
}
