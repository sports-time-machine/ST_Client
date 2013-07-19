#include "St3dData.h"
#include "mi/Console.h"

using namespace mi;
using namespace stclient;


static void _Msg(int color, const string& s, const string& param)
{
	Console::printf(color, "%s - %s\n", s.c_str(), param.c_str());
}

void Msg::BarMessage(const string& s, int width, int first_half)
{
	Console::pushColor(CON_CYAN);
	for (int i=0; i<first_half; ++i)
	{
		putchar('=');
	}
	printf(" %s ", s.c_str());
	
	const int second_half = width - 2 - s.length() - first_half;
	for (int i=0; i<second_half; ++i)
	{
		putchar('=');
	}
	putchar('\n');
	Console::popColor();
}

void Msg::Notice       (const string& s)                       { Console::puts(CON_CYAN,  s); }
void Msg::SystemMessage(const string& s)                       { Console::puts(CON_GREEN, s); }
void Msg::ErrorMessage (const string& s)                       { Console::puts(CON_RED,   s); }
void Msg::Notice       (const string& s, const string& param)  { _Msg(CON_CYAN,  s, param); }
void Msg::SystemMessage(const string& s, const string& param)  { _Msg(CON_GREEN, s, param); }
void Msg::ErrorMessage (const string& s, const string& param)  { _Msg(CON_RED,   s, param); }
