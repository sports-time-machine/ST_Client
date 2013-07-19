#pragma once
#include "mi/Core.h"

namespace stclient{

struct FileHeader
{
	unsigned char        // "1234567890123456"
		signature[6];    // "STMV  "
	uint8  ver_major;
	uint8  ver_minor;
	uint32 total_frames;
	uint32 total_msec;
	//----16bytes---
	unsigned char
		format[16];      // "depth 2d 10b/6b "
	//----16bytes---
};

}//namespace stclient
