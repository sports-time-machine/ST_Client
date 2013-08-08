#pragma once
#include "Core.h"

namespace mi{

class VariantType
{
	typedef std::string String;

public:
	VariantType(const String&);
	int             to_i() const { return intvalue; }
	const char*     to_s() const { return strvalue.c_str(); }
	bool          is_int() const { return is_intvalue; }
	const String& string() const { return strvalue; }

private:
	String strvalue;
	int intvalue;
	bool is_intvalue;
};


static inline int minmax(int x, int min, int max)
{
	return (x<min) ? min : (x>max) ? max : x;
}

static inline uint8 uint8crop(int x)
{
	return (x<0) ? 0 : (x>255) ? 255 : (uint8)x;
}

struct Box
{
	int left,top,right,bottom;
	void set(int a, int b, int c, int d)
	{
		left = a;
		top = b;
		right = c;
		bottom = d;
	}
};

struct Point
{
	int x,y;
	Point() : x(0),y(0) { }
	Point(int a, int b) : x(a),y(b) { }

	bool in(const Box& box) const
	{
		return x>=box.left && y>=box.top && x<=box.right && y<=box.bottom;
	}
};

class Lib
{
public:
	static void splitByChars(const std::string& rawstring, const char* chars, std::vector<std::string>& lines);
	static void splitStringToLines(const std::string& rawstring, std::vector<std::string>& lines);
	static bool splitString(const std::string& rawstring, std::string& cmd, std::vector<VariantType>& arg);

	static void toggle(bool& ref)     { ref = !ref; }

	static const char* to_s(int x);
	static const char* to_s_num0(int x, int n);
};

}//namespace mi
