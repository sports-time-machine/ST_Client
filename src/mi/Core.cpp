#include "Core.h"
#include "Libs.h"
#include <Windows.h>
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "winmm.lib")

using namespace mi;


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



void Folder::createFolder(const char* full_folder_name)
{
	std::vector<std::string> lines;
	Lib::splitByChars(full_folder_name, "/\\", lines);

	std::string folder;
	for (size_t i=0; i<lines.size(); ++i)
	{
		folder += lines[i];
		folder += "/";
		if (CreateDirectory(folder.c_str(), nullptr)!=0)
		{
			//success
			fprintf(stderr, "Folder created: '%s'\n", folder.c_str());
		}
	}
}


const char* mi::boolToYesNo(bool x)
{
	return x ? "YES" : "NO";
}
