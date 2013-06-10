#include "ST_Client.h"
#include "gl_funcs.h"
#include "Config.h"


const int far_clipping = 5000;


void StClient::CreateRawDepthImage(RawDepthImage& raw)
{
	if (!m_depthStream.isValid())
	{
		// Uninitialized (without kinect mode)
		return;
	}


	using namespace openni;

	// Read depth image from Kinect
	m_depthStream.readFrame(&m_depthFrame);

	// Create raw depth image
	const auto* depth_row = (const DepthPixel*)m_depthFrame.getData();
	const int rowsize = m_depthFrame.getStrideInBytes() / sizeof(DepthPixel);
	uint16* dest = raw.image.data();
	const int src_inc = mode.mirroring ? -1 : +1;

	raw.max_value = 0;
	raw.min_value = 0;
	raw.range     = 0;
	for (int y=0; y<480; ++y)
	{
		const auto* src = depth_row + (mode.mirroring ? 639 : 0);
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
	CalcDepthMinMax(raw_cooked);
}

void StClient::CalcDepthMinMax(RawDepthImage& raw)
{
	const uint16* src = raw.image.data();
	raw.max_value = 0;
	raw.min_value = 65535;
	for (int i=0; i<640*480; ++i)
	{
		uint16 v = src[i];
		if (v!=0)
		{
			raw.max_value = max(raw.max_value, v);
			raw.min_value = min(raw.min_value, v);
		}
	}
	raw.range = max(1, raw.max_value - raw.min_value);
}

void StClient::CreateTransformed(
	RawDepthImage& raw_transformed,
	const RawDepthImage& raw_cooked)
{
	//     x
	//  A-----B          A-_g
	//  |     |         /   \_
	// y|     |  -->  e/      B
	//  |     |       /  h   /f
	//  C-----D      C------D
	const auto& kc = config.kinect_calibration;
	Point2i a = kc.a;
	Point2i b = kc.b;
	Point2i c = kc.c;
	Point2i d = kc.d;
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
	CalcDepthMinMax(raw_transformed);
}
