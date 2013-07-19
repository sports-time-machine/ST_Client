#include "mi/mi.h"
#include "StClient.h"

using namespace mgl;
using namespace mi;
using namespace stclient;


// ゲーム情報の破棄、初期化
void GameInfo::init()
{
	movie   .clearAll(config.color.default_player_color);
	partner1.clearAll(config.color.default_player_color);
	partner2.clearAll(config.color.default_player_color);
	partner3.clearAll(config.color.default_player_color);
	movie.frames.clear();
	movie.cam1 = CamParam();
	movie.cam2 = CamParam();
}

string GameInfo::GetFolderName(const string& id)
{
	string folder = string("//")+config.server_name+"/ST/Movie/";

	// 逆順で追加
	// 0000012345 => '5/4/3/2/1/0/0/0/0/0/'
	const size_t LEN = id.size();	
	for (size_t i=0; i<LEN; ++i)
	{
		folder += id[LEN-1-i];
		folder += '/';
	}

	return folder;
}

string GameInfo::GetMovieFileName(const string& id)
{
	return GetFolderName(id) + id + ".stmov";
}

string GameInfo::GetStandardFilePath(const string& id, int subnumber)
{
	return GetFolderName(id) + id + "-" + Lib::to_s(subnumber) + ".stmov";
}

// サムネ保存
//  - ファイル名: "0000012345-1.jpg"
void GameInfo::save_Thumbnail(const string& basename, const string& suffix, int)
{
	string path = basename + "-" + suffix + ".jpg";

	File f;
	if (!f.openForWrite(path.c_str()))
	{
		Console::printf(CON_RED, "Cannot open file '%s' (save_Thumbnail)\n", path.c_str());
		return;
	}
}

bool GameInfo::prepareForSave(const string& player_id, const string& game_id)
{
	this->movie.game_id   = game_id;
	this->movie.player_id = player_id;

	Msg::SystemMessage("Prepare for save!");
	printf("Game-ID: %s\n", game_id.c_str());

	// Folder name: ${BaseFolder}/E/D/C/B/A/0/0/0/0/0/
	const string folder = GetFolderName(game_id);
	printf("Folder: %s\n", folder.c_str());
	mi::Folder::createFolder(folder.c_str());

	// Base name: ${BaseFolder}/E/D/C/B/A/0/0/0/0/0/00000ABCDE
	this->basename = folder + game_id;

	// Open: 00000ABCDE-1.stmov
	const string filename = this->basename + "-" + Lib::to_s(config.client_number) + ".stmov";
	printf("Movie: %s\n", filename.c_str());
	if (!movie_file.openForWrite(filename))
	{
		Msg::ErrorMessage("Cannot open file (preapreForSave)", filename);
		return false;
	}

	return true;
}

void GameInfo::save()
{
	Msg::SystemMessage("Save to file!");

	// Save Movie
	global.gameinfo.movie.saveToFile(movie_file);
	movie_file.close();
	Msg::Notice("Saved!");

	// Save Thumbnail
#if 0
	save_Thumbnail(basename,"1",0);
	save_Thumbnail(basename,"2",0);
	save_Thumbnail(basename,"3",0);
	save_Thumbnail(basename,"4",0);
	save_Thumbnail(basename,"5",0);
	save_Thumbnail(basename,"6",0);
#endif
}
