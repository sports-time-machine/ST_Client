#include "miUdpReceiver.h"
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")


struct miUdpReceiver::Impl
{
	WSAData  wsa;
	SOCKET   sock;
};

miUdpReceiver::miUdpReceiver()
{
	self = nullptr;
}

miUdpReceiver::~miUdpReceiver()
{
	delete self;
}

void miUdpReceiver::init(int port)
{
	self = new Impl;

	::WSAStartup(MAKEWORD(2,0), (WSAData*)&self->wsa);

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
	::WSACleanup();
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
