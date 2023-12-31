// スポーツタイムマシンでのControllerとのUDP通信

#include "StClient.h"
#include "mi/Image.h"
#include "mi/Udp.h"
#include "mi/Libs.h"
#include "mi/Timer.h"
#pragma warning(disable: 4996)


using namespace mi;
using namespace stclient;


// プレイヤー色名からRGBAを得る
// 色はconfig.pslのcolorsセクションで指定しておくこと
static glRGBA getPlayerColorFromString(const std::string& name)
{
	Msg::Notice("getPlayerColorFromString", name);
	const auto itr = config.player_colors.find(name);
	if (itr==config.player_colors.end())
	{
		Msg::ErrorMessage("Color not found!", name);
		// 見つからなかったときのデフォルト色
		return config.color.default_player_color;
	}

	glRGBA col = itr->second;
	printf("{r:%d, g:%d, b:%d, a:%d}\n", col.r, col.g, col.b, col.a);
	return col;
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
	void idle(Args& arg);
	void black(Args& arg);
	void pict(Args& arg);
	void ident(Args& arg);
	void beginInitFloor(Args& arg);
	void endInitFloor(Args& arg);

	// ゲーム情報
	void partner(Args& arg);
	void background(Args& arg);
	void playerStyle(Args& arg);
	void playerColor(Args& arg);
	void idleSelect(Args& arg);

	// ゲーム
	void start(Args& arg);
	void stop(Args& arg);
	void frame(Args& arg);
	void replay(Args& arg);
	void save(Args& arg);
	void init(Args& arg);
	void gameStart(Args& arg);
	void goal(Args& arg);
	void hit(Args& arg);

	// 随時コマンド
	void mirror(Args& arg);
	void diskInfo(Args& arg);
	void reloadConfig(Args& arg);
	void bye(Args& arg);
	void status(Args& arg);
	void colorOverlay(Args& arg);

	void status_check(ClientStatus st);

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

// PING <server-addr> <server-port>
void Command::ping(Args& arg)
{
	arg_check(arg, 1);

	printf("PING received: server is '%s'\n", arg[0].to_s());

	{
		string s;
		s += "PONG ";
		s += Core::getComputerName();
		s += " ";
		s += mi::Udp::getIpAddress();
		s += " ";
		s += Lib::to_s(config.client_number);
		udp_send->init(arg[0].to_s(), UDP_CLIENT_TO_CONTROLLER);
		udp_send->send(s);
		Msg::Notice(s);
	}

	client->changeStatus(STATUS_IDLE);
}

void Command::mirror(Args& arg)
{
	arg_check(arg, 0);
	Lib::toggle(global.mirroring);
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
	s += Lib::to_s(free);
	s += " ";
	s += Lib::to_s(total);
	udp_send->send(s);
}

// RELOAD-CONFIG
void Command::reloadConfig(Args& arg)
{
	arg_check(arg, 0);
	load_config();
	client->reloadResources();
}

// ステータスチェックをしない
static void no_check_status()
{
	// nop
}



// ClientStatusをチェックして規定値以外であれば弾く
void Command::status_check(ClientStatus st)
{
	if (client->clientStatus()!=st)
	{
		Console::pushColor(CON_RED); 
		fprintf(stderr, "INVALID STATUS: status is not <%s>, current status is <%s>\n",
			client->getStatusName(st),
			client->getStatusName());
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
	s += client->getStatusName();
	udp_send->send(s);
}

// STATUS
void Command::status(Args& arg)
{
	arg_check(arg, 0);
	sendStatus();
	Msg::Notice("Current status", client->getStatusName());
}

// FRAME <int:frame_num>
void Command::frame(Args& arg)
{
	arg_check(arg, 1);
	no_check_status();

	const int frame = arg[0].to_i();
	if (frame<0)
	{
		Msg::ErrorMessage("Invalid frame.");
	}
	else
	{
		global.frame_index = frame;
	}
}

// IDLE-SELECT <int:frame_num>
void Command::idleSelect(Args& arg)
{
	arg_check(arg, 1);
	no_check_status();

	const int value = arg[0].to_i();
	global.idle_select = value;
}

// BEGIN-INIT-FLOOR
void Command::beginInitFloor(Args& arg)
{
	arg_check(arg, 0);
	no_check_status();
	
	Msg::SystemMessage("Begin init floor");
	client->clearFloorDepth();
	client->changeStatus(STATUS_INIT_FLOOR);
}

// END-INIT-FLOOR
void Command::endInitFloor(Args& arg)
{
	arg_check(arg, 0);
	status_check(STATUS_INIT_FLOOR);
	
	Msg::SystemMessage("End of init floor");
	client->changeStatus(STATUS_IDLE);
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


// 'P'+8桁の場合
// 1             00000001
// ABC           00000ABC
// HELLO         000HELLO
// HELLO250      HELLO250
// POINT500      POINT500
// POINT9999     OINT9999
// P00000001     00000001
// PABCDEF12     ABCDEF12
// MACINTOSH     error
// PACIFIC500    error
static string normalize(char prefix, const string& untaint, int length)
{
	string raw;
	for (size_t i=0; i<untaint.length(); ++i)
	{
		// [^A-Z0-9]ならエラー
		if (!isalnum(untaint[i]))
		{
			return "";
		}
		raw += toupper(untaint[i]);
	}

	// そもそもIDがlength+1を超えているとエラー
	if (raw.length() > length+1u)
	{
		return "";
	}

	// length+1
	if (raw.length()==length+1u)
	{
		if (raw[0]==prefix)
		{
			// 正規のIDだ!!!
			return raw.substr(1);
		}
		else
		{
			// POINT9999(P:OINT9999)などだ!! -- error
			return "";
		}
	}

	// zero padding
	const size_t ZERO = length-raw.size();
	string id;
	for (size_t i=0; i<ZERO; ++i)
	{
		id += '0';
	}
	id += raw;
	return id;
}



// IDENT <player> <game>
// IDENT命令の前にさまざまなパラメーターがおくられている予定
void Command::ident(Args& arg)
{
	arg_check(arg, 2);
	no_check_status();

	// IDの正規化
	string player_id = normalize('P', arg[0].to_s(),  8);
	string   game_id = normalize('G', arg[1].to_s(), 10);
	
	if (player_id=="")
	{
		Msg::ErrorMessage("Invalid player id", player_id);
		return;
	}
	if (game_id=="")
	{
		Msg::ErrorMessage("Invalid game id", game_id);
		return;
	}

	printf("PLAYER:%s, GAME:%s\n",
		player_id.c_str(),
		game_id.c_str());

	if (!global.gameinfo.prepareForSave(player_id, game_id))
	{
		Msg::ErrorMessage("Open error (ident, prepareForSave)");
		return;
	}

	client->changeStatus(STATUS_READY);
}

// PARTNER <game>
// PARTNER M:CHEETAH-1
void Command::partner(Args& arg)
{
	arg_check(arg, 1);
	status_check(STATUS_IDLE);

	const string id = arg[0].to_s();
	if (id.length()>=2 && id[0]=='M' && id[1]==':')
	{
		// MovingObject!!
		const string name = id.substr(2);
		auto itr = global.moi_lib.find(name);
		if (itr==global.moi_lib.end())
		{
			Msg::ErrorMessage("MovingObject not found", name);
		}
		else
		{
			global.game_start_frame = 0;
			global.in_game_or_replay = false;
			global.partner_mo.init(global.moi_lib[name]);
		}
	}
	else
	{
		// ふつうのパートナー
		const string partner_game = normalize('G', id, 10);
		printf("PARTNER:%s\n", partner_game.c_str());

		client->changeStatus(STATUS_LOADING);

		Msg::Notice("Loading partner movie", partner_game);
		mi::File f;
		if (!global.gameinfo.partner1.load(GameInfo::GetStandardFilePath(partner_game, config.client_number)))
		{
			Msg::ErrorMessage("Cannot open partner movie", partner_game);
		}
		else
		{
			printf("Partner 1: %d frames\n", global.gameinfo.partner1.total_frames);
		}
	}

	client->changeStatus(STATUS_IDLE);
}

// BACKGROUND <name>
void Command::background(Args& arg)
{
	arg_check(arg, 1);
	no_check_status();
	//##status_check(STATUS_IDLE);

	const string env = arg[0].to_s();
	printf("BACKGROUND:%s\n", env.c_str());

	const auto itr = config.run_env.find(env);
	if (itr!=config.run_env.cend())
	{
		Msg::Notice("RunEnv", env);
		global.run_env = &itr->second;
		Msg::Notice("RunEnv", global.run_env->background.fullpath);
	}
	else
	{
		Msg::ErrorMessage("RunEnv not found", env);
		global.run_env = Config::getDefaultRunEnv();
	}
}

// PLAYER-STYLE <name>
void Command::playerStyle(Args& arg)
{
	arg_check(arg, 1);
	no_check_status();
//	status_check(STATUS_IDLE);

	printf("PLAYER-STYLE:%s\n", arg[0].to_s());
}

// PLAYER-COLOR: <name>
void Command::playerColor(Args& arg)
{
	arg_check(arg, 1);
	no_check_status();
//	status_check(STATUS_IDLE);

	printf("PLAYER-COLOR:%s\n", arg[0].to_s());

	global.gameinfo.movie.player_color = arg[0].to_s();
	global.gameinfo.movie.player_color_rgba = getPlayerColorFromString(arg[0].to_s());
}

// INIT
void Command::init(Args& arg)
{
	arg_check(arg, 0);
	no_check_status();

	// ゲーム情報の破棄
	client->initGameInfo();

	// 走行環境の破棄（デフォルトにする）
	global.run_env = Config::getDefaultRunEnv();

	// 並走者の削除
	global.gameinfo.movie   .clearAll(config.color.default_player_color);
	global.gameinfo.partner1.clearAll(config.color.default_player_color);
	global.gameinfo.partner2.clearAll(config.color.default_player_color);
	global.gameinfo.partner3.clearAll(config.color.default_player_color);

	Msg::SystemMessage("Init!");
	client->changeStatus(STATUS_IDLE);

	// MovingObject関係
	global.game_start_frame = 0;
	global.in_game_or_replay = false;
	global.partner_mo.init();
}

// START -- 録画開始（ゲーム開始ではない）
void Command::start(Args& arg)
{
	arg_check(arg, 0);
	status_check(STATUS_READY);

	// ゲーム情報の破棄
	client->initGameInfo();

	client->changeStatus(STATUS_GAME);

	// MovingObject
	global.game_start_frame = 0;
	global.in_game_or_replay = false;
}

// STOP
void Command::stop(Args& arg)
{
	arg_check(arg, 0);
	status_check(STATUS_GAME);
	client->changeStatus(STATUS_READY);

	// MovingObject関係
	global.in_game_or_replay = false;
}

// REPLAY
void Command::replay(Args& arg)
{
	arg_check(arg, 0);
//	status_check(STATUS_READY);
	client->changeStatus(STATUS_REPLAY);

	// MovingObject関係
	global.game_start_frame = 0;
	global.in_game_or_replay = false;
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

// SAVE
void Command::save(Args& arg)
{
	arg_check(arg, 0);
	status_check(STATUS_READY);

	Msg::SystemMessage("Saving...");
	client->changeStatus(STATUS_SAVING);

	// SAVING状態を描画しておく
	client->drawOneFrame();

	// prepareForSave()はIDENT時にやっています
	global.gameinfo.save();

	Msg::SystemMessage("Saved!");
	client->changeStatus(STATUS_IDLE);
}

// GAME-START
void Command::gameStart(Args& arg)
{
	arg_check(arg, 0);
	no_check_status();

	// 「よーい、ドン！」の「ドン！」の最初のフレームにshoutされる
	// 通常プレイでも、リプレイでも、同じように使われる
	// in_game_or_replayをtrueにする唯一のトリガー
	global.game_start_frame = global.frame_index;
	global.in_game_or_replay = true;
	global.partner_mo.initRunParam();
}

// GOAL
void Command::goal(Args& arg)
{
	arg_check(arg, 0);
	no_check_status();

	// ゴールしたフレームでshoutされる
	global.in_game_or_replay = false;
}

// IDLE
void Command::idle(Args& arg)
{
	arg_check(arg, 0);
	no_check_status();
	client->changeStatus(STATUS_IDLE);
}

// BLACK
void Command::black(Args& arg)
{
	arg_check(arg, 0);
	no_check_status();
	client->changeStatus(STATUS_BLACK);
}

// PICT <filename>
void Command::pict(Args& arg)
{
	arg_check(arg, 1);
	no_check_status();
	
	const int pic_no = arg[0].to_i();
	
	if (pic_no<1 || pic_no>MAX_PICT_NUMBER)
	{
		printf("Invalid picuture number %s\n", arg[0].to_i());
	}
	else
	{
		printf("Picuture number %d\n", pic_no);
		global.picture_number = pic_no;
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
	if (cmd.substr(0,1)=="#")
	{
		Console::printf(CON_DARKCYAN, "[DAEMON COMMAND] '%s' -- ignore\n", cmd.c_str());
		return true;
	}

	// UI command
	if (cmd.substr(0,3)=="UI_")
	{
//#		Console::printf(CON_DARKCYAN, "[UI COMMAND] '%s' -- ignore\n", cmd.c_str());
		return true;
	}


	if (cmd!="FRAME")
	{
		Console::printf(CON_CYAN, "\nCommand: %s ", cmd.c_str());
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
	}

	const int argc = arg.size();

	try
	{
		// @command
#define COMMAND(CMD, PROC)    if (cmd.compare(CMD)==0) { PROC(arg); return true; }
		// 起動
		COMMAND("PING",              ping);

		// モード切り替え
		COMMAND("PICT",              pict);
		COMMAND("IDLE",              idle);
		COMMAND("BLACK",             black);
		COMMAND("IDENT",             ident);
		COMMAND("BEGIN-INIT-FLOOR",  beginInitFloor);
		COMMAND("END-INIT-FLOOR",    endInitFloor);

		// アイドル用
		COMMAND("IDLE-SELECT", idleSelect);

		// ゲーム情報
		COMMAND("PARTNER",           partner);
		COMMAND("BACKGROUND",        background);
		COMMAND("PLAYER-STYLE",      playerStyle);
		COMMAND("PLAYER-COLOR",      playerColor);

		// ゲーム
		COMMAND("START",      start);
		COMMAND("STOP",       stop);
		COMMAND("FRAME",      frame);
		COMMAND("REPLAY",     replay);
		COMMAND("HIT",        hit);
		COMMAND("SAVE",       save);
		COMMAND("INIT",       init);
		COMMAND("GAME-START", gameStart);

		// 随時
		COMMAND("COLOR-OVERLAY", colorOverlay);
		COMMAND("STATUS",        status);
		COMMAND("DISKINFO",      diskInfo);
		COMMAND("MIRROR",        mirror);
		COMMAND("RELOAD-CONFIG", reloadConfig);
		COMMAND("BYE",           bye) ;
		COMMAND("QUIT",          bye);
		COMMAND("EXIT",          bye);
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

void StClient::processUdpCommands()
{
	for (;;)
	{
		string rawstring;
		if (udp_recv.receive(rawstring)<=0)
		{
			break;
		}

		if (config.ignore_udp)
		{
			// ignore udp command
			continue;
		}
		
		// Process commands
		std::vector<string> lines;
		Lib::splitStringToLines(rawstring, lines);
		for (size_t i=0; i<lines.size(); ++i)
		{
			Command cmd(this, udp_send);
			cmd.command(lines[i]);
		}
	}
}
