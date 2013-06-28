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
			"エラーにより続行できません",
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
	char first  = full_folder_name[0];
	char second = full_folder_name[1];

	// //SERVERNAME/folder/folder/folder...
	bool netfolder = false;
	if ((first=='/' || first=='\\') && (second=='/' || second=='\\'))
	{
		netfolder = true;
		full_folder_name += 2;
	}

	Lib::splitByChars(full_folder_name, "/\\", lines);

	std::string folder;
	if (netfolder)
	{
		folder += "//";
	}
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
