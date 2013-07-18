#include "Console.h"
#include <Windows.h>
#include <list>
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


struct ConsoleAttrib
{
	int color;

	ConsoleAttrib()
	{
		color = CON_SILVER;
	}
};

static ConsoleAttrib last_attrib;

std::list<ConsoleAttrib> _attrib;

void Console::pushColor(int col)
{
	_attrib.push_back(last_attrib);
	setColor(col);
}

void Console::popColor()
{
	if (_attrib.size()!=0)
	{
		const ConsoleAttrib& a = _attrib.back();
		setColor(a.color);
		_attrib.pop_back();
	}
}

void Console::setColor(int col)
{
	auto cout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (cout!=nullptr)
	{
		SetConsoleTextAttribute(cout, (WORD)col);
		last_attrib.color = col;
	}
}

void Console::printf(int color, const char* f, ...)
{
	char buffer[1024];
	va_list vp;
	va_start(vp, f);
	vsnprintf_s(buffer, sizeof(buffer), f, vp);
	Console::pushColor(color);
	fputs(buffer, stderr);
	Console::popColor();
}

void Console::puts(int color, const std::string& s)
{
	Console::pushColor(color);
	fputs(s.c_str(), stderr);
	nl();
	Console::popColor();
}

void Console::nl()
{
	fputs("\n", stderr);
}

void* Console::getConsoleHwnd()
{
	const int buffer_size = 1024;
	static char new_title[buffer_size];
	static char old_title[buffer_size];

	GetConsoleTitle(old_title, buffer_size);
	wsprintf(new_title,"%d/%d", GetTickCount(), GetCurrentProcessId());
	SetConsoleTitle(new_title);
	Sleep(40);
	HWND hwndFound = FindWindow(NULL, new_title);
	SetConsoleTitle(old_title);
	return static_cast<void*>(hwndFound);
}
