#include "StClient.h"

using namespace mi;
using namespace stclient;

static const char
	STMOVIE_MAGIC_ID[6] = {
		'S','T','M','V',' ',' '},
	DEPTH_2D_10B6B[16]  = {
		'd','e','p','t',
		'h',' ','2','d',
		' ','1','0','b',
		'/','6','b',' '};

const int MAX_TOTAL_FRAMES = 30 * 60 * 5; // 5 minute


void stclient::saveToFile(File& f, const MovieData& movie)
{
	const int TOTAL_FRAMES = movie.total_frames;

	// ファイルヘッダ
	FileHeader header;
	memcpy(header.signature, STMOVIE_MAGIC_ID, sizeof(header.signature));
	memcpy(header.format,    DEPTH_2D_10B6B,   sizeof(header.format));
	header.ver_major = 1;
	header.ver_minor = 0;
	header.total_frames = TOTAL_FRAMES;
	header.total_msec   = TOTAL_FRAMES * 1000 / 30;
	f.write(header);

	// メタ情報
	f.write(movie.cam1);
	f.write(movie.cam2);
	f.write(movie.dot_size);
	f.write(movie.player_color);

	// フレーム情報
	printf("Save %d frames.\n", TOTAL_FRAMES);
	for (int i=0; i<TOTAL_FRAMES; ++i)
	{
		// map[i]はnon-constなのでfindする
		const auto itr = movie.frames.find(i);
		if (itr==movie.frames.end())
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
	f.read(movie.player_color);

	// 実映像
	printf("Total frames: %d\n", header.total_frames);
	for (uint32 i=0; i<header.total_frames; ++i)
	{
		MovieData::Frame& frame = movie.frames[i];

		frame.voxel_count = f.get32();

		uint32 bytedata_size = f.get32();
		printf("\rFrame %d/%d, %d voxels, %d bytes (%08X)",
			1+i,
			header.total_frames,
			frame.voxel_count,
			bytedata_size,
			bytedata_size);
		frame.compressed.resize(bytedata_size);
		f.read(frame.compressed.data(), bytedata_size);
	}
	printf("\n");
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
			return -1;
		}
	}
}

void MovieData::clear()
{
	frames.clear();
	total_frames      = 0;
	dot_size          = 1.0f;
	player_color      = "default";
	player_color_rgba = global_config.color.default_player_color;
}

bool MovieData::load(const string& id)
{
	string name = GameInfo::GetFolderName(id) + id + ".stmov";
	File f;
	if (!f.open(name))
	{
		Msg::ErrorMessage("Cannot open file", name);
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
	if (header.total_frames<=0 || header.total_frames>=MAX_TOTAL_FRAMES)
	{
		fprintf(stderr, "Invalid total frames.\n");
		return false;
	}

	this->total_frames = header.total_frames;

	if (header.ver_major==1 && header.ver_minor==0)
	{
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
