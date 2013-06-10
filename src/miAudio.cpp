#include "miAudio.h"
#include <OpenAL/al.h>
#include <OpenAL/alc.h>

using namespace mi;

#ifdef _M_X64
#pragma comment(lib,"OpenAL32_x64.lib")
#else
#pragma comment(lib,"OpenAL32_x32.lib")
#endif


#if 1
#define DEBUG_PRINT(x) fprintf(stderr, "%s (this=%p)\n", x, this);
#else
#define DEBUG_PRINT(x) /*nop*/
#endif



static Audio& audio = Audio::self();

static inline float float01(float f)
{
	return (f>1.0f) ? 1.0f : (f<0.0f) ? 0.0f : f;
}

static inline void float01crop(float& f)
{
	if (f>1.0f) f=1.0f;
	if (f<0.0f) f=0.0f;
}






void PCM::_Load_RIFF_WAVE(File& f)
{
	if (!f.readStringAndCompare("RIFF"))
	{
		throw "WavHeaderException";
	}

	// filesize-8
	f.get32();

	if (!f.readStringAndCompare("WAVE"))
	{
		throw "WavHeaderException";
	}

	if (!f.readStringAndCompare("fmt "))
	{
		throw "WavFormatException";
	}


	uint32 fmt_size  = f.get32();
	uint16 format    = f.get16();
	uint16 channels  = f.get16();
	uint32 freq      = f.get32();
	uint32 byteRate  = f.get32();
	uint16 blockSize = f.get16();
	uint16 bitsPerSample = f.get16();
	(void)fmt_size;
	(void)byteRate;
	(void)blockSize;
	(void)bitsPerSample;

	enum { WAVE_AUDIO_FORMAT_PCM=1 };

	if (format!=WAVE_AUDIO_FORMAT_PCM)   throw "WavFormatException";
	if (!(channels==1 || channels==2))   throw "WavFormatException";
	if (!(freq>=11050 && freq<=48000))   throw "WavFormatException";

	// "fmt "'s padding
	if (fmt_size%4)
	{
		f.readAbandon((int)(4 - fmt_size%4));
	}

	// Skip to "data" chunk
	for (;;)
	{
		if (f.readStringAndCompare("data"))
			break;
		if (f.eof())
			throw "WavFormatException()";
		
//#		con << ::Format("Skip chunk:{0}",s) << newline;
		size_t data_chunk_size = f.get32();
		if (data_chunk_size%4)
		{
//#			data_chunk_size += 4 - data_chunk_size%4;
		}
		f.readAbandon(data_chunk_size);
	}


	// "data" chunk
	size_t data_chunk_size = f.get32();
	_rawpcm.resize(data_chunk_size);
	f.read(_rawpcm.data(), data_chunk_size);

	_16bits = true;
	_stereo = (channels>1);
	_freq   = static_cast<int>(freq);
}




void PCM::unload()
{
	_rawpcm.clear();
	_freq   = 0;
	_16bits = false;
	_stereo = false;
}


void PCM::load(const char* path)
{
	unload();

	File f(path);

	std::string header;
	f.readStringA(header,4);
	f.rewind();
	
	if (header.compare("RIFF")==0)
	{
		_Load_RIFF_WAVE(f);
	}
	else
	{
		throw "PcmInvalidFormatException()";
	}
}



//=================
// RGX.AudioBuffer
//=================
AudioBuffer::AudioBuffer()
{
	_buffer = 0;
}

AudioBuffer::~AudioBuffer()
{
	free();
}

void AudioBuffer::free()
{
	if (_buffer)
	{
		puts("audiobuffer, free");
#if 0
		alDeleteBuffers(1, &_buffer);
#endif
		_buffer = 0;
	}
}

void AudioBuffer::attach(const PCM& pcm)
{
	free();
	alGenBuffers(1, &_buffer);
	printf("AudioBuffer: %X\n", _buffer);
	alBufferData(_buffer,
		pcm.is_stereo() ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16,
		pcm.rawpcm(),
		static_cast<ALsizei>(pcm.size()),
		static_cast<ALsizei>(pcm.freq()));
}



//===================================
// RGX.AudioSource: Create & Destroy
//===================================
AudioSource::AudioSource()
{
	_status = _Ready;
	_source = 0;
	_gain   = 1.0f;
	_pitch  = 1.0f;
	_is_bgm = false;
	_buffer_size = 0;
	_user_id = 0;
	_begin_tick = 0;
}

AudioSource::~AudioSource()
{
	DEBUG_PRINT("~AudioSource");
	destroy();
}

void AudioSource::destroy()
{
	if (_source)
	{
		DEBUG_PRINT("AudioSource::destroy");
		alDeleteSources(1, &_source);
		_source = 0;
	}
}

void AudioSource::create()
{
	destroy();
	_source = 0;
	alGenSources(1, &_source);
	printf("Audio Source: %X\n", _source);
}

void AudioSource::attach(const AudioBuffer& buffer, float gain, int user_id)
{
	_user_id = user_id;
	_status = _Ready;
	alSourcei(_source, AL_BUFFER, (ALint)buffer.getid());
	alSourcei(_source, AL_LOOPING, AL_FALSE);

	ALint siz = 0;
	alGetBufferi(buffer.getid(), AL_SIZE, &siz);
	_buffer_size = siz;

	// Default Gain
	volume(gain);

	// Default Pitch
	pitch(1.0f);
}


//================================
// RGX.AudioSource: Play Commands
//================================
void AudioSource::_PlaySound(bool infinite)
{
	if (!is_ready()) return;

	_is_bgm = infinite;
	switch (_status)
	{
	case _Play:
	case _Ready:
		_status = _Play;
		volume(_gain);
		alSourcei(_source, AL_LOOPING, infinite);
		alSourcePlay(_source);
		_begin_tick = Core::getSystemTick();
		break;
	case _Pause:
		alSourcePause(_source);
		break;
	}
}

void AudioSource::play()
{
	_PlaySound(false);
}

void AudioSource::play_infinite()
{
	_PlaySound(true);
}

void AudioSource::stop()
{
	if (_source==0) return;
	alSourceStop(_source);
}

void AudioSource::rewind()
{
	if (_source==0) return;
	alSourceRewind(_source);
}

void AudioSource::pause()
{
	if (_status==_Play)
	{
		alSourcePause(_source);
		_status = _Pause;
	}
}

void AudioSource::resume()
{
	int state = 0;
	alGetSourcei(_source, AL_SOURCE_STATE, &state);
	if (state==AL_PAUSED)
	{
		alSourcePlay(_source);
		_status = _Play;
	}
}



//==================================
// RGX.AudioSource: Play Attributes
//==================================
bool AudioSource::is_playing() const
{
	if (!is_ready()) return false;
	ALint state = 0;
	alGetSourcei(_source, AL_SOURCE_STATE, &state);
	return state==AL_PLAYING;
}

bool AudioSource::is_ready() const
{
	return _source!=0;
}

int AudioSource::offset_samp() const
{
	if (!is_playing()) return 0;
	int offset=0;
	alGetSourcei(_source, AL_SAMPLE_OFFSET, &offset);
	return offset;
}

float AudioSource::offset_sec() const
{
	if (!is_playing()) return 0.0f;
	float offset=0;
	alGetSourcef(_source, AL_SEC_OFFSET, &offset);
	return offset;
}

float AudioSource::current_pos() const
{
	if (!is_playing()) return 0.0f;
	ALint offs = 0;
	alGetSourcei(_source, AL_BYTE_OFFSET, &offs);
	return float01(1.0f * offs / _buffer_size);
}

void AudioSource::jump(float sec)
{
	alSourcef(_source, AL_SEC_OFFSET, sec);
}

void AudioSource::jump(int min, int sec, int msec)
{
	jump(min*60 + sec + msec/1000.0f);
}

float AudioSource::volume() const
{
	if (!is_ready()) return 0;
	return _gain;
}

void AudioSource::volume(float gain)
{
	float01crop(gain);
	_gain = gain;
	const float master_gain =
		(_is_bgm ? audio.getMasterMusicVolume() : audio.getMasterSoundVolume()) * audio.getMasterVolume();
	alSourcef(_source, AL_GAIN, gain * master_gain);
}

float AudioSource::pitch() const
{
	if (!is_ready()) return 0;
	return _pitch;
}

void AudioSource::pitch(float pitch)
{
	float01(pitch);
	_pitch = pitch;
	alSourcef(_source, AL_PITCH, pitch * audio.getMasterPitch());
}

void AudioSource::pan(float x)
{
	alSource3f(_source, AL_POSITION, x,0,0);
}

void AudioSource::pos(float x, float y, float z)
{
	alSource3f(_source, AL_POSITION, x,y,z);
}




//=================
// RGX.Audio
//=================
#define aldevice    ((ALCdevice*)_alc_device)
#define alcontext   ((ALCcontext*)_alc_context)

Audio::Audio()
{
	_alc_device  = nullptr;
	_alc_context = nullptr;
	_master_volume       = 1.0f;
	_master_music_volume = 1.0f;
	_master_sound_volume = 1.0f;
	_master_pitch        = 1.0f;
}

Audio::~Audio()
{
	DEBUG_PRINT("~Audio");
	destroy();
}

void Audio::destroy()
{
	DEBUG_PRINT("Audio::destroy");
	if (_alc_context)
	{
		puts("audio, free");
		alcMakeContextCurrent(nullptr);
		fprintf(stderr, "alcontext=%p\n", alcontext);
		alcDestroyContext(alcontext);
		_alc_context = nullptr;
	}
	if (_alc_device)
	{
		alcCloseDevice(aldevice);
		_alc_device = nullptr;
	}
}

void Audio::init()
{
	destroy();

	// Open Device
	_alc_device = alcOpenDevice(nullptr);
	if (_alc_device==nullptr)
	{
		throw "AudioOpenDeviceException()";
	}

	_alc_context = alcCreateContext(aldevice, nullptr);
	if (_alc_context==nullptr)
	{
		throw "AudioContextException()";
	}

	if (alcMakeContextCurrent(alcontext)==ALC_FALSE)
	{
		throw "AudioContextException()";
	}

	fprintf(stderr, "(init) alcontext=%p\n", alcontext);



	// Default listener
	static const ALfloat zero[3] = {};
	static const ALfloat ori[]={
		0.0f, 0.0f, -1.0f,
		0.0f, 1.0f,  0.0f};
	alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
	alListenerfv(AL_POSITION, zero);
	alListenerfv(AL_VELOCITY, zero);
	alListenerfv(AL_ORIENTATION, ori);
}

void Audio::createChannels(int bgm_channels, int se_channels)
{
	bgm.create(true, bgm_channels);
	se .create(false, se_channels);
}

void Audio::resumeAll()
{
	for (int i=0; i<bgm.channels(); ++i)
	{
		bgm[i].resume();
	}
	for (int i=0; i<se.channels(); ++i)
	{
		se[i].resume();
	}
}

void Audio::pauseAll()
{
	for (int i=0; i<bgm.channels(); ++i)
	{
		bgm[i].pause();
	}
	for (int i=0; i<se.channels(); ++i)
	{
		se[i].pause();
	}
}

#define foreach(I,DATA) for(auto I=(DATA).begin(); I!=(DATA).end(); ++I)

void Audio::setMasterMusicVolume(float gain)
{
	float01(gain);
	_master_music_volume = gain;
	
	foreach (src, bgm._source)
	{
		src->volume(src->volume());
	}
}

void Audio::setMasterSoundVolume(float gain)
{
	float01(gain);
	_master_sound_volume = gain;
	
	foreach (src, se._source)
	{
		src->volume(src->volume());
	}
}

void Audio::setMasterVolume(float gain)
{
	float01(gain);
	_master_volume = gain;

	setMasterMusicVolume(getMasterMusicVolume());
	setMasterSoundVolume(getMasterSoundVolume());
}

void Audio::setMasterPitch(float pitch)
{
	_master_pitch = pitch;

	foreach (src, se._source)
	{
		src->pitch(src->pitch());
	}
}






//*********************
// AUDIO PLAYER OBJECT
//*********************
AudioPlayer::~AudioPlayer()
{
	DEBUG_PRINT("~AudioPlayer");
}

void AudioPlayer::create(bool is_bgm, int channels)
{
	_is_bgm = is_bgm;
	_source.resize(channels);
	foreach (ch, _source)
	{
		ch->create();
	}
}

void AudioPlayer::play_one(const AudioBuffer& buffer)
{
	play(buffer, 0, true);
}

void AudioPlayer::play(const AudioBuffer& buffer, int user_id, bool oneshot)
{
	if (_is_bgm)
	{
		// [BGM] uses channel 0 always
		auto& ch = _source[user_id];
		ch.stop();
		ch.attach(buffer, 1.0f);
		ch._user_id = 0;
		oneshot
			? ch.play()
			: ch.play_infinite();
	}
	else
	{
		// [SE] plays looking for a free channel.
		AudioSource* play = nullptr;
		uint32 oldest_tick = 0;
		foreach (ch, _source)
		{
			if (oldest_tick==0 || oldest_tick>ch->_begin_tick)
			{
				oldest_tick = ch->_begin_tick;
				play        = &*ch;
			}			
			
			if (!ch->is_playing())
			{
				play = &*ch;
				break;
			}
		}
		
		// [Otherwise] Overwrite oldest played channel
		play->stop();
		play->attach(buffer, 1.0f);
		play->_user_id = user_id;
		play->play();
	}
}

int AudioPlayer::playing_chennels() const
{
	int count = 0;
	foreach (ch, _source)
	{
		if (ch->is_playing())
		{
			++count;
		}
	}
	return count;
}

void AudioPlayer::mute()
{
	foreach (ch, _source)
	{
		ch->stop();
	}
}
