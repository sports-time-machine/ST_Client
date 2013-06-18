#pragma once
#include "Core.h"


namespace mi{

class Console
{
public:
	static void setTitle(const char* title);
	static void setTitle(const wchar_t* title);
};

}//namespace mi
