#pragma once
#include "Core.h"

namespace mi{

class Fps
{
public:
	Fps();

	void update();
	float getFps() const;

private:
	static const int SIZE = 180;
	double _frame_tick[SIZE];
	int _frame_index;
};

}//namespace mi
