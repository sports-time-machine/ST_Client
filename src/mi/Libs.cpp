#include "Libs.h"
#include <cctype>
#include <cstdlib>     // _ltoa_s

using namespace mi;


VariantType::VariantType(const std::string& s)
{
	strvalue = s;
	char* endptr = nullptr;
	intvalue = (int)strtol(s.c_str(), &endptr, 0);
	is_intvalue = (*endptr=='\0');
}


void Lib::splitByChars(const std::string& rawstring, const char* chars, std::vector<std::string>& lines)
{
	lines.clear();

	const char* src = rawstring.c_str();

	while (*src!='\0')
	{
		if (strchr(chars,*src)!=nullptr)
		{
			++src;
			continue;
		}

		std::string line;
		while (*src!='\0' && strchr(chars,*src)==nullptr)
		{
			line += *src++;
		}
		lines.push_back(line);
	}
}

void Lib::splitStringToLines(const std::string& rawstring, std::vector<std::string>& lines)
{
	lines.clear();

	const char* src = rawstring.c_str();

	while (*src!='\0')
	{
		// blank
		if (*src=='\n' || *src=='\r')
		{
			++src;
			continue;
		}

		std::string line;
		while (*src!='\0' && *src!='\n' && *src!='\r')
		{
			line += *src++;
		}
		lines.push_back(line);
	}
}

bool Lib::splitString(const std::string& rawstring, std::string& cmd, std::vector<VariantType>& arg)
{
	cmd.clear();
	arg.clear();

	const char* src = rawstring.c_str();

	auto skip_whitespaces = [&](){
		while (*src!='\0' && isspace(*src))
		{
			++src;
		}
	};

	auto word_copy_to_dest = [&](std::string& dest, bool upper)->bool{
		dest.clear();
		skip_whitespaces();
		if (*src=='\0')
			return false;

		// Create 'cmd'
		while (*src && !isspace(*src))
		{
			dest += upper ? (char)toupper(*src) : *src;
			++src;
		}
		return true;
	};

	if (!word_copy_to_dest(cmd, true))
		return false;

	// Create 'args'
	for (;;)
	{
		std::string temp;
		if (!word_copy_to_dest(temp, false))
			break;
		arg.push_back(temp);
	}
	return true;
}

const char* Lib::to_s(int x)
{
	static char to_s_buf[1000];
	_ltoa_s(x, to_s_buf, 10);
	return to_s_buf;
}

const char* Lib::to_s_num0(int x, int n)
{
	static char to_s_buf[1000];
	sprintf_s(to_s_buf, "%0*d", n, x);
	return to_s_buf;
}
