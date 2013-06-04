#pragma once

#include <vector>
#include <cstdio>

typedef __int8  int8;
typedef __int16 int16;
typedef __int32 int32;
typedef __int64 int64;
typedef unsigned __int8  uint8;
typedef unsigned __int16 uint16;
typedef unsigned __int32 uint32;
typedef unsigned __int64 uint64;
typedef unsigned int uint;
typedef unsigned int IdNumber;

struct RGBA_raw
{
	uint8 r,g,b,a;
};


#ifdef __cplusplus

class File
{
private:
	FILE* fp;

public:
	File() : fp(nullptr) { }
	File(const char* path) : fp(nullptr) { open(path); }
	~File() { close(); }

	bool open(const char* path)
	{
		close();
		return fopen_s(&fp, path, "rb")==0;
	}

	bool openForWrite(const char* path)
	{
		close();
		return fopen_s(&fp, path, "wb")==0;
	}

	void close()
	{
		if (fp!=nullptr)
		{
			fclose(fp);
			fp = nullptr;
		}
	}

	void rewind()    { fseek(fp, 0L, SEEK_SET); }

	bool readStringAndCompare(const char* str)
	{
		const int nbytes = strlen(str);
		std::vector<char> buffer;
		buffer.resize(nbytes);

		if (fread(buffer.data(), buffer.size(), 1, (FILE*)fp)==0)
			return false;
		if (memcmp(buffer.data(), str, nbytes)!=0)
			return false;
		return true;
	}

	uint8 get8()     { return (uint8)fgetc(fp); }
	uint16 get16()   { uint16 x = get8 (); x |= get8 ()<< 8; return x; }
	uint32 get32()   { uint32 x = get16(); x |= get16()<<16; return x; }
	bool eof()       { return feof(fp)!=0; }

	void read(void* buffer, int size)
	{
		int readbytes = (int)fread(buffer, 1, size, fp);
		if (readbytes!=size)
		{
			printf("read()...%d, %d\n", readbytes, size);
		}
	}

	void readStringA(std::string& dest, int length)
	{
		dest.clear();
		for (int i=0; i<length; ++i)
		{
			dest += (char)get8();
		}
	}

	void readAbandon(int nbytes)
	{
		for (int i=0; i<nbytes; ++i)
		{
			get8();
		}
	}
};


class Core
{
public:
	static int getSystemTick();
	static void dialog(const char* title, const char* text=nullptr);
	static void abort(const char* title, const char* text=nullptr);
	static const std::string& getComputerName();
};


#endif
