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


void Folder::createFolder(const std::string& full_folder_name, bool filename_included)
{
	// ignore - null string
	if (full_folder_name.length()==0)
		return;

	std::vector<std::string> lines;
	const char first  = full_folder_name[0];
	const char second = full_folder_name[1];
	const char last   = full_folder_name[full_folder_name.length()-1];

	// ^folder/folder/folder/$
	if (last=='/' || last=='\\')
	{
		filename_included = false;
	}

	// //SERVERNAME/folder/folder/folder...
	bool netfolder = false;
	int head = 0;
	if ((first=='/' || first=='\\') && (second=='/' || second=='\\'))
	{
		netfolder = true;
		head = 2;
	}

	Lib::splitByChars(full_folder_name.c_str()+head, "/\\", lines);

	std::string folder;
	if (netfolder)
	{
		folder += "//";
	}

	size_t size = lines.size() - (filename_included ? 1 : 0);

	for (size_t i=0; i<size; ++i)
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
