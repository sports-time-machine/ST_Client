#include "Timer.h"
#include <windows.h>

using namespace mi;

Timer::Timer()
{
	this->_begin = getMsec();
}

Timer::Timer(double* ptr)
{
	this->_begin = getMsec();
	this->_dest_ptr = ptr;
}

Timer::~Timer()
{
	if (_dest_ptr!=nullptr)
	{
		*_dest_ptr = get();
	}
}

double Timer::get() const
{
	return getMsec() - _begin;
}


double Timer::getMsec()
{
	static double mul_to_msec = 0.0;
	static bool init = false;

	if (!init)
	{
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		mul_to_msec = 1000.0 / freq.QuadPart;
	}

	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);

	// Milli-seconds
	return (double)counter.QuadPart * mul_to_msec;
}
