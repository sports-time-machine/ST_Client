#pragma once
#include <OpenNI.h>
#include "miCore.h"

#define WITHOUT_KINECT 0


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

	RGBA_raw* video_ram2;
	uint vram_tex2;

	int			m_width;
	int			m_height;


	void displayCalibrationInfo();
};
