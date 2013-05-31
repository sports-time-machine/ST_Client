#pragma once
#include <OpenNI.h>
#include "miCore.h"
#include "miAudio.h"

#define MAX_DEPTH 10000

enum DisplayModes
{
	DISPLAY_MODE_OVERLAY,
	DISPLAY_MODE_DEPTH,
	DISPLAY_MODE_IMAGE
};

class SampleViewer
{
public:
	SampleViewer(const char* strSampleName, openni::Device& device, openni::VideoStream& depth, openni::VideoStream& color);
	virtual ~SampleViewer();

	virtual openni::Status init(int argc, char **argv);
	virtual openni::Status run();	//Does not return

protected:
	virtual void display();
	virtual void displayPostDraw(){};	// Overload to draw over the screen image

	virtual void onKey(int key, int x, int y);

	virtual openni::Status initOpenGL(int argc, char **argv);
	void initOpenGLHooks();

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

	void doCommand();

private:
	SampleViewer(const SampleViewer&);
	SampleViewer& operator=(SampleViewer&);

	void drawImageMode();
	void drawDepthMode();

	static SampleViewer* ms_self;
	static void glutIdle();
	static void glutDisplay();
	static void glutKeyboard(unsigned char key, int x, int y);
	static void glutKeyboardSpecial(int key, int x, int y);

	float			m_pDepthHist[MAX_DEPTH];
	char			m_strSampleName[ONI_MAX_STR];
	unsigned int		m_nTexMapX;
	unsigned int		m_nTexMapY;
	DisplayModes		m_eViewState;

	RGBA_raw* video_ram;
	uint vram_tex;

	RGBA_raw* video_ram2;
	uint vram_tex2;

	int			m_width;
	int			m_height;

	mi::Audio& audio;
};
