#include "miCore.h"
#include <Windows.h>


int Core::getSystemTick()
{
	return (int)timeGetTime();
}
