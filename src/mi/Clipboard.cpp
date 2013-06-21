#include "Clipboard.h"
#include <Windows.h>

using namespace mi;

void Clipboard::setText(const std::string& s)
{
	HGLOBAL mem = ::GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, s.length()+1);
	char* text = (char*)::GlobalLock(mem);
	::lstrcpyA(text, s.c_str());
	::GlobalUnlock(mem);

	::OpenClipboard(NULL);
	::EmptyClipboard();
	::SetClipboardData(CF_TEXT, text);
	::CloseClipboard();

	::GlobalFree(mem);
}
