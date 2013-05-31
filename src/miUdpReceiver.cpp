#include "miUdpReceiver.h"
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")



const std::string& miUdp::getComputerName()
{
	static char name[1024];
	DWORD size = sizeof(name);
	::GetComputerNameA(name, &size);

	static std::string res;
	res = name;
	return res;
}

const std::string& miUdp::getIpAddress()
{
	static std::string res;

	char ac[80];
	if (gethostname(ac, sizeof(ac))==SOCKET_ERROR)
	{
		res = "(socket error)";
		return res;
	}

	hostent* hosts = gethostbyname(ac);
	if (hosts==nullptr)
	{
		res = "(no hosts detected)";
		return res;
	}

	res = "(no host)";
	for (int i=0; hosts->h_addr_list[i]!=nullptr; ++i)
	{
		in_addr addr;
		memcpy(&addr, hosts->h_addr_list[i], sizeof(struct in_addr));
		const char* addr_string = inet_ntoa(addr);
		if (i==0)
		{
			res = addr_string;
		}
		else if (i>0)
		{
			fprintf(stderr,"(notice) too many nic found: '%s'\n", addr_string);
		}
	}
 
	return res;
}



struct miUdp::Impl
{
	bool enabled;
	WSAData wsa;
};

miUdp::miUdp()
{
	self = new Impl;
	self->enabled = false;
}

miUdp::~miUdp()
{
	destroy();
}

void miUdp::init()
{
	if (!self->enabled)
	{
		self->enabled = true;
		::WSAStartup(MAKEWORD(2,0), (WSAData*)&self->wsa);
	}
}

void miUdp::destroy()
{
	if (self->enabled)
	{
		::WSACleanup();
	}
	delete self;
}





struct miUdpReceiver::Impl
{
	SOCKET sock;
};

miUdpReceiver::miUdpReceiver()
{
	self = new Impl;
}

miUdpReceiver::~miUdpReceiver()
{
	delete self;
}

void miUdpReceiver::init(int port)
{
	miUdp::get().init();

	self->sock = ::socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons((WORD)port);
	addr.sin_addr.S_un.S_addr = INADDR_ANY;
	::bind(self->sock, (sockaddr*)&addr, sizeof(addr));

	// non-blocking mode
	DWORD val = 1;
	::ioctlsocket(self->sock, FIONBIO, &val);
}

void miUdpReceiver::destroy()
{
	::closesocket(self->sock);
}

int miUdpReceiver::receive(std::string& dest)
{
	const int NO_FLAGS = 0;
	bool received = false;
	int length = 0;

	dest.clear();
	for (;;)
	{
		static char buf[65536];
		int bytes = ::recv(self->sock, buf, sizeof(buf)-1, NO_FLAGS);
		if (bytes==SOCKET_ERROR || bytes==0)
		{
			break;
		}

		buf[bytes] = '\0';
		dest += buf;
		length += dest.length();
		received = true;
	}

	return received ? length : -1;
}



struct miUdpSender::Impl
{
	SOCKET sock;
	sockaddr_in send_addr;
};

miUdpSender::miUdpSender()
{
	self = new Impl;
}

miUdpSender::~miUdpSender()
{
	delete self;
}

void miUdpSender::init(const char* address, int port)
{
	miUdp::get().init();

	destroy();

	self->sock = ::socket(AF_INET, SOCK_DGRAM, 0);

	self->send_addr.sin_family = AF_INET;
	self->send_addr.sin_port = ::htons((WORD)port);
	self->send_addr.sin_addr.S_un.S_addr = ::inet_addr(address);
}

void miUdpSender::destroy()
{
	::closesocket(self->sock);
}

bool miUdpSender::send(const std::string& src)
{
	const int NO_FLAGS = 0;
	int bytes = ::sendto(self->sock, src.c_str(), src.length(),
			NO_FLAGS,  (const struct sockaddr*)&self->send_addr, sizeof(self->send_addr));
	if (bytes==SOCKET_ERROR)
	{
		return false;
	}
	return true;
}
