#include "ST_Client.h"
#include "file_io.h"
#include "mi/Timer.h"

using namespace stclient;

enum
{
	W = 1024,
	H = 256,
	D = 256,
	BYTE_PER_PLANE = W*H/8,
};

#if 1
const float CUBE_LEFT   = -2.2f;
const float CUBE_RIGHT  = +2.2f;
const float CUBE_BOTTOM = -0.2f;
const float CUBE_TOP    = +3.2f;
#else
const float CUBE_LEFT   = -1.2f;
const float CUBE_RIGHT  = +1.2f;
const float CUBE_BOTTOM = -0.2f;
const float CUBE_TOP    = +1.5f;
#endif
const float CUBE_NEAR   =  0.0f;
const float CUBE_FAR    =  3.0f;
const float CUBE_WIDTH  = (-CUBE_LEFT)+(CUBE_RIGHT);
const float CUBE_HEIGHT = (-CUBE_BOTTOM)+(CUBE_TOP);
const float CUBE_DEPTH  = (-CUBE_NEAR)+(CUBE_FAR);


struct Plane
{
	uint8 bit[BYTE_PER_PLANE];

	void clear()
	{
		memset(bit, 0, sizeof(bit));
	}
};

struct Cube
{
	Plane plane[D];
		
	void clear()
	{
		for (int i=0; i<D; ++i)
		{
			plane[i].clear();
		}
	}

	static Cube& get() { static Cube* cube = new Cube; return *cube; }

private:
	Cube() {}
};

//	static void record(MovieData::Frame& dest_frame);
//	static void playback(Dots& dots, const MovieData::Frame& frame);


struct VoxelToCube
{
	int inner_voxels;
	int outer_voxels;
	int duplex_voxels;
};

static VoxelToCube voxel_to_cube(const Dots& dot_set, Cube& cube)
{
	VoxelToCube res = {};

	// ボクセルをcubeにストアしていく
	res.inner_voxels = 0;
	for (int i=0; i<dot_set.size(); ++i)
	{
		const auto& d = dot_set[i];
		if (d.x>=CUBE_LEFT && d.x<=CUBE_RIGHT && d.y>=CUBE_BOTTOM && d.y<=CUBE_TOP && d.z>=CUBE_NEAR && d.z<=CUBE_FAR)
		{
			const int x = (d.x-CUBE_LEFT  ) * W / CUBE_WIDTH;
			const int y = (d.y-CUBE_BOTTOM) * H / CUBE_HEIGHT;
			const int z = (d.z-CUBE_NEAR  ) * D / CUBE_DEPTH;
			if ((uint)x<W && (uint)y<H && (uint)z<D)
			{
				int addr = x + (y*W);
				const int bit = 1 << (addr & 7);

				auto& ref = cube.plane[z].bit[addr>>3];
				if (!(ref & bit))
				{
					ref = bit;
					++res.inner_voxels;
				}
				else
				{
					++res.duplex_voxels;
				}
			}
		}
		else
		{
			++res.outer_voxels;
		}
	}
	return res;
}

static void cube_to_store(const Cube& cube, const uint8** store_dest, int* store_length)
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
	for (int z=0; z<D; ++z)
	{
		// Zプレーンごとに圧縮する
		// 512x512x512の空間に10万のボクセルが存在する場合、
		// 存在率は0.07%でしかないため、どのぐらいゼロが連続したかを記録していく。
		// 0000 0000 1000 0000 0000 0000 0100 というデータの場合、0が8つ、1、0が16、1、0が2つなので、
		// [8,16,2]というデータになる。
		auto& plane = cube.plane[z].bit;
		int i = 0;
		while (i<BYTE_PER_PLANE)
		{
			// ゼロの個数
			int zero_length = 0;
			while (i<BYTE_PER_PLANE && plane[i]==0)
			{
				++zero_length;
				++i;
				if (zero_length==4194303)
					break;
			}

			// [0xxxxxxx]:                       7bit 1-127
			// [1xxxxxxx] [0yyyyyyy]            14bit 1-16383
			// [1xxxxxxx] [1yyyyyyy] [zzzzzzzz] 22bit 1-4194303

			// ゼロ以外のデータ
			uint8 non_zero = 0;
			if (plane[i]==0 || i>=BYTE_PER_PLANE)
			{
				// 非ゼロが存在しなかった
				non_zero = 0;
			}
			else
			{
				// 非ゼロあり
				non_zero = plane[i++];
			}
			
			if (zero_length>=16384)
			{
				// triple byte length
				*store++ = 0x80 |  (zero_length       & 0x7F);
				*store++ = 0x80 | ((zero_length >> 7) & 0x7F);
				*store++ =         (zero_length >> 14);
			}
			else if (zero_length>=128)
			{
				// double byte length
				*store++ = 0x80 | (zero_length & 0x7F);
				*store++ =         zero_length  >>  7;
			}
			else
			{
				// single byte length
				*store++ = zero_length;
			}
			*store++ = non_zero;
		}
	}

	*store_dest   = store_buffer.data();
	*store_length = store - store_buffer.data();
}

void VoxelRecorder::record(const Dots& dots, MovieData::Frame& dest_frame)
{
	mi::Timer tm(&time_profile.record.total);

	Cube& cube = Cube::get();
	cube.clear();

	VoxelToCube vc;
	{
		mi::Timer tm(&time_profile.record.enc_stage1);
		vc = voxel_to_cube(dots, cube);
	}

	const uint8* store = nullptr;
	int store_bytes = 0;
	{
		mi::Timer tm(&time_profile.record.enc_stage2);
		cube_to_store(cube, &store, &store_bytes);
	}

	{
		mi::Timer tm(&time_profile.record.enc_stage3);
		// Copy 'frame' from 'store'
		dest_frame.voxel_count = vc.inner_voxels;
		dest_frame.compressed.resize(store_bytes);
		memcpy(dest_frame.compressed.data(), store, store_bytes);
	}

#if 1
	printf("<REC> total[%d] inner[%d] outer[%d] duplex[%d] [%d]Kbytes\r",
		dots.size(),
		vc.inner_voxels,
		vc.outer_voxels,
		vc.duplex_voxels,
		store_bytes);
#endif
}

void VoxelRecorder::playback(Dots& dots, const MovieData::Frame& frame)
{
	if (frame.voxel_count==0)
	{
		puts("No voxel data.");
		return;
	}


	mi::Timer tm(&time_profile.playback.total);

	dots.init(frame.voxel_count);
//#	printf("<PLAYBACK> %d voxels\n", frame.voxel_count);

	int dot_index = 0;
	int index = 0;
	for (volatile size_t i=0; i<frame.compressed.size(); )
	{
		const int first = frame.compressed[i++];
		int zero_length = first;
		if (first & 0x80)
		{
			const int second = frame.compressed[i++];

			if (second & 0x80)
			{
				const int third = frame.compressed[i++];
				zero_length = (first & 0x7F) | ((second & 0x7f) << 7) | (third << 14);
			}
			else
			{
				zero_length = (first & 0x7F) | (second << 7);
			}
		}
		
		volatile uint8 non_zero    = frame.compressed[i++];
		
		if (zero_length==0 && non_zero==0)
		{
			puts("INVALID FRAME!");
			break;
		}

		index += zero_length;
		if (non_zero!=0)
		{
			for (int i=0; i<8; ++i)
			{
				if (non_zero & (1<<i))
				{
					auto& vox = dots[dot_index++];
					int bit_addr = (index<<3) + i;
					volatile const int x = (bit_addr)         % W;
					volatile const int y = (bit_addr) / (W)   % H;
					volatile const int z = (bit_addr) / (W*H);
		#if 1
					vox.x = x * CUBE_WIDTH  / W + CUBE_LEFT;
					vox.y = y * CUBE_HEIGHT / H + CUBE_BOTTOM;
					vox.z = z * CUBE_DEPTH  / D + CUBE_NEAR;
		#else
					vox.x = x * 0.01;
					vox.y = y * 0.01;
					vox.z = z * 0.01;
		#endif
				}
			}

			index += 1;
		}
//		printf("(playback) (%d)\n", index);
	}
}
