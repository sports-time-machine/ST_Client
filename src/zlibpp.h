#pragma once
#include "zlib.h"
#include <vector>

#ifdef _M_X64
#pragma comment(lib,"zlib_x64.lib")
#else
#pragma comment(lib,"zdll_x32.lib")
#endif


class zlibpp
{
public:
	static const int CHUNK = 1024 * 128;
	typedef unsigned __int8 byte;
	typedef std::vector<byte> bytes;

public:
	static bool compress(const byte* src, int srcsize, bytes& dest, int compress_level = Z_DEFAULT_COMPRESSION);
	static bool compress(const bytes& src, bytes& dest, int compress_level = Z_DEFAULT_COMPRESSION);
	static bool decompress(const byte* src, int srcsize, bytes& dest);
	static bool decompress(const bytes& src, bytes& dest);
	static void printBytes(const bytes& data);
	static void printChars(const bytes& data);
	static bool equalBytes(const bytes& data1, const bytes& data2);
	static bool compress(const byte* src, size_t srcsize, bytes& dest)      { return compress(src, (int)srcsize, dest); }
	static bool decompress(const byte* src, size_t srcsize, bytes& dest)    { return decompress(src, (int)srcsize, dest); }
};
