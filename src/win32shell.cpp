#include "mi/Core.h"
#include <Shlobj.h>

const std::string& mi::Core::getDesktopFolder()
{
	char folder[1024];
	SHGetSpecialFolderPath(nullptr, folder, CSIDL_DESKTOP, false);
	
	static std::string res;
	res = folder;
	return res;
}
