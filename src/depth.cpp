#include "StClient.h"
#include "gl_funcs.h"
#include "Config.h"


using namespace stclient;


const uint16 FAR_DEPTH = 10000;

//========================================
// KinectからDepthデータをそのまま読み取る
//========================================
void Kdev::CreateRawDepthImage_Read()
{
	if (!depth.isValid())
	{
		// Uninitialized (without kinect mode)
		return;
	}

	// Read depth image from Kinect
	depth.readFrame(&depthFrame);
}

//===========================================================
// Depthイメージを取得する
//------------------------
//  - Depthデータは0-10000程度の整数でもらえ単位はmmとなる。
//    したがって3.23mの位置にあるドットは3230というDepthになる
//===========================================================
void Kdev::CreateRawDepthImage()
{
	using namespace openni;

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
			if (v==0)
			{
				// invalid data => far
				v = FAR_DEPTH;
			}
			*dest++ = v;
			src += src_inc;
		}
		depth_row += rowsize;
	}
}

//=============================================================================
// Depthイメージから床イメージを排除する
//-------------------------------------
//  - 床イメージは距離の「最大値」であるので、その「手前」だけを有効な画素とする。
//    つまり床イメージDepthよりも「少ない」Depthだけが有効である。
//=============================================================================
void Kdev::CreateCookedImage()
{
	float avg_x = 0.0f;
	float avg_y = 0.0f;

	int count = 0;
	int i = 0;
	for (int y=0; y<480; ++y)
	{
		for (int x=0; x<640; ++x, ++i)
		{
			const int src   = raw_depth.image[i];
			const int floor = raw_floor.image[i];

			if (src<floor || floor==FAR_DEPTH)
			{
				avg_x += x;
				avg_y += y;
				++count;
				raw_cooked.image[i] = (uint16)src;
			}
			else
			{
				// Floorより奥
				raw_cooked.image[i] = 0;
			}
		}
	}
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
}

void Kdev::initRam()
{
	glGenTextures(1, &this->vram_tex);
	glGenTextures(1, &this->vram_floor);
	clearFloorDepth();
}

//==================================
// FloorDepthのクリア
//==================================
void Kdev::clearFloorDepth()
{
	for (int i=0; i<640*480; ++i)
	{
		raw_floor.image[i] = FAR_DEPTH;
	}
}

//==================================
// FloorDepthの更新
//----------------------------------
//  - depthが小さいほど「近い」ため、
//    0でなくより小さい値を採用する
//==================================
void Kdev::updateFloorDepth()
{
	// Copy depth to floor
	for (int i=0; i<640*480; ++i)
	{
		const uint16 depth = raw_depth.image[i];

		if (raw_floor.image[i]==0 || raw_floor.image[i]>depth)
		{
			raw_floor.image[i] = depth - 50;
		}
	}
}
