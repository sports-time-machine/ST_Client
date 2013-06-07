#pragma once
#include <OpenNI.h>
#include "miCore.h"

#define WITHOUT_KINECT 0


#define MIN_NUM_CHUNKS(data_size, chunk_size)	((((data_size)-1) / (chunk_size) + 1))
#define MIN_CHUNKS_SIZE(data_size, chunk_size)	(MIN_NUM_CHUNKS(data_size, chunk_size) * (chunk_size))


struct Mode
{
	bool show_hit_boxes;
	bool sync_enabled;
	bool mixed_enabled;
	bool zero255_show;
	bool alpha_mode;
	bool pixel_completion;
	bool mirroring;
	bool borderline;
	bool calibration;
	bool view4test;
};


struct RgbaTex
{
	RGBA_raw* vram;
	uint tex;
	uint width;
	uint height;
	uint ram_width, ram_height;
	uint pitch;     // f(2^X, width)

	RgbaTex();
	virtual ~RgbaTex();

	void create(int w, int h);
};



#define MAX_DEPTH 10000

enum DisplayModes
{
	DISPLAY_MODE_OVERLAY,
	DISPLAY_MODE_DEPTH,
	DISPLAY_MODE_IMAGE
};

class StClient
{
public:
	StClient(openni::Device& device, openni::VideoStream& depth, openni::VideoStream& color);
	virtual ~StClient();

	virtual bool init(int argc, char **argv);
	virtual bool run();	//Does not return

protected:
	virtual void display();
	virtual void displayPostDraw(){};	// Overload to draw over the screen image

	virtual void onKey(int key, int x, int y);
	virtual void onMouse(int button, int state, int x, int y);

	bool initOpenGL(int argc, char **argv);

	openni::VideoFrameRef m_depthFrame;
	openni::VideoFrameRef m_colorFrame;

	openni::Device&       m_device;
	openni::VideoStream&  m_depthStream;
	openni::VideoStream&  m_colorStream;
	openni::VideoStream** m_streams;

private:
	void displayDepthScreen();
	void displayBlackScreen();
	void displayPictureScreen();

	bool doCommand();
	bool doCommand2(const std::string& line);

private:
	StClient(const StClient&);
	StClient& operator=(StClient&);

	void drawImageMode();
	void drawDepthMode();
	void BuildDepthImage(uint8* dest);

	static StClient* ms_self;
	static void glutIdle();
	static void glutDisplay();
	static void glutKeyboard(unsigned char key, int x, int y);
	static void glutKeyboardSpecial(int key, int x, int y);
	static void glutMouse(int button, int state, int x, int y);
	static void glutReshape(int width, int height);

	float			m_pDepthHist[MAX_DEPTH];
	unsigned int		m_nTexMapX;
	unsigned int		m_nTexMapY;
	DisplayModes		m_eViewState;

	RGBA_raw* video_ram;
	uint vram_tex;
	uint vram_floor;

	RGBA_raw* video_ram2;
	uint vram_tex2;

	int			m_width;
	int			m_height;

	struct RawDepthImage
	{
		std::vector<uint16> image;
		uint16 min_value;
		uint16 max_value;
		uint16 range;        // max_value - min_value

		RawDepthImage()
		{
			image.resize(640*480);
			min_value = 0;
			max_value = 0;
			range = 0;
		}
	};


	RawDepthImage raw_depth;
	RawDepthImage raw_floor;
	RawDepthImage raw_cooked;
	RawDepthImage raw_transformed;

	RgbaTex  img_rawdepth;
	RgbaTex  img_floor;
	RgbaTex  img_cooked;
	RgbaTex  img_transformed;


	void CreateCoockedDepth(RawDepthImage& raw_cooked, const RawDepthImage& raw_depth, const RawDepthImage& raw_floor);
	void CalcDepthMinMax(RawDepthImage& raw);
	void RawDepthImageToRgbaTex(const RawDepthImage& raw, RgbaTex& dest);
	void CreateTransformed(RawDepthImage& raw_transformed, const RawDepthImage& raw_cooked);


	void CreateRawDepthImage(RawDepthImage& raw);
	void drawPlaybackMovie();
	void displayCalibrationInfo();
	void saveFloorDepth();
};


extern Mode mode;
