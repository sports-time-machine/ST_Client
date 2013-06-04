#pragma once
#include "miCore.h"

namespace mi{


class PCM
{
public:
	typedef const void PCMDATA;
	enum Format
	{
		Mono,
		Stereo,
	};

public:
	PCM(const char* path)           { load(path); }
	void load(const char* path);
	void unload();

	int         size() const { return static_cast<int>(_rawpcm.size()); }
	PCMDATA*  rawpcm() const { return &_rawpcm[0]; }
	int         freq() const { return _freq; }
	bool   is_stereo() const { return _stereo; }
	bool    is_16bit() const { return _16bits; }

private:
	void _Load_RIFF_WAVE(File& f);

protected:
	std::vector<char>   _rawpcm;
	int    _freq;
	bool   _16bits,_stereo;
};


class AudioBuffer
{
public:
	AudioBuffer();
	~AudioBuffer();

	void free();
	void attach(const PCM& pcm);
	void operator<<(const PCM& pcm) { attach(pcm); }

	bool is_loaded() const { return _buffer!=0; }
	IdNumber getid() const { return _buffer; }

private:
	IdNumber _buffer;
};


class AudioSource
{
	friend class Audio;
	friend class AudioPlayer;

public:
	// Create & Destroy
	AudioSource();
	virtual ~AudioSource();
	void create();
	void destroy();
	void attach(const AudioBuffer& buffer, float gain, int user_id=0);
	void operator<<(const AudioBuffer& buffer)         { attach(buffer, 1.0f); }

	// Play Commands
	void play();
	void play_infinite();
	void stop();
	void rewind();
	void pause();
	void resume();

	// Play Attributes
	void jump(float sec);
	void jump(int min, int sec, int msec=0);
	void volume(float gain);
	void pitch(float pitch);
	void pan(float x);
	void pos(float x, float y, float z);

	// Properties
	int   offset_samp() const;
	float  offset_sec() const;
	float current_pos() const;
	float       pitch() const;
	float      volume() const;
	bool   is_playing() const;
	bool     is_ready() const;
	int       user_id() const   { return _user_id; }

private:
	void _PlaySound(bool);

private:
	char guard1[1000];
	IdNumber _source;
	bool     _is_bgm;
	int      _buffer_size;
	int      _user_id;
	uint32   _begin_tick;
	float    _gain,_pitch;

	enum _Status
	{
		_Ready,
		_Play,
		_Pause,
	};
	_Status _status;
	char guard2[1000];
};


class AudioPlayer
{
	friend class Audio;

public:
	virtual ~AudioPlayer();

	void       play(const AudioBuffer& buffer, int user_id=0, bool oneshot=false);
	void   play_one(const AudioBuffer& buffer);
	void operator()(const AudioBuffer& buffer, int user_id=0)    { play(buffer,user_id); }

	void mute();

	int  playing_chennels() const;
	bool is_playing() const             { return playing_chennels()>0; }
	int  channels() const               { return static_cast<int>(_source.size()); }
	AudioSource& operator[](int index)  { return _source[index % _source.size()]; }

private:
	void create(bool is_bgm, int channels);

private:
	bool _is_bgm;
	std::vector<AudioSource> _source;
};


class Audio
{
	friend class AudioPlayer;

public:
	static const uint
		DEFAULT_CHANNELS = 8,
		MAX_CHANNELS = 32;

public:
	// Singleton
	static Audio& self() { static Audio au; return au; }

private:
	Audio();
	virtual ~Audio();

public:
	// Open & Close
	void init();
	void createChannels(int bgm, int se);
	void destroy();

	// Audio Player Object
	AudioPlayer bgm,se;

	// Global Pause/Resume
	void pauseAll();
	void resumeAll();

	// Global Commands
	void  setMasterMusicVolume(float gain);
	void  setMasterSoundVolume(float gain);
	void  setMasterVolume(float gain);
	void  setMasterPitch(float pitch);

	// Global Property
	float getMasterVolume()      const  { return _master_volume;       }
	float getMasterMusicVolume() const  { return _master_music_volume; }
	float getMasterSoundVolume() const  { return _master_sound_volume; }
	float getMasterPitch()       const  { return _master_pitch;        }
	int   getPlayingChennels()   const  { return bgm.playing_chennels() + se.playing_chennels(); }
	bool  isPlaying()            const  { return bgm.is_playing() || se.is_playing(); }

private:
	void* _alc_device;
	void* _alc_context;
	float _master_volume,_master_music_volume,_master_sound_volume,_master_pitch;
};


}//namespace mi
