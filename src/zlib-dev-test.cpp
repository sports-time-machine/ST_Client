#include "stdafx.h"
#include "zlibpp.h"
#pragma comment(lib,"winmm.lib")
#include <Windows.h>

using namespace std;

void testCompress()
{
	const int SIZE = 640 * 480;
	unsigned char* src = new unsigned char[SIZE];
	for (int i=0; i<SIZE; ++i)
	{
//		src[i] = (((i*i+33*i)>>4)%3) * (i%50);
		src[i] = i*5/33 + (i%7==0 ? 100 : 0);
	}

#if 0
	for (int i=0; i<SIZE; ++i)
	{
		printf("%02X ", src[i]);
	}
#endif

	for (int i=1; i<=9; ++i)
	{
		vector<byte> data;
		long tm = timeGetTime();
		zlibpp::compress(src, SIZE, data);
		printf("%d, %d -> %d (%.1f%%) (%d msec)\n",
			i, SIZE, data.size(),
			100.0 * data.size() / SIZE,
			timeGetTime()-tm);
	}

	delete[] src;
}

void testDecompress()
{
	const char* str = "heeee! haaaa! hello world!!!!! i am zlib for cplusplus wrapper. my name is zlibpp. thanks. wooooaaaaaa!";

	printf("%s\n", str);

	zlibpp::bytes compressed;
	zlibpp::compress((zlibpp::byte*)str, strlen(str), compressed);
	zlibpp::printBytes(compressed);

	zlibpp::bytes raw;
	zlibpp::decompress(compressed.data(), compressed.size(), raw);
	zlibpp::printBytes(raw);
	zlibpp::printChars(raw);
}


int main()
{
	testCompress();
	testDecompress();
	fgetc(stdin);
	return 0;
}
