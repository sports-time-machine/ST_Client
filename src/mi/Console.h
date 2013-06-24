#pragma once
#include "Core.h"


namespace mi{

enum ConsoleColor
{
	CON_BLACK = 0,
	CON_DARKBLUE,
	CON_DARKGREEN,
	CON_DARKCYAN,
	CON_DARKRED,
	CON_PURPLE,
	CON_OCHRE,
	CON_SILVER,
	CON_GRAY,
	CON_BLUE,
	CON_GREEN,
	CON_CYAN,
	CON_RED,
	CON_MAGENTA,
	CON_YELLOW,
	CON_WHITE,
};

class Console
{
public:
	static void setTitle(const char* title);
	static void setTitle(const wchar_t* title);
	static void setColor(int color);
	static void pushColor(int color);
	static void popColor();
	static void printf(int color, const char* f, ...);
	static void puts(int color, const char* s);
	static void nl();
};

}//namespace mi
