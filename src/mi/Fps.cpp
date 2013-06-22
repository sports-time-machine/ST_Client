#include "Fps.h"
#include "Timer.h"

using namespace mi;

Fps::Fps()
{
	_frame_index = 0;
}

void Fps::update()
{
	_frame_index = (_frame_index+1) % SIZE;
	_frame_tick[_frame_index] = Timer::getMsec();
}

float Fps::getFps() const
{
	const int tail_index = (_frame_index - 1 + SIZE) % SIZE;
	const double tail = _frame_tick[tail_index];
	const double head = _frame_tick[_frame_index];
	if (tail==0)
	{
		return 0;
	}
	else
	{
		return (float)(1000.0 / (head-tail));
	}
}
