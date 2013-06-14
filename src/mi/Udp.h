#pragma once
#include "Core.h"

namespace mi{

class Udp
{
public:
	static const std::string& getIpAddress();

	static Udp& get() { static Udp obj; return obj; }

	virtual ~Udp();
	void init();
	void destroy();

private:
	Udp();

	struct Impl;
	Impl* self;
};

class UdpReceiver
{
public:
	UdpReceiver();
	virtual ~UdpReceiver();

	void init(int port);
	void destroy();

	int receive(std::string& dest);

private:
	struct Impl;
	Impl* self;
};

class UdpSender
{
public:
	UdpSender();
	virtual ~UdpSender();

	void init(const char* addr, int port);
	void destroy();

	bool send(const std::string& src);

private:
	struct Impl;
	Impl* self;
};

}//namespace mi
