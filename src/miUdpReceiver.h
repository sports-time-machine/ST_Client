#include "miCore.h"

class miUdpReceiver
{
public:
	miUdpReceiver();
	~miUdpReceiver();

	void init(int port);
	void destroy();

	int receive(std::string& dest);

private:
	struct Impl;
	Impl* self;
};
