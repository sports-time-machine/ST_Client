// Undeprecate CRT functions
#ifndef _CRT_SECURE_NO_DEPRECATE 
	#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#define SHOW_RAW_NEAR_AND_FAR   0

#include "miCore.h"
#include "miImage.h"
#include "FreeType.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#ifdef _M_X64
#pragma comment(lib,"OpenNI2_x64.lib")
#else
#pragma comment(lib,"OpenNI2_x32.lib")
#endif



static inline uint8 uint8crop(int x)
{
	return (x<0) ? 0 : (x>255) ? 255 : 0;
}


#if SHOW_RAW_NEAR_AND_FAR
int far_value = 0;
int near_value = 0;
#endif


enum MovieMode
{
	MOVIE_READY,
	MOVIE_RECORD,
	MOVIE_PLAYBACK,
};

MovieMode movie_mode = MOVIE_READY;

#pragma warning(disable:4366)
#define GL_GENERATE_MIPMAP_SGIS 0x8191
#include "ST_Client.h"

#include "XnCppWrapper.h"
#include "zlibpp.h"

#if (ONI_PLATFORM == ONI_PLATFORM_MACOSX)
        #include <GLUT/glut.h>
#else
        #include <GL/glut.h>
#endif

#include "OniSampleUtilities.h"

const int GL_WIN_SIZE_X = 640;
const int GL_WIN_SIZE_Y	= 480;
const int TEXTURE_SIZE = 512;

const int MOVIE_MAX_SECS = 50;
const int MOVIE_FPS = 30;
const int MOVIE_MAX_FRAMES = MOVIE_MAX_SECS * MOVIE_FPS;
const int PIXELS_PER_SCREEN = 640*480;

struct Mode
{
	bool sync_enabled;
	bool mixed_enabled;
	bool zero255_show;
	bool alpha_mode;
	bool pixel_completion;
} mode;


GLuint graphic_tex;


using namespace mi;


//#define DEFAULT_DISPLAY_MODE	DISPLAY_MODE_OVERLAY
#define DEFAULT_DISPLAY_MODE	DISPLAY_MODE_DEPTH

#define MIN_NUM_CHUNKS(data_size, chunk_size)	((((data_size)-1) / (chunk_size) + 1))
#define MIN_CHUNKS_SIZE(data_size, chunk_size)	(MIN_NUM_CHUNKS(data_size, chunk_size) * (chunk_size))

SampleViewer* SampleViewer::ms_self = NULL;

void SampleViewer::glutIdle()
{
	glutPostRedisplay();
}
void SampleViewer::glutDisplay()
{
	SampleViewer::ms_self->display();
}
void SampleViewer::glutKeyboard(unsigned char key, int x, int y)
{
	SampleViewer::ms_self->onKey(key, x, y);
}
void SampleViewer::glutKeyboardSpecial(int key, int x, int y)
{
	SampleViewer::ms_self->onKey(key+1000, x, y);
}


#include <map>
#include <vector>

typedef std::vector<openni::RGB888Pixel> RgbScreen;
typedef std::map<int,RgbScreen> RgbScreenMovie;

std::vector<zlibpp::bytes> recorded_frames;

int recorded_tail = 0;
int movie_index = 0;

openni::RGB888Pixel* moviex = nullptr;



SampleViewer::SampleViewer(const char* strSampleName, openni::Device& device, openni::VideoStream& depth, openni::VideoStream& color) :
	m_device(device), m_depthStream(depth), m_colorStream(color), m_streams(NULL),
	m_eViewState(DEFAULT_DISPLAY_MODE),
	m_pTexMap(NULL),
	video_ram(NULL),
	audio(Audio::self())
{
	ms_self = this;
	strncpy(m_strSampleName, strSampleName, ONI_MAX_STR);
}
SampleViewer::~SampleViewer()
{
	delete[] m_pTexMap;
	delete[] video_ram;

	ms_self = NULL;

	if (m_streams != NULL)
	{
		delete []m_streams;
	}
}

AudioBuffer se0,se1;
freetype::font_data monospace, serif;


openni::Status SampleViewer::init(int argc, char **argv)
{
	openni::VideoMode depthVideoMode;
	openni::VideoMode colorVideoMode;

	if (m_depthStream.isValid() && m_colorStream.isValid())
	{
		depthVideoMode = m_depthStream.getVideoMode();
		colorVideoMode = m_colorStream.getVideoMode();

		int depthWidth = depthVideoMode.getResolutionX();
		int depthHeight = depthVideoMode.getResolutionY();
		int colorWidth = colorVideoMode.getResolutionX();
		int colorHeight = colorVideoMode.getResolutionY();

		if (depthWidth == colorWidth &&
			depthHeight == colorHeight)
		{
			m_width = depthWidth;
			m_height = depthHeight;
		}
		else
		{
			printf("Error - expect color and depth to be in same resolution: D: %dx%d, C: %dx%d\n",
				depthWidth, depthHeight,
				colorWidth, colorHeight);
			return openni::STATUS_ERROR;
		}
	}
	else if (m_depthStream.isValid())
	{
		depthVideoMode = m_depthStream.getVideoMode();
		m_width = depthVideoMode.getResolutionX();
		m_height = depthVideoMode.getResolutionY();
	}
	else if (m_colorStream.isValid())
	{
		colorVideoMode = m_colorStream.getVideoMode();
		m_width = colorVideoMode.getResolutionX();
		m_height = colorVideoMode.getResolutionY();
	}
	else
	{
		printf("Error - expects at least one of the streams to be valid...\n");
		return openni::STATUS_ERROR;
	}

	m_streams = new openni::VideoStream*[2];
	m_streams[0] = &m_depthStream;
	m_streams[1] = &m_colorStream;

	// Texture map init
	printf("%d x %d\n", m_width, m_height);
	m_nTexMapX = MIN_CHUNKS_SIZE(m_width, TEXTURE_SIZE);
	m_nTexMapY = MIN_CHUNKS_SIZE(m_height, TEXTURE_SIZE);
	m_pTexMap = new openni::RGB888Pixel[m_nTexMapX * m_nTexMapY];

	video_ram = new RGBA_raw[m_nTexMapX * m_nTexMapY];


	{
		puts("Init audio...");
		audio.init();
		audio.createChannels(1, 8);

		PCM pcm("C:/Users/STM/Desktop/wav/se_maoudamashii_onepoint30.wav");
		se0.attach(pcm);

		PCM pcm2("C:/Users/STM/Desktop/wav/se_maoudamashii_onepoint15.wav");
		se1.attach(pcm2);
		puts("Init audio...done!");
	}


	if (initOpenGL(argc, argv) != openni::STATUS_OK)
	{
		return openni::STATUS_ERROR;
	}

	// Init routine @init@
	{
		puts("Init font...");
		const std::string font_folder = "C:/Windows/Fonts/";
		monospace.init(font_folder + "Courbd.ttf", 16);
	//	serif    .init(font_folder + "verdana.ttf", 16);
		puts("Init font...done!");
	}

	return openni::STATUS_OK;
}

miImage eureka;
miImage clam;

openni::Status SampleViewer::run()	//Does not return
{
	glutMainLoop();

	return openni::STATUS_OK;
}


#if 0
void rounding(uint8* pixels)
{
	int rounded = 0;
	for (int y=0; y<480; ++y)
	{
		for (int x=1; x<639; ++x)
		{
			if (abs(pixels[x]-pixels[x-1])>100 &&
				abs(pixels[x]-pixels[x+1])>100)
			{
				pixels[x] = 0;
				++rounded;
			}
		}
	}
	printf("%d rounded\n", rounded);
}
#endif


#if 0
//  src: 640x480 uint8 depth
// dest: output byte stream
void encoding(uint8* src, std::vector<uint8>& byte_stream)
{
#if 0
	// TEST DATA
	int i=0;
	for (int y=0; y<480; ++y)
	{
		for (int x=0; x<640; ++x)
		{
			src[i++] = (x==30 && y>=50 && y<=100) ? 200 : 0;
		}
	}
#endif

	const int SIZE = 640*480;
	for (int i=0; i<SIZE;)
	{
		if (src[i]==0)
		{
			// Zero run-length
			int len = 1;
			for (; len<127; ++len)
			{
				if (i+len >= SIZE)
					break;
				if (src[i+len]!=0)
					break;
			}
			
			// 01: zero x1
			// 0F: zero x15
			// 7F: zero x127
			byte_stream.push_back(len);
			i += len;
		}
		else
		{
			// Zero run-length
			int len = 1;
			for (; len<127; ++len)
			{
				if (i+len >= SIZE)
					break;
				if (src[i+len]==0)
					break;
			}
			
			// 80: Non Zero x1
			// 8F: Non Zero x16
			// FF: Non Zero x128
			byte_stream.push_back((len-1) + 0x80);
			for (int j=0; j<len; ++j)
			{
				byte_stream.push_back(src[i+j]);
			}

			i += len;
		}
	}
}

void decoding(const std::vector<uint8>& byte_stream, uint8* dest)
{
	int read_index = 0;
	auto fetch = [&]()->int{
		if (read_index >= byte_stream.size())
			return 0;
		int retval = byte_stream[read_index++];
		return retval;
	};

	while (read_index < byte_stream.size())
	{
		int x = fetch();
		if (x<=127)
		{
			// Zero
			for (int i=1; i<=x; ++i)
			{
				*dest++ = 0;
			}
		}
		else
		{
			// Non-Zero
			x -= 0x80;
			for (int i=0; i<=x; ++i)
			{
				*dest++ = fetch();
			}
		}
	}
}
#endif



void drawBitmap(
		int dx, int dy, int dw, int dh,
		RGBA_raw* bitmap,
		int bw, int bh,
		float u1, float v1, float u2, float v2)
{
	glBindTexture(GL_TEXTURE_2D, graphic_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bw, bh, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap);

	const int x1 = dx;
	const int y1 = dy;
	const int x2 = dx + dw;
	const int y2 = dy + dh;

	glColor4f(1,1,1,1);
	glBegin(GL_QUADS);
	glTexCoord2f(u1, v1); glVertex2f(x1, y1);
	glTexCoord2f(u2, v1); glVertex2f(x2, y1);
	glTexCoord2f(u2, v2); glVertex2f(x2, y2);
	glTexCoord2f(u1, v2); glVertex2f(x1, y2);
	glEnd();
}


void SampleViewer::drawImageMode()
{
	using namespace openni;

	const RGB888Pixel* source_row = (const RGB888Pixel*)m_colorFrame.getData();
	RGB888Pixel* dest_row = m_pTexMap + m_colorFrame.getCropOriginY() * m_nTexMapX;
	const int source_rows = m_colorFrame.getStrideInBytes() / sizeof(RGB888Pixel);

	for (int y=0; y<m_colorFrame.getHeight(); ++y)
	{
		const RGB888Pixel* src = source_row;
		RGB888Pixel* dest = dest_row + m_colorFrame.getCropOriginX();

		for (int x=0; x<m_colorFrame.getWidth(); ++x)
		{
			*dest++ = *src++;
		}

		dest_row   += m_nTexMapX;
		source_row += source_rows;
	}

#if 0
	drawBitmap(
		0, 0, GL_WIN_SIZE_X, GL_WIN_SIZE_Y,
		m_pTexMap,
		m_nTexMapX, m_nTexMapY,
		0.0f,
		0.0f,
		(float)m_width  / m_nTexMapX,
		(float)m_height / m_nTexMapY);
#endif
}

void SampleViewer::drawDepthMode()
{
	using namespace openni;

	const int WORK = 4;
	static uint8
		curr_pre[640*480],
		mixed[640*480],
		work[WORK][640*480];
	static int
		work_total[640*480];
	static int
		work_index = 0;

	uint8* const curr = work[work_index];
	work_index = (work_index+1) % WORK;

	// Depth raw => Current pre buffer
	{
		const auto* depth_row = (const DepthPixel*)m_depthFrame.getData();
		const int rowsize = m_depthFrame.getStrideInBytes() / sizeof(DepthPixel);
		uint8* dest = curr_pre;
		for (int y=0; y<480; ++y)
		{
			const auto* src = depth_row;

			for (int x=0; x<640; ++x)
			{
				int depth = *src++;
#if SHOW_RAW_NEAR_AND_FAR
				far_value  = max(far_value, depth);
				near_value = (near_value > depth && depth!=0) ? depth : near_value;
#endif

				if (depth==0)
				{
					// invalid data (too near, too far)
					*dest++ = 0;
				}
				else
				{
					depth = depth*255/3000;
					if (depth>255)
					{
						// too far
						*dest++ = 0;
					}
					else
					{
						if (depth>255) depth=255;
						if (depth<1) depth=1;
						*dest++ = 255-depth;
					}
				}
			}

			depth_row += rowsize;
		}
	}


	{
		const uint8* src = curr_pre;
		uint8* dest = curr;
		for (int y=0; y<480; ++y)
		{
			for (int x=0; x<640; ++x)
			{
				uint8 depth = *src++;

				if (depth>=100 && depth<=200 && depth/2%2==0)
				//if (depth==200)
				{
					depth = 20;
				}


				*dest++ = depth;
			}
		}
	}



	// Curr+Back => Mixed
	if (mode.mixed_enabled)
	{
		for (int i=0; i<640*480; ++i)
		{
			work_total[i] = 0;
		}
		for (int j=0; j<WORK; ++j)
		{
			for (int i=0; i<640*480; ++i)
			{
				if (work[j][i]!=255)
				{
					work_total[i] += work[j][i];
				}
			}
		}

		// Total => Mixed
		for (int i=0; i<640*480; ++i)
		{
			mixed[i] = work_total[i] / WORK;
		}
	}

	switch (movie_mode)
	{
	case MOVIE_RECORD:
		if (recorded_tail >= recorded_frames.size())
		{
			puts("time over! record stop.");
			movie_mode = MOVIE_READY;
		}
		else
		{
			zlibpp::bytes& byte_stream = recorded_frames[recorded_tail++];
			zlibpp::compress(curr, 640*480, byte_stream, 2);
			printf("%d bytes (%.1f%%)\n",
				byte_stream.size(),
				byte_stream.size() * 100.0 / (640*480));
		}
		break;
	case MOVIE_PLAYBACK:
		if (movie_index >= recorded_tail)
		{
			puts("movie is end. stop.");
			movie_mode = MOVIE_READY;
		}
		else
		{
			zlibpp::bytes& byte_stream = recorded_frames[movie_index++];
			zlibpp::bytes outdata;
			zlibpp::decompress(byte_stream, outdata);
			for (int i=0; i<640*480; ++i)
			{
				curr[i] = (curr[i] + outdata[i])>>1;
			}
		}
		break;
	}


	// Mixed to Texture
	{
		auto* src = mode.mixed_enabled ? mixed : curr;

		for (int y=0; y<480; ++y)
		{
			RGBA_raw* dest = video_ram + y*m_nTexMapX;

			for (int x=0; x<640; ++x, ++dest, ++src)
			{
#if 1
				switch (*src)
				{
				case 0:
					dest->r = 40;
					dest->g = 80;
					dest->b = 110;
					dest->a = 200;
					break;
				case 20:
					dest->r = 40;
					dest->g = 80;
					dest->b = 110;
					dest->a = 10;
					break;
				default:
					dest->r = *src;
					dest->g = *src;
					dest->b = *src;
					dest->a = 220;
					break;
				}
#elif 0
				dest->r = (x*255/640);
				dest->g = (y*255/640);
				dest->b = (255-x*255/640);
				dest->a = (*src>=1 && *src<255) ? (mode.alpha_mode ? *src : 255) : 0;
#elif 0
				dest->r = uint8crop(x*255/640 + *src);
				dest->g = (y*255/640);
				dest->b = (255-x*255/640);
				dest->a = 100;
#else
				if (mode.zero255_show)
				{
					if (*src==0)
					{
						pTex->r = 20; //BLUE
						pTex->g = 50;
						pTex->b = 90;
						continue;
					}
					else if (*src==255)
					{
						pTex->r = 80; //RED
						pTex->g = 20;
						pTex->b = 20;
						continue;
					}
				}
				else
				{
					if (*src==0 || *src==255)
					{
						pTex->r = 0;
						pTex->g = 0;
						pTex->b = 0;
						continue;
					}
				}

				int lum = *src;
				pTex->r = lum;
				pTex->g = 255-lum;
				pTex->b = lum;
#endif
			}
		}
	}

	// @draw
	drawBitmap(
		0, 0, GL_WIN_SIZE_X, GL_WIN_SIZE_Y,
		video_ram,
		m_nTexMapX, m_nTexMapY,
		0.0f,
		0.0f,
		(float)m_width  / m_nTexMapX,
		(float)m_height / m_nTexMapY);
}

void SampleViewer::display()
{
	//‚¤‚²‚©‚È‚¢?
	m_device.setDepthColorSyncEnabled(mode.sync_enabled);
	//END

	int changedIndex;
	openni::Status rc = openni::OpenNI::waitForAnyStream(m_streams, 2, &changedIndex);
	if (rc != openni::STATUS_OK)
	{
		printf("Wait failed\n");
		return;
	}

	switch (changedIndex)
	{
	case 0:
		m_depthStream.readFrame(&m_depthFrame);
		break;
	case 1:
		m_colorStream.readFrame(&m_colorFrame);
		break;
	default:
		printf("Error in wait\n");
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, GL_WIN_SIZE_X, GL_WIN_SIZE_Y, 0, -1.0, 1.0);

	if (m_depthFrame.isValid())
	{
	//	calculateHistogram(m_pDepthHist, MAX_DEPTH, m_depthFrame);
	}

	memset(m_pTexMap, 0, m_nTexMapX*m_nTexMapY*sizeof(openni::RGB888Pixel));

	eureka.draw(0,0, 640,480, 255);

#if SHOW_RAW_NEAR_AND_FAR
	near_value = 10000;
	far_value = 0;
#endif
	switch (m_eViewState)
	{
	case DISPLAY_MODE_IMAGE:
		if (m_colorFrame.isValid())
			drawImageMode();
		break;
	case DISPLAY_MODE_DEPTH:
		if (m_depthFrame.isValid())
			drawDepthMode();
		break;
	}


	static float r;
	r += 0.7;

	clam.drawRotated(170,70, 80,150, r, 80);


	// RED TEXT
	{
		static uint time_begin;
		if (time_begin==0)
		{
			time_begin = timeGetTime();
		}

		static float cnt = 0;
		static int frames = 0;
		++frames;
		cnt += 0.01;
		glColor3ub(255, 255, 255);
		glPushMatrix();
		glLoadIdentity();
		glRotatef(12*cnt, 0, 0, 1);
		glScalef(1.2f, 1.2f+0.3f*cosf(cnt/5), 1.f);
		glTranslatef(-180.f, 0.f, 0.f);
		int time_diff = (timeGetTime() - time_begin);
	// @fps
		freetype::print(monospace, 20, 240, "%d, %d, %.1ffps",
				frames,
				time_diff,
				1000.0f * frames/time_diff);
		glPopMatrix();
	}

#if SHOW_RAW_NEAR_AND_FAR
	{
		// @far
		glColor3ub(255, 255, 255);
		glPushMatrix();
		glLoadIdentity();
		freetype::print(monospace, 20, 280, "near-far : %5d-%5d",
			near_value,	
			far_value);
		glPopMatrix();
	}
#endif



	// Swap the OpenGL display buffers
	glutSwapBuffers();
}

void toggle(bool& ref)
{
	auto log = [&](bool state, int key, const char* text){
		printf("%12s (%c): %s\n",
			text,
			key,
			state ? "YES" : "NO");
	};

	ref = !ref;
	puts("-----------------------------");
	log(mode.sync_enabled,     'k', "sync");
	log(mode.mixed_enabled,    'm', "mixed");
	log(mode.zero255_show,     'z', "zero255");
	log(mode.alpha_mode,       'a', "alpha");
	log(mode.pixel_completion, 'e', "pixel completion");
	puts("-----------------------------");
}

struct FileHeader
{
	unsigned __int8
		signature[4],  // "stm "
		compress[4],   // "zip "
		graphic[4];    // "dpth"
	int total_frames;
};

void saveToFile(FILE* fp, const std::vector<zlibpp::bytes>& recorded_frames)
{
	FileHeader header;

	header.signature[0] = 's';
	header.signature[1] = 't';
	header.signature[2] = 'm';
	header.signature[3] = ' ';
	header.compress[0] = 'z';
	header.compress[1] = 'i';
	header.compress[2] = 'p';
	header.compress[3] = ' ';
	header.graphic[0] = 'd';
	header.graphic[1] = 'p';
	header.graphic[2] = 't';
	header.graphic[3] = 'h';
	header.total_frames = recorded_tail;

	fwrite(&header, sizeof(header), 1, fp);
	for (int i=0; i<recorded_tail; ++i)
	{
		const auto& frame = recorded_frames[i];
		uint32 frame_size = (uint32)frame.size();
		fwrite(&frame_size, sizeof(frame_size), 1, fp);
		fwrite(frame.data(), frame_size, 1, fp);
	}

	// for human
	fputs("//END", fp);
}

bool checkMagic(const unsigned __int8* data, const char* str)
{
	return
		data[0]==str[0] &&
		data[1]==str[1] &&
		data[2]==str[2] &&
		data[3]==str[3];
}

bool loadFromFile(FILE* fp, std::vector<zlibpp::bytes>& recorded_frames)
{
	FileHeader header;
	fread(&header, sizeof(header), 1, fp);

	if (!checkMagic(header.signature, "stm "))
	{
		fprintf(stderr, "File is not STM format.\n");
		return false;
	}
	if (!checkMagic(header.compress, "zip "))
	{
		fprintf(stderr, "Unsupport compress format.\n");
		return false;
	}
	if (!checkMagic(header.graphic, "dpth"))
	{
		fprintf(stderr, "Unsupport graphic format.\n");
		return false;
	}
	if (header.total_frames<=0 || header.total_frames>=30*60*5)
	{
		fprintf(stderr, "Invalid total frames.\n");
		return false;
	}

	recorded_tail = header.total_frames;
	printf("Total %d frames\n", header.total_frames);
	
	recorded_frames.clear();
	recorded_frames.resize(header.total_frames);
	for (int i=0; i<header.total_frames; ++i)
	{
		auto& frame = recorded_frames[i];
		unsigned __int32 frame_size;
		fread(&frame_size, sizeof(frame_size), 1, fp);

		frame.resize(frame_size);
		fread(frame.data(), frame_size, 1, fp);
	}

	return true;
}


enum
{
	KEY_F1 = 1001,
	KEY_F2 = 1002,
	KEY_F3 = 1003,
	KEY_F4 = 1004,
	KEY_F5 = 1005,
	KEY_F6 = 1006,
	KEY_F7 = 1007,
	KEY_F8 = 1008,
	KEY_F9 = 1009,
	KEY_F10 = 1010,
	KEY_F11 = 1011,
	KEY_F12 = 1012,
};

void saveAgent(int slot)
{
	printf("Save to file slot %d...", slot);

	char buf[100];
	sprintf_s(buf, "file%d", slot);
	FILE* fp = fopen(buf, "wb");
	if (fp==nullptr)
	{
		puts("invalid!!");
	}
	else
	{
		saveToFile(fp, recorded_frames);
		fclose(fp);
		puts("done!");
	}	
}

void loadAgent(int slot)
{
	printf("Load from file slot %d...", slot);
	char buf[100];
	sprintf_s(buf, "file%d", slot);
	FILE* fp = fopen(buf, "rb");
	if (fp==nullptr)
	{
		puts("file not found!");
	}
	else
	{
		if (loadFromFile(fp, recorded_frames))
		{
			fclose(fp);
			puts("done!");
			movie_mode = MOVIE_PLAYBACK;
			movie_index = 0;
		}
		else
		{
			puts("load error!");
		}
	}
}

void SampleViewer::onKey(int key, int /*x*/, int /*y*/)
{
	switch (key)
	{
	default:
		printf("[key %d]\n", key);
		break;
	case 27:
		m_depthStream.stop();
		m_colorStream.stop();
		m_depthStream.destroy();
		m_colorStream.destroy();
		m_device.close();
		openni::OpenNI::shutdown();
		exit(1);
	case '1':
		m_eViewState = DISPLAY_MODE_OVERLAY;
		m_device.setImageRegistrationMode(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR);
		break;
	case '2':
		m_eViewState = DISPLAY_MODE_DEPTH;
		m_device.setImageRegistrationMode(openni::IMAGE_REGISTRATION_OFF);
		break;
	case '3':
		m_eViewState = DISPLAY_MODE_IMAGE;
		m_device.setImageRegistrationMode(openni::IMAGE_REGISTRATION_OFF);
		break;
	case 'M':
		m_depthStream.setMirroringEnabled(!m_depthStream.getMirroringEnabled());
		m_colorStream.setMirroringEnabled(!m_colorStream.getMirroringEnabled());
		break;
	case 'r':
		recorded_frames.clear();
		recorded_frames.resize(MOVIE_MAX_FRAMES);
		recorded_tail = 0;
		movie_mode = MOVIE_RECORD;
		movie_index = 0;
		break;
	case 's':
		printf("recoding stop. %d frames recorded.\n", recorded_tail);
		{
			size_t total_bytes = 0;
			for (int i=0; i<recorded_tail; ++i)
			{
				total_bytes += recorded_frames[i].size();
			}
			printf("total %u Kbytes.\n", total_bytes/1000);
		}
		movie_mode = MOVIE_READY;
		movie_index = 0;
		break;
	case KEY_F1: saveAgent(1); break;
	case KEY_F2: saveAgent(2); break;
	case KEY_F3: saveAgent(3); break;
	case KEY_F4: saveAgent(4); break;
	case KEY_F5: saveAgent(5); break;
	case KEY_F6: loadAgent(1); break;
	case KEY_F7: loadAgent(2); break;
	case KEY_F8: loadAgent(3); break;
	case KEY_F9: loadAgent(4); break;
	case KEY_F10: loadAgent(5); break;
	case 'p':
		printf("playback movie.\n");
		movie_mode = MOVIE_PLAYBACK;
		movie_index = 0;
		break;
	case 'k':  toggle(mode.sync_enabled);  break;
	case 'm':  toggle(mode.mixed_enabled); break;
	case 'z':  toggle(mode.zero255_show);  break;
	case 'a':  toggle(mode.alpha_mode);    break;
	case 'e':  toggle(mode.pixel_completion);    break;

	case 'x':
		puts("play sound 1");
		audio.setMasterPitch(1.0);
		audio.se.play(se0);
		break;
	case 'c':
		puts("play sound 2");
		audio.setMasterPitch(1.0);
		audio.se.play(se1);
		break;
	case 'v':
		puts("play sound 2");
		audio.setMasterPitch(0.2);
		audio.se.play(se0);
		break;
	}
}

openni::Status SampleViewer::initOpenGL(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(GL_WIN_SIZE_X, GL_WIN_SIZE_Y);
	glutCreateWindow (m_strSampleName);
	// 	glutFullScreen();
	glutSetCursor(GLUT_CURSOR_NONE);

	initOpenGLHooks();

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	glGenTextures(1, &graphic_tex);
	glBindTexture(GL_TEXTURE_2D , graphic_tex);


	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


	eureka.createFromImageA("C:/Users/STM/Desktop/eureka.jpg");
	printf("%d\n", eureka.getTexture());

	clam.createFromImageA("C:/Users/STM/Desktop/clam.jpg");
	printf("%d\n", clam.getTexture());


	return openni::STATUS_OK;
}
void SampleViewer::initOpenGLHooks()
{
	glutKeyboardFunc(glutKeyboard);
	glutDisplayFunc(glutDisplay);
	glutIdleFunc(glutIdle);
	glutSpecialFunc(glutKeyboardSpecial);
}
