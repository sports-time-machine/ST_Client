#include "ST_Client.h"
#include "file_io.h"
#include "mi/Timer.h"


using namespace stclient;


struct VoxelToCube
{
	int inner_voxels;
	int outer_voxels;
	int duplex_voxels;
};

static void depth_to_store_aux(const RawDepthImage& depth, uint8*& store)
{
	// in:  0<=x<=10000
	// out: 0<=x<=1023
	auto depth_convert = [](int x)->int{
		if (x>4095)
			return 4095>>2;
		else
			return x>>2;
	};

	const int DEPTH_SIZE = 640*480;
	for (int i=0; i<DEPTH_SIZE; ++i)
	{
		const int focus = depth_convert(depth.image[i]);
		int run = 0;
		while (run<32)
		{
			int addr = i+run+1;
			if (addr>=DEPTH_SIZE)
				break;
			if (depth_convert(depth.image[addr])!=focus)
				break;
			++run;
		}

		i += run;
		*store++ = (uint8)(focus & 0xFF);
		*store++ = (uint8)((focus>>8) | (run<<2));
	}
}

static void depth_to_store(const RawDepthImage& depth1, const RawDepthImage& depth2, const uint8** store_dest, int* store_bytes)
{
	// ワークデータ(voxspace)をランレングスで圧縮しストア(store)にいれる
	static std::vector<uint8> store_buffer;
	if (store_buffer.empty())
	{
		// 十分に大きなものにしておく
		const int MEGA = 1024*1024;
		store_buffer.resize(8 * MEGA);
		puts("ALLOCATE!");
	}

	uint8* store = store_buffer.data();

	depth_to_store_aux(depth1, store);
	depth_to_store_aux(depth2, store);

	*store_dest  = store_buffer.data();
	*store_bytes = store - store_buffer.data();
}

void Depth10b6b::record(const RawDepthImage& depth1, const RawDepthImage& depth2, MovieData::Frame& dest_frame)
{
	mi::Timer tm(&time_profile.record.total);

	const uint8* store = nullptr;
	int store_bytes = 0;
	{
		mi::Timer tm(&time_profile.record.enc_stage1);
		depth_to_store(depth1, depth2, &store, &store_bytes);
	}

	{
		mi::Timer tm(&time_profile.record.enc_stage2);
		dest_frame.voxel_count = 0;
		dest_frame.compressed.resize(store_bytes);
		memcpy(dest_frame.compressed.data(), store, store_bytes);
	}

#if 0
	printf("<REC> [%d]bytes (%.1fMbytes)\r",
		store_bytes,
		store_bytes/1000000.0);
#endif
}

void Depth10b6b::playback(RawDepthImage& dest1, RawDepthImage& dest2, const MovieData::Frame& frame)
{
#define CHECK 1
	int dest_index_save[2] = {};

	mi::Timer tm(&time_profile.playback.total);
	{
		mi::Timer tm(&time_profile.playback.dec_stage1);

		size_t src_index = 0;
		const uint8* src = frame.compressed.data();
		const auto DATASIZE = frame.compressed.size();

		for (int target=0; target<2; ++target)
		{
			RawDepthImage& dest = (target==0) ? dest1 : dest2;

			volatile int dest_index = 0;
			while (src_index < DATASIZE)
			{
				uint8 first  = src[src_index++];
				uint8 second = src[src_index++];
		
				int depth = ((first) | ((second&0x03)<<8)) << 2;
				int run   = (second>>2) + 1;

				for (int i=0; i<run; ++i)
				{
					dest.image[dest_index++] = (uint16)depth;
					if (dest_index >= 640*480)
					{
						break;
					}
				}

					if (dest_index >= 640*480)
					{
						break;
					}
			}

#if CHECK
			dest_index_save[target] = dest_index;
#endif
		}
	}

#if CHECK
	printf("<REC> [dev1=%d][dev2=%d][src=%d]\r",
		dest_index_save[0],
		dest_index_save[1],
		frame.compressed.size());
#endif
}
