// 概ね汎用なUDPクラス

#include "Udp.h"
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")

using namespace mi;

const std::string& Udp::getIpAddress()
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



struct Udp::Impl
{
	bool enabled;
	WSAData wsa;
};

Udp::Udp()
{
	self = new Impl;
	self->enabled = false;
}

Udp::~Udp()
{
	destroy();
}

void Udp::init()
{
	if (!self->enabled)
	{
		self->enabled = true;
		::WSAStartup(MAKEWORD(2,0), (WSAData*)&self->wsa);
	}
}

void Udp::destroy()
{
	if (self->enabled)
	{
		::WSACleanup();
	}
	delete self;
}





struct UdpReceiver::Impl
{
	SOCKET sock;
};

UdpReceiver::UdpReceiver()
{
	self = new Impl;
}

UdpReceiver::~UdpReceiver()
{
	delete self;
}

void UdpReceiver::init(int port)
{
	Udp::get().init();

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

void UdpReceiver::destroy()
{
	::closesocket(self->sock);
}

int UdpReceiver::receive(std::string& dest)
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



struct UdpSender::Impl
{
	SOCKET sock;
	sockaddr_in send_addr;
};

UdpSender::UdpSender()
{
	self = new Impl;
}

UdpSender::~UdpSender()
{
	delete self;
}


unsigned long GetInetAddress(const char* src)
{
	typedef std::map<std::string,int> Cache;
	static Cache cache;

	// キャッシュしていればそのアドレスを使う
	std::string s = src;
	Cache::const_iterator itr = cache.find(s);
	if (itr!=cache.end())
	{
		return itr->second;
	}

	const auto ipv4 = inet_addr(src);
	if (ipv4==0xFFFFFFFFu)
	{
		// 名前を引く
		const hostent* host = gethostbyname(src);
		if (host!=nullptr)
		{
			for (int i=0; host->h_addr_list[i]!=nullptr; ++i)
			{
				const BYTE* addr = (const BYTE*)host->h_addr_list[i];
				fprintf(stderr, "GetInetAddress: %s is %d.%d.%d.%d\n",
					src, addr[0], addr[1], addr[2], addr[3]);
				const unsigned long ipa = (addr[0]) + (addr[1]<<8) + (addr[2]<<16) + (addr[3]<<24);
				cache[src] = ipa;
				return ipa;
			}
		}
	}

	cache[src] = ipv4;
	return ipv4;
}

void UdpSender::init(const char* address, int port)
{
	Udp::get().init();

	destroy();

	self->sock = ::socket(AF_INET, SOCK_DGRAM, 0);
	self->send_addr.sin_family = AF_INET;
	self->send_addr.sin_port = ::htons((WORD)port);
	self->send_addr.sin_addr.S_un.S_addr = GetInetAddress(address);
}

void UdpSender::destroy()
{
	::closesocket(self->sock);
}

bool UdpSender::send(const std::string& src)
{
	// STでのUDPは行ごとに判定するのでnewlineを追加する
	std::string buf = src;
	buf += "\n";

	const int NO_FLAGS = 0;
	int bytes = ::sendto(self->sock, buf.c_str(), buf.length(),
			NO_FLAGS,  (const struct sockaddr*)&self->send_addr, sizeof(self->send_addr));
	if (bytes==SOCKET_ERROR)
	{
		fprintf(stderr, "send error, msg:[%s]\n", src.c_str());
		return false;
	}
	return true;
}
