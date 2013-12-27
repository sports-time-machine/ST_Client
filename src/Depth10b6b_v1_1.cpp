//==============================================================
// .stmovのデータを読み書きするルーチン
// Kinectからもらえる1-10000までのdepth情報を、
// 0-1023におさめ、それをバイトストリームにおさめていく
// バイトストリームには16ビット単位で追加していく
// このときのビット配分が、データ10bits、ランレングス長6bitsなので
// フォーマット名が10b/6bになっている。
//==============================================================
#include "St3dData.h"
#include "file_io.h"
#include "mi/Timer.h"
#ifdef TIME_PROFILE
#include "StClient.h"
#endif


using namespace stclient;


// 10000/1023 = 9.775
// 2502/256 = 9.773
// 1/9.775 = 0.1023
// 104/1024 = 0.1023

static void depth_to_store_aux(const RawDepthImage& depth, uint8*& store)
{
	// v1.0のときはここにバグがありました
	// in:  0<=x<=10000
	// out: 0<=x<=1023
	auto depth_convert = [](int x)->int{
		return x * 104 >> 10;
	};

	const int DEPTH_SIZE = 640*480;
	for (int i=0; i<DEPTH_SIZE; ++i)
	{
		// focusは現在注目しているdepthデータ
		// depth.image[i]は0-10000だが、変換して、
		// 0-1023(10bit)におさまるようにしている
		const int focus = depth_convert(depth.image[i]);
		int run = 0;

		// 32ラン(6bit)までランレングスを挑戦する
		while (run<32)
		{
			int addr = i+run+1;
			if (addr>=DEPTH_SIZE)
				break;
			if (depth_convert(depth.image[addr])!=focus)
				break;
			++run;
		}

		// バイトストリームに書き出す
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
		// 圧縮したデータがカメラ2つ分入る大きさである必要がある
		// 640x480=300Kなので、ランレングスがワーストケースで
		// 2倍にふくれても600K、カメラ2つ分で1.2Mだとしても、
		// 8Mの容量があれば完全に大丈夫
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

void Depth10b6b_v1_1::record(const RawDepthImage& depth1, const RawDepthImage& depth2, MovieData::Frame& dest_frame)
{
#ifdef TIME_PROFILE
	mi::Timer tm(&time_profile.record.total);
#endif

	const uint8* store = nullptr;
	int store_bytes = 0;
	{
#ifdef TIME_PROFILE
		mi::Timer tm(&time_profile.record.enc_stage1);
#endif
		depth_to_store(depth1, depth2, &store, &store_bytes);
	}

	{
#ifdef TIME_PROFILE
		mi::Timer tm(&time_profile.record.enc_stage2);
#endif
		dest_frame.voxel_count = 0;
		dest_frame.compressed.resize(store_bytes);
		memcpy(dest_frame.compressed.data(), store, store_bytes);
	}
}

void Depth10b6b_v1_1::playback(RawDepthImage& dest1, RawDepthImage& dest2, const MovieData::Frame& frame)
{
#ifdef TIME_PROFILE
	mi::Timer tm(&time_profile.playback.total);
#endif
	{
#ifdef TIME_PROFILE
		mi::Timer tm(&time_profile.playback.dec_stage1);
#endif

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
		
				int depth = ((first) | ((second&0x03)<<8)) * 2502 >> 8;
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
		}
	}
}
