#include "file_io.h"


void saveToFile(FILE* fp, const MovieData& movie)
{
	FileHeader header;

	header.signature[0] = 's';
	header.signature[1] = 't';
	header.signature[2] = 'm';
	header.signature[3] = ' ';
	header.compress[0] = 'z';
	header.compress[1] = 'i';
	header.compress[2] = 'p';
	header.compress[3] = ' ';
	header.graphic[0] = 'd';
	header.graphic[1] = 'p';
	header.graphic[2] = 't';
	header.graphic[3] = 'h';
#if 0
	header.total_frames = movie.recorded_tail;

	fwrite(&header, sizeof(header), 1, fp);
	for (size_t i=0; i<movie.recorded_tail; ++i)
	{
		const auto& frame = movie.frames[i];
		uint32 frame_size = (uint32)frame.size();
		fwrite(&frame_size, sizeof(frame_size), 1, fp);
		fwrite(frame.data(), frame_size, 1, fp);
	}
#endif
	puts("–¢ŽÀ‘•‚Å‚·");

	// for human
	fputs("//END", fp);
}

bool checkMagic(const unsigned __int8* data, const char* str)
{
	return
		data[0]==str[0] &&
		data[1]==str[1] &&
		data[2]==str[2] &&
		data[3]==str[3];
}

bool loadFromFile(FILE* fp, MovieData& movie)
{
	FileHeader header;
	fread(&header, sizeof(header), 1, fp);

	if (!checkMagic(header.signature, "stm "))
	{
		fprintf(stderr, "File is not STM format.\n");
		return false;
	}
	if (!checkMagic(header.compress, "zip "))
	{
		fprintf(stderr, "Unsupport compress format.\n");
		return false;
	}
	if (!checkMagic(header.graphic, "dpth"))
	{
		fprintf(stderr, "Unsupport graphic format.\n");
		return false;
	}
	if (header.total_frames<=0 || header.total_frames>=30*60*5)
	{
		fprintf(stderr, "Invalid total frames.\n");
		return false;
	}

#if 0
	movie.recorded_tail = header.total_frames;
	printf("Total %d frames\n", header.total_frames);

	movie.frames.clear();
	movie.frames.resize(header.total_frames);
	for (int i=0; i<header.total_frames; ++i)
	{
		auto& frame = movie.frames[i];
		unsigned __int32 frame_size;
		fread(&frame_size, sizeof(frame_size), 1, fp);

		frame.resize(frame_size);
		fread(frame.data(), frame_size, 1, fp);
	}
#endif
	puts("–¢ŽÀ‘•‚Å‚·");

	return true;
}
