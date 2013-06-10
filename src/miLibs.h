#pragma once
#include "miCore.h"


class VariantType
{
public:
	VariantType(const std::string&);
	int                  to_i() const { return intvalue; }
	const char*          to_s() const { return strvalue.c_str(); }
	bool               is_int() const { return is_intvalue; }
	const std::string& string() const { return strvalue; }

private:
	std::string strvalue;
	int intvalue;
	bool is_intvalue;
};


class Lib
{
public:
	static void splitStringToLines(const std::string& rawstring, std::vector<std::string>& lines);
	static bool splitString(const std::string& rawstring, std::string& cmd, std::vector<VariantType>& arg);
};



