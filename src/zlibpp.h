#pragma once

#ifdef _M_X64
#pragma comment(lib,"zlib_x64.lib")
#else
#pragma comment(lib,"zdll_x32.lib")
#endif

#include "zlib.h"
#include <vector>

class zlibpp
{
public:
	static const int CHUNK = 1024 * 128;
	typedef unsigned __int8 byte;
	typedef std::vector<byte> bytes;

public:
	static bool compress(byte* src, int srcsize, bytes& dest, int compress_level = Z_DEFAULT_COMPRESSION);
	static bool compress(bytes& src, bytes& dest, int compress_level = Z_DEFAULT_COMPRESSION);
	static bool decompress(byte* src, int srcsize, bytes& dest);
	static bool decompress(bytes& src, bytes& dest);
	static void printBytes(const bytes& data);
	static void printChars(const bytes& data);
	static bool equalBytes(const bytes& data1, const bytes& data2);
	static bool compress(byte* src, size_t srcsize, bytes& dest)      { return compress(src, (int)srcsize, dest); }
	static bool decompress(byte* src, size_t srcsize, bytes& dest)    { return decompress(src, (int)srcsize, dest); }
};
