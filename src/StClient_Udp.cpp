#include "StClient.h"
#include "mi/Image.h"
#include "mi/Udp.h"
#include "mi/Libs.h"
#include "mi/Timer.h"
#pragma warning(disable: 4996)


using namespace mi;
using namespace stclient;


const char* stclient::to_s(int x)
{
	static char to_s_buf[1000];
	_ltoa(x, to_s_buf, 10);
	return to_s_buf;
}



static inline bool commandIs(const string& cmd,
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

enum Invalid
{
	INVALID_FORMAT,
	INVALID_STATUS,
};

typedef const std::vector<VariantType> Args;


class Command
{
public:
	Command(StClient* cl, mi::UdpSender& send) :
		client(cl),
		udp_send(&send)
	{
	}

	bool command(const string& line);
	void sendStatus();

private:
	// 起動
	void ping(Args& arg);

	// モード切り替え
	void black(Args& arg);
	void pict(Args& arg);
	void ident(Args& arg);

	// ゲーム情報
	void partner(Args& arg);
	void background(Args& arg);
	void playerStyle(Args& arg);
	void playerColor(Args& arg);

	// ゲーム
	void start(Args& arg);
	void stop(Args& arg);
	void frame(Args& arg);
	void replay(Args& arg);
	void save(Args& arg);
	void init(Args& arg);
	void hit(Args& arg);

	// 随時コマンド
	void mirror(Args& arg);
	void diskInfo(Args& arg);
	void reloadConfig(Args& arg);
	void bye(Args& arg);
	void status(Args& arg);
	void colorOverlay(Args& arg);

private:
	StClient* client;
	mi::UdpSender* udp_send;
};



static inline void arg_check(Args& arg, size_t x)
{
	if (arg.size()!=x)
		throw INVALID_FORMAT;
}

static inline void arg_check(Args& arg, size_t min, size_t max)
{
	if (arg.size()<min || arg.size()>max)
		throw INVALID_FORMAT;
}

void Command::ping(Args& arg)
{
	arg_check(arg, 1);

	printf("PING received: server is '%s'\n", arg[0].to_s());

	string s;
	s += "PONG ";
	s += Core::getComputerName();
	s += " ";
	s += mi::Udp::getIpAddress();
	s += " ";
	s += to_s(config.client_number);

	udp_send->init(arg[0].to_s(), UDP_CLIENT_TO_CONTROLLER);
	udp_send->send(s);
	global.setStatus(STATUS_IDLE);
}

void Command::mirror(Args& arg)
{
	arg_check(arg, 0);
	toggle(mode.mirroring);
}

// DISKINFO
void Command::diskInfo(Args& arg)
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
		return (int)(uint32)(size / 1000 / 1000);
	};

	const int free  = mega_bytes(free_bytes);
	const int total = mega_bytes(total_bytes);
	fprintf(stderr, "%u MB free(%.1f%%), %u MB total\n",
			free,
			free*100.0f/total,
			total);
	string s;
	s += "DISKSIZE ";
	s += to_s(free);
	s += " ";
	s += to_s(total);
	udp_send->send(s);
}

// RELOAD-CONFIG
void Command::reloadConfig(Args& arg)
{
	arg_check(arg, 0);
	load_config();
}

static const char* getStatusName(int present=-1)
{
	const auto st = (present==-1) ? global.clientStatus() : (ClientStatus)present;
	switch (st)
	{
	case STATUS_SLEEP:  return "SLEEP";
	case STATUS_IDLE:   return "IDLE";
	case STATUS_PICT:   return "PICT";
	case STATUS_BLACK:  return "BLACK";
	case STATUS_READY:  return "READY";
	case STATUS_GAME:   return "GAME";
	case STATUS_REPLAY: return "REPLAY";
	case STATUS_SAVING: return "SAVING";
	}
	return "UNKNOWN-STATUS";
}

// ステータスチェックをしない
static void no_check_status()
{
	// nop
}



// ClientStatusをチェックして規定値以外であれば弾く
static void status_check(ClientStatus st)
{
	if (global.clientStatus()!=st)
	{
		Console::pushColor(CON_RED); 
		fprintf(stderr, "INVALID STATUS: status is not <%s>, current status is <%s>\n",
			getStatusName(st),
			getStatusName());
		Console::popColor();
		throw INVALID_STATUS;
	}
}

void Command::sendStatus()
{
	string s;
	s += "STATUS ";
	s += Core::getComputerName();
	s += " ";
	s += getStatusName();
	udp_send->send(s);
}

// STATUS
void Command::status(Args& arg)
{
	arg_check(arg, 0);
	sendStatus();
}

// COLOR-OVERLAY <r> <g> <b> <a>
void Command::colorOverlay(Args& arg)
{
	arg_check(arg, 4);
	const int r = minmax(arg[0].to_i(), 0, 255);
	const int g = minmax(arg[1].to_i(), 0, 255);
	const int b = minmax(arg[2].to_i(), 0, 255);
	const int a = minmax(arg[3].to_i(), 0, 255);
	global.color_overlay.set(r, g, b, a);
}


// 0123456789 => 9/8/7/6/5/4/3/2/1/0/0123456789
static void SetFileNameFromGameId(string& dest, const char* s)
{
#if 1
	dest = s;
#else
	dest.clear();
	int length = strlen(s);
	for (int i=0; i<length; ++i)
	{
		dest += s[length-1-i];
		dest += '/';
	}
	dest += s;
#endif
}



// IDENT <player> <game>
void Command::ident(Args& arg)
{
	arg_check(arg, 2);
	status_check(STATUS_IDLE);

	const auto& player = arg[0];
	const auto& game   = arg[1];

	printf("PLAYER:%s, GAME:%s\n",
		player.to_s(),
		game.to_s());
	
	string filename;
	SetFileNameFromGameId(filename, game.to_s());
	if (!global.save_file.openForWrite(filename.c_str()))
	{
		puts("Open error (savefile)");
		return;
	}

	printf("Filename %s ok\n", filename.c_str());

	global.setStatus(STATUS_READY);
	sendStatus();
}

// PARTNER <game>
void Command::partner(Args& arg)
{
	arg_check(arg, 1);
	status_check(STATUS_READY);

	printf("PARTNER:%s\n", arg[0].to_s());
	
	string filename;
	SetFileNameFromGameId(filename, arg[0].to_s());
	if (!global.save_file.openForWrite(filename.c_str()))
	{
		puts("Open error (savefile)");
		return;
	}

	printf("Filename %s ok\n", filename.c_str());

	global.setStatus(STATUS_GAME);
	sendStatus();
}

// BACKGROUND <name>
void Command::background(Args& arg)
{
	arg_check(arg, 1);
	status_check(STATUS_READY);

	printf("BACKGROUND:%s\n", arg[0].to_s());
}

// PLAYER-STYLE <name>
void Command::playerStyle(Args& arg)
{
	arg_check(arg, 1);
	status_check(STATUS_READY);

	printf("PLAYER-STYLE:%s\n", arg[0].to_s());
}

// PLAYER-COLOR: <name>
void Command::playerColor(Args& arg)
{
	arg_check(arg, 1);
	status_check(STATUS_READY);

	printf("PLAYER-COLOR:%s\n", arg[0].to_s());
}

// START
void Command::start(Args& arg)
{
	arg_check(arg, 0);
	status_check(STATUS_READY);

	client->initHitObjects();
	client->startMovieRecordSettings();

	global.setStatus(STATUS_GAME);
	sendStatus();
}

// STOP
void Command::stop(Args& arg)
{
	arg_check(arg, 0);
	status_check(STATUS_GAME);

	global.setStatus(STATUS_READY);
	sendStatus();
}

// REPLAY
void Command::replay(Args& arg)
{
	arg_check(arg, 0);
	status_check(STATUS_READY);
	global.setStatus(STATUS_REPLAY);
}

// HIT <int>
void Command::hit(Args& arg)
{
	arg_check(arg, 1);
	status_check(STATUS_GAME);
	const int next_id = arg[0].to_i();
	Console::printf(CON_GREEN, "hit next_id is %d\n", next_id);
	global.hit_stage = next_id;
	global.on_hit_setup(next_id);
}

void Notice(const char* s)
{
	Console::puts(CON_CYAN, s);
}

void SystemMessage(const char* s)
{
	Console::puts(CON_GREEN, s);
}

void ErrorMessage(const char* s)
{
	Console::puts(CON_RED, s);
}

// FRAME <int:frame_num>
void Command::frame(Args& arg)
{
	arg_check(arg, 1);
	no_check_status();

	const int frame = arg[0].to_i();
	if (frame<0)
	{
		ErrorMessage("Invalid frame.");
	}
	else
	{
		printf("frame recvd: %d\n", frame);
		global.frame_index = frame;
	}
}

// SAVE
void Command::save(Args& arg)
{
	arg_check(arg, 0);
	status_check(STATUS_READY);

	SystemMessage("Saving...");
	global.setStatus(STATUS_SAVING);
	sendStatus();

	SystemMessage("Saved!");
	global.setStatus(STATUS_READY);
	sendStatus();
}

// INIT
void Command::init(Args& arg)
{
	arg_check(arg, 0);
	no_check_status();

	SystemMessage("Init!");
	global.setStatus(STATUS_IDLE);

	// ゲーム情報の破棄
	global.gameinfo.init();
}

// BLACK
void Command::black(Args& arg)
{
	arg_check(arg, 0);
	status_check(STATUS_IDLE);
	global.setStatus(STATUS_BLACK);
}

// PICT <filename>
void Command::pict(Args& arg)
{
	arg_check(arg, 1);
	status_check(STATUS_IDLE);

	string path;
	path += "//STMX64/ST/Picture/";
	path += arg[0].to_s();
	if (global.pic.createFromImageA(path.c_str()))
	{
		global.setStatus(STATUS_PICT);
		return;
	}
	else
	{
		printf("picture load error. %s\n", arg[0].to_s());
	}
}

// BYE
// EXIT
// QUIT
void Command::bye(Args& arg)
{
	arg_check(arg, 0);
	exit(0);
}


bool Command::command(const string& line)
{
	string cmd;
	std::vector<VariantType> arg;
	Lib::splitString(line, cmd, arg);

	// Daemon command
	if (cmd[0]=='#')
	{
		Console::printf(CON_DARKCYAN, "[DAEMON COMMAND] '%s' -- ignore", cmd.c_str());
		return true;
	}


	Console::printf(CON_CYAN, "Command: %s ", cmd.c_str());
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
		// 起動
		COMMAND("PING",     ping);

		// モード切り替え
		COMMAND("PICT",     pict);
		COMMAND("BLACK",    black);
		COMMAND("IDENT",    ident);

		// ゲーム情報
		COMMAND("PARTNER",      partner);
		COMMAND("BACKGROUND",   background);
		COMMAND("PLAYER-STYLE", playerStyle);
		COMMAND("PLAYER-COLOR", playerColor);

		// ゲーム
		COMMAND("START",    start);
		COMMAND("STOP",     stop);
		COMMAND("FRAME",    frame);
		COMMAND("REPLAY",   replay);
		COMMAND("HIT",      hit);
		COMMAND("SAVE",     save);
		COMMAND("INIT",     init);

		// 随時
		COMMAND("COLOR-OVERLAY", colorOverlay);
		COMMAND("STATUS",  status);
		COMMAND("DISKINFO", diskInfo);
		COMMAND("MIRROR",   mirror);
		COMMAND("RELOAD-CONFIG", reloadConfig);
		COMMAND("BYE",      bye) ;
		COMMAND("QUIT",     bye);
		COMMAND("EXIT",     bye);
#undef COMMAND

		Console::pushColor(CON_GREEN); 
		printf("Invalid udp-command '%s'\n", cmd.c_str());
		Console::popColor();
	}
	catch (Invalid)
	{
		Console::pushColor(CON_YELLOW); 
		printf("Invalid '%s' argc=%d\n", cmd.c_str(), argc);
		Console::popColor();
	}

	return false;
}

bool StClient::doCommand()
{
	string rawstring;
	if (udp_recv.receive(rawstring)<=0)
	{
		return false;
	}

	std::vector<string> lines;
	Lib::splitStringToLines(rawstring, lines);

	for (size_t i=0; i<lines.size(); ++i)
	{
		Command cmd(this, udp_send);
		cmd.command(lines[i]);
	}
	return true;
}
