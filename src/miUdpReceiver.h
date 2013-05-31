#include "miCore.h"

class miUdp
{
public:
	static const std::string& getComputerName();
	static const std::string& getIpAddress();

	static miUdp& get() { static miUdp obj; return obj; }

	~miUdp();
	void init();
	void destroy();

private:
	miUdp();

	struct Impl;
	Impl* self;
};

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

class miUdpSender
{
public:
	miUdpSender();
	~miUdpSender();

	void init(const char* addr, int port);
	void destroy();

	bool send(const std::string& src);

private:
	struct Impl;
	Impl* self;
};
