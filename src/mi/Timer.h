#pragma once
#include <string>


namespace mi{

class Timer
{
public:
	static double getMsec();

	Timer();
	Timer(double* dest_ref);

	virtual ~Timer();

	operator   double() const { return get(); }
	double operator()() const { return get(); }
	double        get() const;

private:
	double _begin;
	double* _dest_ptr;
};

}//namespace mi
