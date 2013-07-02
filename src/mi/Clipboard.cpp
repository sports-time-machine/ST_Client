#include "Clipboard.h"
#include <Windows.h>

using namespace mi;

static void ClipboardAux(const std::string& s)
{
	HGLOBAL mem = ::GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, s.length()+1);
	if (mem==NULL)
	{
		puts("mem is NULL");;
		return;
	}

	char* text = (char*)::GlobalLock(mem);
	if (text==NULL)
	{
		puts("text is NULL");;
		return;
	}

	::lstrcpyA(text, s.c_str());
	::GlobalUnlock(mem);
	::SetClipboardData(CF_TEXT, text);
	::GlobalFree(mem);
}

void Clipboard::setText(const std::string& s)
{
	// クリップボード開きたい
	int tries = 0;
	for (;;)
	{
		if (OpenClipboard(NULL)!=0)
		{
			puts("Open clipboard");
			// okay
			break;
		}
		++tries;
		if (tries>=20)
		{
			puts("Can't open clipboard!");;
			return;
		}
		Sleep(1);
	}

	try {
		ClipboardAux(s);
	} catch (...) {
		puts("Clipboard fault.");
	}

	::CloseClipboard();

	puts("Close Clipboard");
}
