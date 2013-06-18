#include "ST_Client.h"
#include "mi/Image.h"
#include "mi/Udp.h"
#include "mi/Libs.h"
#include "mi/Timer.h"
#pragma warning(disable: 4996)


using namespace mi;
using namespace stclient;


bool commandIs(const std::string& cmd,
		const char* cmd1,
		const char* cmd2=nullptr,
		const char* cmd3=nullptr)
{
	if (cmd1!=nullptr && cmd.compare(cmd1)==0)
		return true;
	if (cmd2!=nullptr && cmd.compare(cmd2)==0)
		return true;
	if (cmd3!=nullptr && cmd.compare(cmd3)==0)
		return true;
	return false;
}

enum InvalidFormat
{
	INVALID_FORMAT,
};

typedef const std::vector<VariantType> Args;


class Command
{
public:
	Command(mi::UdpSender& send) :
		udp_send(&send)
	{
	}

	bool command(const std::string& line);
	void sendStatus();

private:
	void commandDepth(Args& arg);
	void commandBlack(Args& arg);
	void commandStatus(Args& arg);
	void commandStart(Args& arg);
	void commandPing(Args& arg);
	void commandIdent(Args& arg);

	mi::UdpSender* udp_send;
};



void arg_check(Args& arg, size_t x)
{
	if (arg.size()!=x)
		throw INVALID_FORMAT;
}


void Command::commandDepth(Args& arg)
{
	arg_check(arg, 0);
	global.client_status = STATUS_DEPTH;
}

void Command::commandBlack(Args& arg)
{
	arg_check(arg, 0);
	global.client_status = STATUS_BLACK;
}

void commandMirror(Args& arg)
{
	arg_check(arg, 0);
	toggle(mode.mirroring);
}

void commandDiskInfo(Args& arg)
{
	arg_check(arg, 0);

	static const ULARGE_INTEGER zero = {};
	ULARGE_INTEGER free_bytes;
	ULARGE_INTEGER total_bytes;

	if (GetDiskFreeSpaceEx("C:", &free_bytes, &total_bytes, nullptr)==0)
	{
		free_bytes  = zero;
		total_bytes = zero;
		fprintf(stderr, "(error) GetDiskFreeSpaceEx\n");
	}

	auto mega_bytes = [](ULARGE_INTEGER ul)->uint32{
		const uint64 size = ((uint64)ul.HighPart<<32) | ((uint64)ul.LowPart);
		return (uint32)(size / 1000000);
	};

	uint32 free  = mega_bytes(free_bytes);
	uint32 total = mega_bytes(total_bytes);
	printf("%u MB free(%.1f%%), %u MB total\n",
			free,
			free*100.0f/total,
			total);
}

static void commandReloadConfig(Args& arg)
{
	arg_check(arg, 0);
	load_config();
}

void Command::sendStatus()
{
	const auto st = global.client_status;

	std::string s;
	s += "STATUS ";
	s += Core::getComputerName();
	s += " ";
	s += (
		(st==STATUS_BLACK) ? "BLACK" :
		(st==STATUS_PICTURE) ? "PICTURE" :
		(st==STATUS_IDLE) ? "IDLE" :
		(st==STATUS_GAMEREADY) ? "GAMEREADY" :
		(st==STATUS_GAME) ? "GAME" :
		(st==STATUS_DEPTH) ? "DEPTH" :
		(st==STATUS_GAMESTOP) ? "GAMESTOP" :
		(st==STATUS_TIMEOUT) ? "TIMEOUT" :
		(st==STATUS_GOAL) ? "GOAL" :
			"UNKNOWN-STATUS");
	udp_send->send(s);
}

void Command::commandStatus(Args& arg)
{
	arg_check(arg, 0);
	sendStatus();
}

void Command::commandStart(Args& arg)
{
	arg_check(arg, 0);

	global.client_status = STATUS_GAME;
	sendStatus();
}

static void commandBorderLine(Args& arg)
{
	arg_check(arg, 0);
	toggle(mode.borderline);
}


const char* to_s(int x)
{
	static char to_s_buf[1000];
	_ltoa(x, to_s_buf, 10);
	return to_s_buf;
}

void Command::commandPing(Args& arg)
{
	arg_check(arg, 1);

	printf("PING received: server is '%s'\n", arg[0].to_s());

	std::string s;
	s += "PONG ";
	s += Core::getComputerName();
	s += " ";
	s += mi::Udp::getIpAddress();
	s += " ";
	s += to_s(config.client_number);

	udp_send->init(arg[0].to_s(), UDP_SERVER_RECV);
	udp_send->send(s);
}

void commandPict(Args& arg)
{
	arg_check(arg, 1);

	std::string path;
	path += "//STMX64/ST/Picture/";
	path += arg[0].to_s();
	if (global.pic.createFromImageA(path.c_str()))
	{
		global.client_status = STATUS_PICTURE;
		return;
	}
	else
	{
		printf("picture load error. %s\n", arg[0].to_s());
	}
}


void Command::commandIdent(Args& arg)
{
	arg_check(arg, 2);


	printf("%s, %s\n", arg[0].to_s(), arg[1].to_s());
	
	std::string filename = (arg[0].string() + "-" + arg[1].string());
	if (!global.save_file.openForWrite(filename.c_str()))
	{
		puts("Open error (savefile)");
		return;
	}

	printf("Filename %s ok\n", filename.c_str());

	global.client_status = STATUS_GAMEREADY;
	sendStatus();
}

void commandBye(Args& arg)
{
	arg_check(arg, 0);
	exit(0);
}

void commandHitBoxes(Args& arg)
{
	arg_check(arg, 0);
	toggle(mode.show_hit_boxes);
}


bool Command::command(const std::string& line)
{
	std::string cmd;
	std::vector<VariantType> arg;
	Lib::splitString(line, cmd, arg);

	// Daemon command
	if (cmd[0]=='#')
	{
		printf("[DAEMON COMMAND] '%s' -- ignore", cmd.c_str());
		return true;
	}


	printf("[UDP COMMAND] '%s' ", cmd.c_str());
	for (size_t i=0; i<arg.size(); ++i)
	{
		if (arg[i].is_int())
		{
			printf("[int:%d]", arg[i].to_i());
		}
		else
		{
			printf("[str:%s]", arg[i].to_s());
		}
	}
	printf("\n");

	const int argc = arg.size();

	try
	{
		// @command
#define COMMAND(CMD, PROC)    if (cmd.compare(CMD)==0) { PROC(arg); return true; }
		COMMAND("HITBOXES", commandHitBoxes);
		COMMAND("RELOADCONFIG", commandReloadConfig);

		COMMAND("DISKINFO", commandDiskInfo);
		COMMAND("MIRROR",   commandMirror);
		COMMAND("BLACK",    commandBlack);
		COMMAND("DEPTH",    commandDepth);
		COMMAND("STATUS",   commandStatus);
		COMMAND("START",    commandStart);

		COMMAND("BORDERLINE", commandBorderLine);
		COMMAND("IDENT",    commandIdent);
		COMMAND("PING",     commandPing);
		COMMAND("PICT",     commandPict);
		COMMAND("BYE",      commandBye);
		COMMAND("QUIT",     commandBye);
		COMMAND("EXIT",     commandBye);
#undef COMMAND

		printf("Invalid udp-command '%s'\n", cmd.c_str());
	}
	catch (InvalidFormat)
	{
		printf("Invalid format '%s' argc=%d\n", cmd.c_str(), argc);
	}

	return false;
}

bool StClient::doCommand()
{
	std::string rawstring;
	if (udp_recv.receive(rawstring)<=0)
	{
		return false;
	}

	std::vector<std::string> lines;
	Lib::splitStringToLines(rawstring, lines);

	for (size_t i=0; i<lines.size(); ++i)
	{
		Command cmd(udp_send);
		cmd.command(lines[i]);
	}
	return true;
}
