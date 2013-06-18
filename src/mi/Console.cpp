#include "Console.h"
#include <Windows.h>
#pragma comment(lib, "User32.lib")

using namespace mi;

void Console::setTitle(const char* title)
{
	::SetConsoleTitleA(title);
}

void Console::setTitle(const wchar_t* title)
{
	::SetConsoleTitleW(title);
}
