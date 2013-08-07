#include "file_io.h"
#include "St3dData.h"
#include "mi/Libs.h"

using namespace mi;
using namespace stclient;


// バージョン
//   v1.0    Depth10b6b_v1_0
//   v1.1    Depth10b6b_v1_1

static const char
	STMOVIE_MAGIC_ID[6] = {
		'S','T','M','V',' ',' '},
	DEPTH_2D_10B6B[16]  = {
		'd','e','p','t',
		'h',' ','2','d',
		' ','1','0','b',
		'/','6','b',' '};


void MovieData::saveToFile(File& f) const
{
	const int TOTAL_FRAMES = this->total_frames;

	// ファイルヘッダ
	FileHeader header;
	memcpy(header.signature, STMOVIE_MAGIC_ID, sizeof(header.signature));
	memcpy(header.format,    DEPTH_2D_10B6B,   sizeof(header.format));
	header.ver_major = static_cast<uint8>(this->ver / 10);
	header.ver_minor = static_cast<uint8>(this->ver % 10);
	header.total_frames = TOTAL_FRAMES;
	header.total_msec   = TOTAL_FRAMES * 1000 / 30;
	f.write(header);

	// メタ情報
	f.write(this->cam1);
	f.write(this->cam2);
	f.write(this->dot_size);

	// フレーム情報
	printf("Save %d frames.\n", TOTAL_FRAMES);
	for (int i=0; i<TOTAL_FRAMES; ++i)
	{
		// map[i]はnon-constなのでfindする
		const auto itr = this->frames.find(i);
		if (itr==this->frames.end())
		{
			// フレームスキップした分は保存されない
			f.put32(0);  // no voxels
			f.put32(0);  // no bytedata
			continue;
		}
		const auto& frame = itr->second;
		
		f.put32(frame.voxel_count);
		
		const std::vector<uint8>& bytedata = frame.compressed;
		f.put32(bytedata.size());
		f.write(bytedata.data(), bytedata.size());
		printf("\rFrame %d/%d: %d bytes", 1+i, TOTAL_FRAMES, bytedata.size());
	}
	printf("\n");

	// eof mark
	f.write("[EOF]");
}

template<typename T>
bool checkMagic(const T& field, const char* str)
{
	return memcmp(&field, str, sizeof(field))==0;
}

static void load_ver1_0(File& f, const FileHeader& header, MovieData& movie)
{
	// メタ情報
	f.read(movie.cam1);
	f.read(movie.cam2);
	f.read(movie.dot_size);
//	f.read(movie.player_color);

	// 実映像
#if 0
	printf("Total frames: %d\n", header.total_frames);
#endif
	for (uint32 i=0; i<header.total_frames; ++i)
	{
		MovieData::Frame& frame = movie.frames[i];

		frame.voxel_count = f.get32();

		uint32 bytedata_size = f.get32();
#if 0
		printf("\rFrame %d/%d, %d voxels, %d bytes (%08X)",
			1+i,
			header.total_frames,
			frame.voxel_count,
			bytedata_size,
			bytedata_size);
#endif
		frame.compressed.resize(bytedata_size);
		f.read(frame.compressed.data(), bytedata_size);
	}
#if 0
	printf("\n");
#endif
}

int MovieData::getValidFrame(int frame) const
{
	for (;;)
	{
		if (frames.find(frame)!=frames.end())
		{
			// good!
			return frame;
		}

		// 前のフレームを表示しようと試みる
		--frame;
		
		// フレームがなくなったら表示を諦める
		if (frame<0)
		{
			OutputDebugString("frame not found! org\n");
			return -1;
		}
	}
}

void MovieData::clearMovie()
{
	frames.clear();
	total_frames = 0;
}

void MovieData::clearAll(mgl::glRGBA default_player_color)
{
	clearMovie();
	ver               = VER_INVALID;
	dot_size          = 1.0f;
	player_color      = "default";
	player_color_rgba = default_player_color;
}

bool MovieData::load(const string& path)
{
	Msg::Notice("Movie load", path.c_str());
	File f;
	if (!f.open(path))
	{
		Msg::ErrorMessage("Cannot open file (MovieData::load)", path);
		return false;
	}

	FileHeader header;
	f.read(header);

	if (!checkMagic(header.signature, STMOVIE_MAGIC_ID))
	{
		fprintf(stderr, "File is not ST-Movie format.\n");
		return false;
	}
	if (!checkMagic(header.format, DEPTH_2D_10B6B))
	{
		fprintf(stderr, "Unsupport format.\n");
		return false;
	}
	if (header.total_frames<=0 || header.total_frames>999999)
	{
		fprintf(stderr, "Invalid total frames.\n");
		return false;
	}

	this->total_frames = header.total_frames;
	this->ver = VER_INVALID;

	if (header.ver_major==1 && header.ver_minor==0)
	{
		this->ver = VER_1_0;
		load_ver1_0(f, header, *this);
	}
	else if (header.ver_major==1 && header.ver_minor==1)
	{
		// ロード自体はv1.0と同じ
		this->ver = VER_1_1;
		load_ver1_0(f, header, *this);
	}
	else
	{
		fprintf(stderr, "Unsupport movie version: stmovie ver %d.%d.\n",
			header.ver_major,
			header.ver_minor);
		return false;
	}

	return true;
}
