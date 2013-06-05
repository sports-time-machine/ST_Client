#pragma once
#include "miCore.h"
#include "zlibpp.h"


struct FileHeader
{
	unsigned __int8
		signature[4],  // "stm "
		compress[4],   // "zip "
		graphic[4];    // "dpth"
	int total_frames;
};

struct MovieData
{
	std::vector<zlibpp::bytes> frames;
	size_t recorded_tail;

	MovieData()
	{
		recorded_tail = 0;
	}
};

extern void saveToFile(FILE* fp, const MovieData& movie);
extern bool loadFromFile(FILE* fp, MovieData& movie);
