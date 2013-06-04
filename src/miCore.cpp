#include "miCore.h"
#include <Windows.h>
#pragma comment(lib, "User32.lib")


int Core::getSystemTick()
{
	return (int)timeGetTime();
}

void Core::dialog(const char* title, const char* text)
{
	std::string err;
	err += title;
	if (text!=nullptr)
	{
		err += "\r\n";
		err += text;
	}

	::MessageBoxA(nullptr,
			err.c_str(),
			"ÉGÉâÅ[Ç…ÇÊÇËë±çsÇ≈Ç´Ç‹ÇπÇÒ",
			MB_OK);
}

void Core::abort(const char* title, const char* text)
{
	dialog(title, text);
	exit(-1);
}

const std::string& Core::getComputerName()
{
	static char name[1024];
	DWORD size = sizeof(name);
	::GetComputerNameA(name, &size);

	static std::string res;
	res = name;
	return res;
}
