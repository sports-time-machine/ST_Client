#include "mi/Core.h"
#include <Shlobj.h>

std::string mi::Core::getDesktopFolder()
{
	char folder[1024];
	SHGetSpecialFolderPath(nullptr, folder, CSIDL_DESKTOP, false);
	return folder;
}

std::string mi::Core::getAppFolder()
{
	char _appname[1000];
	GetModuleFileName(NULL, _appname, sizeof(_appname));

	std::string appname = _appname;
	auto pos = appname.rfind('\\');
	if (pos!=appname.npos)
	{
		appname = appname.substr(0, pos+1);
	}
	return appname;
}
