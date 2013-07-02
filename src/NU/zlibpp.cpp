#include "zlibpp.h"

static void appendChunkToVector(zlibpp::bytes& dest, const zlibpp::byte* chunk, int chunksize)
{
	const size_t oldsize = dest.size();
	dest.resize(oldsize + chunksize);
	for (int i=0; i<chunksize; ++i)
	{
		dest[oldsize+i] = chunk[i];
	}
}


bool zlibpp::compress(const byte* src, int srcsize, bytes& dest, int compress_level)
{
	dest.clear();

	// allocate deflate
	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree  = Z_NULL;
	strm.opaque = Z_NULL;
	if (Z_OK!=deflateInit(&strm, compress_level))
		return false;

	// Input data
	strm.avail_in = srcsize;
	strm.next_in  = (Bytef*)src;
	
	for (;;)
	{
		// Output
		static byte work[CHUNK];
		strm.avail_out = CHUNK;
		strm.next_out  = work;
		int ret = deflate(&strm, Z_FINISH);
		if (ret==Z_STREAM_ERROR)
		{
			puts("Z_STREAM_ERROR");
		}
		
		// append
		int have = CHUNK - strm.avail_out;
		appendChunkToVector(dest, work, have);

		if (strm.avail_out!=0)
			break;
	}

	// Clean up and return
	(void)deflateEnd(&strm);
	return true;
}


bool zlibpp::decompress(const byte* src, int srcsize, bytes& dest)
{
	dest.clear();

	// Init
	z_stream strm;
	strm.zalloc   = Z_NULL;
	strm.zfree    = Z_NULL;
	strm.opaque   = Z_NULL;
	strm.avail_in = 0;
	strm.next_in  = Z_NULL;
	if (Z_OK!=inflateInit(&strm))
		return false;

	strm.avail_in = srcsize;
	strm.next_in  = (Bytef*)src;

	for (;;)
	{
		static byte work[CHUNK];
		strm.avail_out = CHUNK;
		strm.next_out  = work;
		int ret = inflate(&strm, Z_NO_FLUSH);
		switch (ret)
		{
		case Z_STREAM_ERROR:
			return false;
		case Z_NEED_DICT:
			ret = Z_DATA_ERROR;     /* and fall through */
		case Z_DATA_ERROR:
		case Z_MEM_ERROR:
			(void)inflateEnd(&strm);
			return false;
		}

		// append
		const int have = CHUNK - strm.avail_out;
		appendChunkToVector(dest, work, have);

		if (strm.avail_out!=0)
			break;
	}

	// Clean up and return
	(void)inflateEnd(&strm);
	return true;
}

void zlibpp::printBytes(const bytes& data)
{
	for (size_t i=0; i<data.size(); ++i)
	{
		printf("[%02X]", data[i]);
	}
	putchar('\n');
}

void zlibpp::printChars(const bytes& data)
{
	for (size_t i=0; i<data.size(); ++i)
	{
		putchar(data[i]);
	}
	putchar('\n');
}

bool zlibpp::equalBytes(const bytes& data1, const bytes& data2)
{
	if (data1.size() != data2.size())
		return false;

	for (size_t i=0; i<data1.size(); ++i)
	{
		if (data1[i] != data2[i])
			return false;
	}
	return true;
}

bool zlibpp::compress(const bytes& src, bytes& dest, int compress_level)
{
	return compress(src.data(), (int)src.size(), dest, compress_level);
}

bool zlibpp::decompress(const bytes& src, bytes& dest)
{
	return decompress(src.data(), (int)src.size(), dest);
}
